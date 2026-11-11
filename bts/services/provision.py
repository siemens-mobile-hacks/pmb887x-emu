#!/usr/bin/env python3
"""Send OMA Client Provisioning settings to every new subscriber once per stack run.

Polls the subscriber list of osmo-hlr over its VTY and sends each new IMSI a
WAP push with the Internet, browser and MMS settings through the SMPP
interface of osmo-msc. The push is signed with NETWPIN (OMA ProvBoot), which
the phone verifies against its own IMSI without asking for a PIN.
"""

import hashlib
import hmac
import re
import time

from osmocom import HLR_VTY_PORT, send_wap_push, vty_command

POLL_INTERVAL_S = 5

APN = "internet"
NAPID = "INTERNET GPRS"
APN_USER = "internet"
APN_PASSWORD = "internet"
HOMEPAGE = "http://google.com/"
MMSC = "http://172.16.32.1:8002/"
NO_DNS = "0.0.0.0"
QOS_SUBSCRIBED = "0"
QOS_PARAMETERS = (
    "TRAFFIC-CLASS", "DELIVERY-ERR-SDU", "DELIVERY-ORDER", "MAX-SDU-SIZE", "MAX-BITRATE-UPLINK",
    "MAX-BITRATE-DNLINK", "RESIDUAL-BER", "SDU-ERROR-RATIO", "TRAFFIC-HANDL-PRIO", "TRANSFER-DELAY",
    "GUARANTEED-BITRATE-UPLINK", "GUARANTEED-BITRATE-DNLINK",
)

SUBSCRIBER_LINE = re.compile(r"^\s*\d+\s+(\d+)\s+(\d{6,15})\s")

WBXML_HEADER = bytes([0x03, 0x0B, 0x6A, 0x00])
WBXML_END = b"\x01"
TAG_PROVISIONINGDOC = b"\x45"
TAG_CHARACTERISTIC = b"\xC6"
TAG_PARM = b"\x87"
ATTR_NAME = b"\x05"
ATTR_VALUE = b"\x06"
ATTR_TYPE = b"\x50"

WSP_PUSH = 0x06
WSP_LENGTH_QUOTE = 0x1F
CONTENT_TYPE_PROVISIONING = 0xB6
PARAM_SEC = 0x91
SEC_NETWPIN = 0x80
PARAM_MAC = 0x92



def wbxml_string(text):
    return b"\x03" + text.encode() + b"\x00"


def parm(name, value=None):
    out = TAG_PARM + ATTR_NAME + wbxml_string(name)
    if value is not None:
        out += ATTR_VALUE + wbxml_string(value)
    return out + WBXML_END


def characteristic(kind, *children):
    return TAG_CHARACTERISTIC + ATTR_TYPE + wbxml_string(kind) + WBXML_END + b"".join(children) + WBXML_END


def provisioning_document():
    return WBXML_HEADER + TAG_PROVISIONINGDOC + b"".join([
        characteristic("BOOTSTRAP", parm("NAME", "Internet")),
        characteristic("NAPDEF",
            parm("NAPID", NAPID), parm("BEARER", "GSM-GPRS"), parm("NAME", "Internet"),
            parm("NAP-ADDRESS", APN), parm("NAP-ADDRTYPE", "APN"), parm("CALLTYPE", "ANALOG-MODEM"), parm("LINGER", "180"),
            parm("DNS-ADDR", NO_DNS), parm("DNS-ADDR", NO_DNS),
            *(parm(name, QOS_SUBSCRIBED) for name in QOS_PARAMETERS),
            characteristic("NAPAUTHINFO", parm("AUTHTYPE", "PAP"), parm("AUTHNAME", APN_USER), parm("AUTHSECRET", APN_PASSWORD))),
        characteristic("APPLICATION",
            parm("APPID", "w2"), parm("TO-NAPID", NAPID), parm("INTERNET"), parm("NAME", "Internet"),
            characteristic("RESOURCE", parm("URI", HOMEPAGE), parm("NAME", "Home"), parm("STARTPAGE"))),
        characteristic("APPLICATION",
            parm("APPID", "w4"), parm("TO-NAPID", NAPID), parm("INTERNET"), parm("NAME", "MMS"), parm("ADDR", MMSC)),
    ]) + WBXML_END


def netwpin(imsi):
    """IMSI in the EF_IMSI semi-octet format without the length byte (OMA ProvBoot)."""
    digits = [int(digit) for digit in imsi]
    odd = len(digits) % 2
    tail = digits[1:] + ([] if odd else [0xF])
    first = (digits[0] << 4) | (0x9 if odd else 0x1)
    return bytes([first] + [(tail[i + 1] << 4) | tail[i] for i in range(0, len(tail), 2)])


def wap_push(imsi):
    document = provisioning_document()
    mac = hmac.new(netwpin(imsi), document, hashlib.sha1).hexdigest().upper().encode()
    content_type = bytes([CONTENT_TYPE_PROVISIONING, PARAM_SEC, SEC_NETWPIN, PARAM_MAC]) + mac + b"\x00"
    headers = bytes([WSP_LENGTH_QUOTE, len(content_type)]) + content_type
    return bytes([0x01, WSP_PUSH, len(headers)]) + headers + document


def subscribers():
    for line in vty_command(HLR_VTY_PORT, "show subscribers all"):
        if match := SUBSCRIBER_LINE.match(line):
            yield match.group(2), match.group(1)


def main():
    provisioned = set()
    reference = 0
    while True:
        try:
            for imsi, msisdn in subscribers():
                if imsi in provisioned:
                    continue
                reference = (reference + 1) & 0xFF
                send_wap_push(msisdn, wap_push(imsi), reference)
                provisioned.add(imsi)
                print(f"provisioned {imsi} ({msisdn})", flush=True)
        except (OSError, RuntimeError) as error:
            print(f"provisioning failed: {error}", flush=True)
        time.sleep(POLL_INTERVAL_S)


if __name__ == "__main__":
    main()
