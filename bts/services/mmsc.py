#!/usr/bin/env python3
"""Minimal MMSC (OMA MMS Encapsulation 1.2).

Phones post m-send-req to this server. The MMSC stores the message, answers
with m-send-conf and notifies every recipient with an m-notification-ind WAP
push. The recipient fetches the stored m-retrieve-conf over HTTP GET. The
sender address is taken from the PDP context of the client IP address.
"""

import http.server
import os
import random
import re
import time
import uuid

from osmocom import GGSN_VTY_PORT, HLR_VTY_PORT, send_wap_push, vty_command

LISTEN = ("0.0.0.0", 8002)
BASE_URL = "http://172.16.32.1:8002/"
STORE_DIR = "/var/lib/mmsc"
EXPIRY_S = 7 * 24 * 3600
MMS_CONTENT_TYPE = "application/vnd.wap.mms-message"

FIELD_BCC = 0x81
FIELD_CC = 0x82
FIELD_CONTENT_LOCATION = 0x83
FIELD_CONTENT_TYPE = 0x84
FIELD_DATE = 0x85
FIELD_EXPIRY = 0x88
FIELD_FROM = 0x89
FIELD_MESSAGE_CLASS = 0x8A
FIELD_MESSAGE_ID = 0x8B
FIELD_MESSAGE_TYPE = 0x8C
FIELD_MMS_VERSION = 0x8D
FIELD_MESSAGE_SIZE = 0x8E
FIELD_RESPONSE_STATUS = 0x92
FIELD_TO = 0x97
FIELD_TRANSACTION_ID = 0x98

MESSAGE_SEND_REQ = 0x80
MESSAGE_SEND_CONF = 0x81
MESSAGE_NOTIFICATION_IND = 0x82
MESSAGE_RETRIEVE_CONF = 0x84
MMS_VERSION_1_2 = 0x92
RESPONSE_STATUS_OK = 0x80
MESSAGE_CLASS_PERSONAL = 0x80
ADDRESS_PRESENT = 0x80
EXPIRY_RELATIVE = 0x81
REPLACED_FIELDS = {FIELD_MESSAGE_TYPE, FIELD_TRANSACTION_ID, FIELD_MMS_VERSION, FIELD_FROM, FIELD_MESSAGE_ID, FIELD_DATE}

WSP_PUSH = 0x06
WSP_CONTENT_TYPE_MMS = 0xBE
WSP_HEADER_APPLICATION_ID = 0xAF
WSP_APPLICATION_ID_MMS_UA = 0x84

PDP_IMSI = re.compile(r"IMSI[:\s]+(\d{6,15})")
HLR_MSISDN = re.compile(r"MSISDN:\s*(\d+)")


def uintvar(data, pos):
    value = 0
    while True:
        byte = data[pos]
        pos += 1
        value = (value << 7) | (byte & 0x7F)
        if byte < 0x80:
            return value, pos


def value_end(data, pos):
    """End of one WSP-encoded header value starting at pos."""
    first = data[pos]
    if first >= 0x80:
        return pos + 1
    if first <= 0x1E:
        return pos + 1 + first
    if first == 0x1F:
        length, start = uintvar(data, pos + 1)
        return start + length
    return data.index(0, pos) + 1


def parse_headers(pdu):
    """Return the header fields before Content-Type and the Content-Type field with the body."""
    headers = []
    pos = 0
    while pdu[pos] != FIELD_CONTENT_TYPE:
        end = value_end(pdu, pos + 1)
        headers.append((pdu[pos], pdu[pos + 1:end]))
        pos = end
    return headers, pdu[pos:]


def encoded_string(value):
    """Text of an Encoded-string-value; the charset of the value-length form is skipped."""
    if value[0] <= 0x1F:
        charset = 1 if value[0] <= 0x1E else uintvar(value, 1)[1]
        value = value[value_end(value, charset):]
    return value.rstrip(b"\x00").decode(errors="replace")


def address_number(address):
    return address.split("/", 1)[0].lstrip("+")


