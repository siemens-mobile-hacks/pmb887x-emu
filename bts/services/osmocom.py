"""Access to the Osmocom stack: VTY commands and WAP push over SMPP."""

import socket
import struct

SMPP = ("127.0.0.1", 2775)
SMPP_SYSTEM_ID = "provision"
SMPP_PASSWORD = "prov"
SMPP_BIND_TRANSMITTER = 0x00000002
SMPP_SUBMIT_SM = 0x00000004
SMPP_INTERFACE_VERSION = 0x34
SMPP_ESM_CLASS_UDHI = 0x40
SMPP_DATA_CODING_8BIT = 0x04
ORIGINATOR = "1234"

WAP_PUSH_PORTS = bytes([0x05, 0x04, 0x0B, 0x84, 0x23, 0xF0])
UDH_CONCAT = 0x00
SMS_USER_DATA_SIZE = 140

HLR_VTY_PORT = 4258
GGSN_VTY_PORT = 4260


def vty_read_prompt(sock):
    output = b""
    while not output.rstrip().endswith((b">", b"#")):
        chunk = sock.recv(65536)
        if not chunk:
            raise OSError("VTY closed the connection")
        output += chunk
    return output


def vty_command(port, command):
    """Run one command in enable mode and return its output lines."""
    with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
        vty_read_prompt(sock)
        sock.sendall(b"enable\r\n")
        vty_read_prompt(sock)
        sock.sendall(command.encode() + b"\r\n")
        return vty_read_prompt(sock).decode(errors="replace").splitlines()


def cstring(text):
    return text.encode() + b"\x00"


class Smpp:
    def __init__(self):
        self.sock = socket.create_connection(SMPP, timeout=10)
        self.sequence = 0
        body = cstring(SMPP_SYSTEM_ID) + cstring(SMPP_PASSWORD) + cstring("") + bytes([SMPP_INTERFACE_VERSION, 0, 0])
        self.request(SMPP_BIND_TRANSMITTER, body + cstring(""))

    def request(self, command, body):
        self.sequence += 1
        self.sock.sendall(struct.pack(">IIII", 16 + len(body), command, 0, self.sequence) + body)
        length, _, status, _ = struct.unpack(">IIII", self.sock.recv(16))
        self.sock.recv(length - 16)
        if status != 0:
            raise RuntimeError(f"SMPP command {command:#x} failed with status {status:#x}")

    def submit(self, msisdn, message):
        body = (cstring("") + bytes([0, 0]) + cstring(ORIGINATOR) + bytes([1, 1]) + cstring(msisdn) +
                bytes([SMPP_ESM_CLASS_UDHI, 0, 0]) + cstring("") + cstring("") +
                bytes([0, 0, SMPP_DATA_CODING_8BIT, 0, len(message)]) + message)
        self.request(SMPP_SUBMIT_SM, body)

    def close(self):
        self.sock.close()


def send_wap_push(msisdn, push, reference):
    """Send a WSP push PDU to WDP port 2948 as concatenated binary SMS."""
    chunk = SMS_USER_DATA_SIZE - 1 - len(WAP_PUSH_PORTS) - 5
    parts = [push[i:i + chunk] for i in range(0, len(push), chunk)]
    smpp = Smpp()
    try:
        for index, part in enumerate(parts):
            udh = WAP_PUSH_PORTS + bytes([UDH_CONCAT, 3, reference, len(parts), index + 1])
            smpp.submit(msisdn, bytes([len(udh)]) + udh + part)
    finally:
        smpp.close()
