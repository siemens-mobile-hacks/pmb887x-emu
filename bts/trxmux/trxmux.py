#!/usr/bin/env python3
"""Give every emulator its own cell of the shared network.

Each cell is an osmo-bts-trx instance; towards it this is the TRX: it owns the
CLOCK, TRXC and TRXD ports of the cell and answers the control commands. An
emulator that connects to the client port is assigned a free cell for as long
as it keeps sending hellos. The cell runs on the virtual time of its emulator:
clock indications follow the frames the emulator reports complete, and between
reports the clock advances with the host clock. Like a real transceiver the
cell reports every timeslot of every uplink frame: a timeslot the emulator did
not transmit on gets a NOPE indication, because osmo-bts and osmo-pcu derive
their uplink timing from that continuous stream.

Emulators exchange single UDP datagrams whose first byte is the message type:

  H  client -> mux  register or keep alive, no payload
  C  mux -> client  frame number (u32), downlink and uplink kHz (u32 each),
                    TSC (u8), powered (u8); all big-endian
  D  mux -> client  TRXD v1 downlink PDU as sent by the BTS
  U  client -> mux  TRXD v1 uplink PDU with the frame number of the BTS
  F  client -> mux  frame number of the BTS (u32, big-endian): the client sent
                    every uplink burst of this frame and the frames before
"""

import argparse
import asyncio
import os
import socket
import struct
import time

HYPERFRAME_FRAMES = 2715648
FRAME_NUMERATOR_NS = 60_000_000
FRAME_DENOMINATOR = 13
FRAME_NS = FRAME_NUMERATOR_NS / FRAME_DENOMINATOR
CELL_PORT_STRIDE = 10
BTS_PORT_OFFSET = 100
CLOCK_INDICATION_FRAMES = 26
TICK_FRAMES = 4
# The emulator transmits a frame about two frames after it reports the previous one complete.
CLOCK_LEAD_FRAMES = 2
BURST_BITS = 148
DOWNLINK_PDU_SIZE = 6 + BURST_BITS
UPLINK_HEADER_SIZE = 11
UPLINK_PDU_SIZE = UPLINK_HEADER_SIZE + BURST_BITS
CLIENT_TIMEOUT_S = 5.0
TIMESLOTS = 8
MTS_NOPE = 0x80
TRXD_VERSION = 1
NOMINAL_TX_POWER_DBM = "23"
BSIC_BCC_MASK = 0x07

CLOCK, CONTROL, DATA = range(3)


class Cell:
    def __init__(self, mux, index, address, base_port):
        self.mux = mux
        self.index = index
        self.powered = False
        self.downlink_khz = 0
        self.uplink_khz = 0
        self.tsc = 0
        self.client = None
        self.complete_fn = None
        self.anchor_fn = 0
        self.anchor_ns = time.monotonic_ns()
        self.clock_fn = None
        self.uplink_fn = None
        self.uplink_received = set()
        port = base_port + index * CELL_PORT_STRIDE
        self.sockets = [open_socket(address, port + i, port + BTS_PORT_OFFSET + i) for i in range(3)]
        mux.loop.add_reader(self.sockets[CONTROL], self.read_control)
        mux.loop.add_reader(self.sockets[DATA], self.read_downlink)

    def frame_number(self):
        elapsed = int((time.monotonic_ns() - self.anchor_ns) / FRAME_NS)
        return (self.anchor_fn + elapsed) % HYPERFRAME_FRAMES

    def send_clock(self):
        self.clock_fn = self.frame_number()
        send(self.sockets[CLOCK], f"IND CLOCK {self.clock_fn}".encode() + b"\0")

    def tick(self):
        if not self.powered:
            self.uplink_fn = None
            return
        fn = self.frame_number()
        if self.clock_fn is None or (is_after(fn, self.clock_fn) and
                                     frames_between(self.clock_fn, fn) >= CLOCK_INDICATION_FRAMES):
            self.send_clock()
        self.complete_uplink()

    def uplink_frontier(self):
        if self.complete_fn is not None:
            return self.complete_fn
        return (self.frame_number() - CLOCK_LEAD_FRAMES) % HYPERFRAME_FRAMES

    def complete_uplink(self):
        frontier = self.uplink_frontier()
        if self.uplink_fn is None:
            self.uplink_fn = frontier
            return

        while is_after(frontier, self.uplink_fn):
            self.uplink_fn = (self.uplink_fn + 1) % HYPERFRAME_FRAMES
            for tn in range(TIMESLOTS):
                if (self.uplink_fn, tn) not in self.uplink_received:
                    send(self.sockets[DATA], nope_indication(tn, self.uplink_fn))
        self.uplink_received = {burst for burst in self.uplink_received if is_after(burst[0], self.uplink_fn)}

    def attach(self, client):
        self.client = client
        self.complete_fn = None
        print(f"client {client[0]}:{client[1]} attached to cell {self.index}", flush=True)

    def detach(self):
        print(f"client {self.client[0]}:{self.client[1]} left cell {self.index}", flush=True)
        self.client = None
        self.complete_fn = None

    def send_config(self):
        message = b"C" + struct.pack(">IIIBB", self.frame_number(), self.downlink_khz, self.uplink_khz, self.tsc,
                                     self.powered)
        send(self.mux.client_socket, message, self.client)

    def receive_uplink(self, pdu):
        tn = pdu[0] & 0x07
        fn = struct.unpack(">I", pdu[1:5])[0]
        if self.uplink_fn is not None and not is_after(fn, self.uplink_fn):
            print(f"cell {self.index}: late uplink burst fn={fn} tn={tn}", flush=True)
            return
        self.uplink_received.add((fn, tn))
        send(self.sockets[DATA], pdu)

    def receive_complete(self, fn):
        if self.complete_fn is not None and not is_after(fn, self.complete_fn):
            return
        self.complete_fn = fn
        self.anchor_fn = (fn + CLOCK_LEAD_FRAMES) % HYPERFRAME_FRAMES
        self.anchor_ns = time.monotonic_ns()
        self.complete_uplink()

    def read_control(self):
        message = self.sockets[CONTROL].recv(256).rstrip(b"\0").decode(errors="replace")
        if not message.startswith("CMD "):
            return

        name, _, params = message[4:].partition(" ")
        status = 0
        reply_params = params
        if name == "POWERON":
            self.powered = True
            self.send_clock()
        elif name == "POWEROFF":
            self.powered = False
        elif name == "TXTUNE":
            self.downlink_khz = int(params)
        elif name == "RXTUNE":
            self.uplink_khz = int(params)
        elif name in ("SETTSC", "SETBSIC"):
            self.tsc = int(params) & BSIC_BCC_MASK
        elif name == "SETFORMAT":
            status = TRXD_VERSION if int(params) >= TRXD_VERSION else -1
        elif name == "NOMTXPOWER":
            reply_params = NOMINAL_TX_POWER_DBM

        reply = f"RSP {name} {status} {reply_params}" if reply_params else f"RSP {name} {status}"
        send(self.sockets[CONTROL], reply.encode() + b"\0")
        if self.client and name in ("POWERON", "POWEROFF", "TXTUNE", "RXTUNE", "SETTSC", "SETBSIC"):
            self.send_config()

    def read_downlink(self):
        pdu = self.sockets[DATA].recv(DOWNLINK_PDU_SIZE + 1)
        if self.client and len(pdu) == DOWNLINK_PDU_SIZE and pdu[0] >> 4 == TRXD_VERSION:
            send(self.mux.client_socket, b"D" + pdu, self.client)