def long_integer(value):
    data = value.to_bytes(max(1, (value.bit_length() + 7) // 8), "big")
    return bytes([len(data)]) + data


def text(value):
    return value.encode() + b"\x00"


def from_field(msisdn):
    address = text(f"{msisdn}/TYPE=PLMN")
    return bytes([FIELD_FROM, len(address) + 1, ADDRESS_PRESENT]) + address


def msisdn_for_ip(ip):
    pdp = "\n".join(vty_command(GGSN_VTY_PORT, f"show pdp-context ggsn ggsn0 ipv4 {ip}"))
    imsi = PDP_IMSI.search(pdp)
    if not imsi:
        raise LookupError(f"no PDP context for {ip}")
    subscriber = "\n".join(vty_command(HLR_VTY_PORT, f"show subscriber imsi {imsi.group(1)}"))
    msisdn = HLR_MSISDN.search(subscriber)
    if not msisdn:
        raise LookupError(f"no MSISDN for IMSI {imsi.group(1)}")
    return msisdn.group(1)


def notification(message_id, sender, size):
    body = (bytes([FIELD_MESSAGE_TYPE, MESSAGE_NOTIFICATION_IND, FIELD_TRANSACTION_ID]) + text(message_id) +
            bytes([FIELD_MMS_VERSION, MMS_VERSION_1_2]) + from_field(sender) +
            bytes([FIELD_MESSAGE_CLASS, MESSAGE_CLASS_PERSONAL, FIELD_MESSAGE_SIZE]) + long_integer(size))
    expiry = bytes([EXPIRY_RELATIVE]) + long_integer(EXPIRY_S)
    body += bytes([FIELD_EXPIRY, len(expiry)]) + expiry
    body += bytes([FIELD_CONTENT_LOCATION]) + text(BASE_URL + message_id)
    headers = bytes([WSP_CONTENT_TYPE_MMS, WSP_HEADER_APPLICATION_ID, WSP_APPLICATION_ID_MMS_UA])
    return bytes([0x01, WSP_PUSH, len(headers)]) + headers + body


def store(request, sender):
    headers, content = parse_headers(request)
    message_id = uuid.uuid4().hex
    retrieve = (bytes([FIELD_MESSAGE_TYPE, MESSAGE_RETRIEVE_CONF, FIELD_MESSAGE_ID]) + text(message_id) +
                bytes([FIELD_MMS_VERSION, MMS_VERSION_1_2, FIELD_DATE]) + long_integer(int(time.time())) +
                from_field(sender) +
                b"".join(bytes([field]) + value for field, value in headers if field not in REPLACED_FIELDS) +
                content)
    with open(os.path.join(STORE_DIR, message_id), "wb") as file:
        file.write(retrieve)
    recipients = [address_number(encoded_string(value)) for field, value in headers
                  if field in (FIELD_TO, FIELD_CC, FIELD_BCC)]
    transaction_id = next(value for field, value in headers if field == FIELD_TRANSACTION_ID)
    return message_id, transaction_id, recipients, len(retrieve)


class Handler(http.server.BaseHTTPRequestHandler):
    def reply(self, body=b""):
        self.send_response(200)
        self.send_header("Content-Type", MMS_CONTENT_TYPE)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        message_id = os.path.basename(self.path)
        try:
            with open(os.path.join(STORE_DIR, message_id), "rb") as file:
                self.reply(file.read())
        except (FileNotFoundError, IsADirectoryError):
            self.send_error(404)

    def do_POST(self):
        request = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        if len(request) < 2 or request[0] != FIELD_MESSAGE_TYPE or request[1] != MESSAGE_SEND_REQ:
            self.reply()
            return

        sender = msisdn_for_ip(self.client_address[0])
        message_id, transaction_id, recipients, size = store(request, sender)
        self.reply(bytes([FIELD_MESSAGE_TYPE, MESSAGE_SEND_CONF, FIELD_TRANSACTION_ID]) + transaction_id +
                   bytes([FIELD_MMS_VERSION, MMS_VERSION_1_2, FIELD_RESPONSE_STATUS, RESPONSE_STATUS_OK,
                          FIELD_MESSAGE_ID]) + text(message_id))
        for recipient in recipients:
            send_wap_push(recipient, notification(message_id, sender, size), random.randrange(256))
        print(f"MMS {message_id} from {sender} to {', '.join(recipients)}, {size} bytes", flush=True)


def main():
    os.makedirs(STORE_DIR, exist_ok=True)
    http.server.ThreadingHTTPServer(LISTEN, Handler).serve_forever()


if __name__ == "__main__":
    main()
