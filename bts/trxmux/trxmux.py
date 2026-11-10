#!/usr/bin/env python3
"""Connect many emulators to one osmo-bts-trx.

Towards osmo-bts-trx this is the TRX: it owns the CLOCK, TRXC and TRXD ports,
answers the control commands and sends clock indications from the host clock.
Emulators connect to the client port and exchange single UDP datagrams whose
first byte is the message type:

  H  client -> mux  register or keep alive, no payload
  C  mux -> client  frame number (u32), downlink and uplink kHz (u32 each),
                    TSC (u8), powered (u8); all big-endian
  D  mux -> client  TRXD v0 downlink PDU as sent by the BTS
  U  client -> mux  TRXD v0 uplink PDU with the frame number of the BTS
"""

import argparse
import asyncio
import socket
import struct
import time

HYPERFRAME_FRAMES = 2715648
FRAME_NUMERATOR_NS = 60_000_000
FRAME_DENOMINATOR = 13
CLOCK_INDICATION_FRAMES = 216
BURST_BITS = 148
DOWNLINK_PDU_SIZE = 6 + BURST_BITS
UPLINK_PDU_SIZE = 8 + BURST_BITS + 2
CLIENT_TIMEOUT_S = 5.0
TRXD_VERSION = "0"
NOMINAL_TX_POWER_DBM = "23"
BSIC_BCC_MASK = 0x07
BTS_PORT_OFFSET = 100

CLOCK, CONTROL, DATA = range(3)


class TrxMux:
    def __init__(self, address, base_port, client_port):
        self.loop = asyncio.get_running_loop()
        self.start_ns = time.monotonic_ns()
        self.powered = False
        self.downlink_khz = 0
        self.uplink_khz = 0
        self.tsc = 0
        self.clients = {}
        self.bts = [self.open_socket(address, base_port + i, base_port + BTS_PORT_OFFSET + i) for i in range(3)]
        self.client_socket = self.open_socket(address, client_port, None)
        self.loop.add_reader(self.bts[CONTROL], self.read_control)
        self.loop.add_reader(self.bts[DATA], self.read_downlink)
        self.loop.add_reader(self.client_socket, self.read_client)
        self.schedule_clock(0)

    @staticmethod
    def open_socket(address, port, remote_port):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((address, port))
        if remote_port is not None:
            sock.connect((address, remote_port))
        sock.setblocking(False)
        return sock

    def frame_number(self):
        elapsed_ns = time.monotonic_ns() - self.start_ns
        return elapsed_ns * FRAME_DENOMINATOR // FRAME_NUMERATOR_NS % HYPERFRAME_FRAMES

    def schedule_clock(self, frames):
        frame_ns = FRAME_NUMERATOR_NS / FRAME_DENOMINATOR
        delay_s = frames * frame_ns / 1e9
        self.loop.call_later(delay_s, self.tick)

    def tick(self):
        if self.powered:
            self.send(self.bts[CLOCK], f"IND CLOCK {self.frame_number()}".encode() + b"\0")
        self.expire_clients()
        self.schedule_clock(CLOCK_INDICATION_FRAMES)

    def send(self, sock, data, address=None):
        try:
            if address is None:
                sock.send(data)
            else:
                sock.sendto(data, address)
        except OSError as error:
            print(f"send failed: {error}", flush=True)

    def send_config(self, client, fn):
        message = b"C" + struct.pack(">IIIBB", fn, self.downlink_khz, self.uplink_khz, self.tsc, self.powered)
        self.send(self.client_socket, message, client)

    def expire_clients(self):
        now = time.monotonic()
        for client, last_seen in list(self.clients.items()):
            if now - last_seen > CLIENT_TIMEOUT_S:
                del self.clients[client]
                print(f"client {client[0]}:{client[1]} timed out", flush=True)

    def read_control(self):
        message = self.bts[CONTROL].recv(256).rstrip(b"\0").decode(errors="replace")
        if not message.startswith("CMD "):
            return

        name, _, params = message[4:].partition(" ")
        reply_params = params
        if name == "POWERON":
            self.powered = True
            self.send(self.bts[CLOCK], f"IND CLOCK {self.frame_number()}".encode() + b"\0")
        elif name == "POWEROFF":
            self.powered = False
        elif name == "TXTUNE":
            self.downlink_khz = int(params)
        elif name == "RXTUNE":
            self.uplink_khz = int(params)
        elif name in ("SETTSC", "SETBSIC"):
            self.tsc = int(params) & BSIC_BCC_MASK
        elif name == "SETFORMAT":
            reply_params = TRXD_VERSION
        elif name == "NOMTXPOWER":
            reply_params = NOMINAL_TX_POWER_DBM

        reply = f"RSP {name} 0 {reply_params}" if reply_params else f"RSP {name} 0"
        self.send(self.bts[CONTROL], reply.encode() + b"\0")
        if name in ("POWERON", "POWEROFF", "TXTUNE", "RXTUNE", "SETTSC", "SETBSIC"):
            fn = self.frame_number()
            for client in self.clients:
                self.send_config(client, fn)

    def read_downlink(self):
        pdu = self.bts[DATA].recv(DOWNLINK_PDU_SIZE + 1)
        if len(pdu) != DOWNLINK_PDU_SIZE or pdu[0] >> 4 != 0:
            return
        for client in self.clients:
            self.send(self.client_socket, b"D" + pdu, client)

    def read_client(self):
        message, client = self.client_socket.recvfrom(UPLINK_PDU_SIZE + 1)
        if message[:1] == b"H":
            if client not in self.clients:
                print(f"client {client[0]}:{client[1]} connected", flush=True)
            self.clients[client] = time.monotonic()
            self.send_config(client, self.frame_number())
        elif message[:1] == b"U" and len(message) == UPLINK_PDU_SIZE + 1 and client in self.clients:
            self.send(self.bts[DATA], message[1:])


async def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--address", default="127.0.0.1")
    parser.add_argument("--base-port", type=int, default=5700)
    parser.add_argument("--client-port", type=int, default=5750)
    args = parser.parse_args()

    TrxMux(args.address, args.base_port, args.client_port)
    print(f"trxmux: BTS on {args.address}:{args.base_port}, clients on {args.address}:{args.client_port}", flush=True)
    await asyncio.Event().wait()


if __name__ == "__main__":
    asyncio.run(main())