class TrxMux:
    def __init__(self, address, base_port, client_address, client_port, cells):
        self.loop = asyncio.get_running_loop()
        self.client_socket = open_socket(client_address, client_port, None)
        self.cells = [Cell(self, index, address, base_port) for index in range(cells)]
        self.clients = {}
        self.loop.add_reader(self.client_socket, self.read_client)
        self.tick()

    def tick(self):
        self.expire_clients()
        for cell in self.cells:
            cell.tick()
        self.loop.call_later(TICK_FRAMES * FRAME_NS / 1e9, self.tick)

    def expire_clients(self):
        now = time.monotonic()
        for client, (cell, last_seen) in list(self.clients.items()):
            if now - last_seen > CLIENT_TIMEOUT_S:
                del self.clients[client]
                cell.detach()

    def attach(self, client):
        cell = next((cell for cell in self.cells if cell.client is None), None)
        if cell is None:
            print(f"client {client[0]}:{client[1]} rejected: no free cell", flush=True)
            return None
        cell.attach(client)
        return cell

    def read_client(self):
        message, client = self.client_socket.recvfrom(UPLINK_PDU_SIZE + 1)
        entry = self.clients.get(client)
        if message[:1] == b"H":
            cell = entry[0] if entry else self.attach(client)
            if cell is None:
                return
            self.clients[client] = (cell, time.monotonic())
            cell.send_config()
        elif entry is None:
            return
        elif message[:1] == b"U" and len(message) == UPLINK_PDU_SIZE + 1:
            entry[0].receive_uplink(message[1:])
        elif message[:1] == b"F" and len(message) == 5:
            entry[0].receive_complete(struct.unpack(">I", message[1:5])[0])


def open_socket(address, port, remote_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((address, port))
    if remote_port is not None:
        sock.connect((address, remote_port))
    sock.setblocking(False)
    return sock


def send(sock, data, address=None):
    try:
        if address is None:
            sock.send(data)
        else:
            sock.sendto(data, address)
    except OSError as error:
        print(f"send failed: {error}", flush=True)


def nope_indication(tn, fn):
    return struct.pack(">BIBhBh", (TRXD_VERSION << 4) | tn, fn, 0, 0, MTS_NOPE, 0)


def is_after(fn, reference):
    return 0 < (fn - reference) % HYPERFRAME_FRAMES < HYPERFRAME_FRAMES // 2


def frames_between(fn, later):
    return (later - fn) % HYPERFRAME_FRAMES


async def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--address", default="127.0.0.1")
    parser.add_argument("--base-port", type=int, default=6700)
    parser.add_argument("--client-address", default="0.0.0.0")
    parser.add_argument("--client-port", type=int, default=5750)
    parser.add_argument("--cells", type=int, default=int(os.environ.get("BTS_CELLS", "4")))
    args = parser.parse_args()

    TrxMux(args.address, args.base_port, args.client_address, args.client_port, args.cells)
    print(f"trxmux: {args.cells} cells from {args.address}:{args.base_port}, clients on "
          f"{args.client_address}:{args.client_port}", flush=True)
    await asyncio.Event().wait()


if __name__ == "__main__":
    asyncio.run(main())
