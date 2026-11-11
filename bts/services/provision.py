#!/usr/bin/env python3
"""Set up every new subscriber once per stack run.

Polls the subscriber list of osmo-hlr over its VTY and gives each new IMSI the
XOR-3G key of the emulated SIM for GPRS authentication and ciphering and a
phone number derived from the IMSI. With PROVISION_SETTINGS it also sends a
WAP push with the OMA Client Provisioning document provisioning.xml through the
SMPP interface of osmo-msc. The push is signed with NETWPIN (OMA ProvBoot),
which the phone verifies against its own IMSI without asking for a PIN.
"""

import hashlib
import hmac
import re
import subprocess
import time

import stack
from osmocom import HLR_VTY_PORT, send_wap_push, vty_command

POLL_INTERVAL_S = 0.2

PROVISIONING_DOCUMENT = "/etc/osmocom/provisioning.xml"

SUBSCRIBER_LINE = re.compile(r"^\s*\d+\s+\S+\s+(\d{6,15})\s")

WSP_PUSH = 0x06
WSP_LENGTH_QUOTE = 0x1F
CONTENT_TYPE_PROVISIONING = 0xB6
PARAM_SEC = 0x91
SEC_NETWPIN = 0x80
PARAM_MAC = 0x92



def provisioning_document():
    result = subprocess.run(["xml2wbxml", "-n", "-v", "1.3", "-o", "-", PROVISIONING_DOCUMENT], capture_output=True, check=True)
    return result.stdout


def netwpin(imsi):
    """IMSI in the EF_IMSI semi-octet format without the length byte (OMA ProvBoot)."""
    digits = [int(digit) for digit in imsi]
    odd = len(digits) % 2
    tail = digits[1:] + ([] if odd else [0xF])
    first = (digits[0] << 4) | (0x9 if odd else 0x1)
    return bytes([first] + [(tail[i + 1] << 4) | tail[i] for i in range(0, len(tail), 2)])


def wap_push(imsi, document):
    mac = hmac.new(netwpin(imsi), document, hashlib.sha1).hexdigest().upper().encode()
    content_type = bytes([CONTENT_TYPE_PROVISIONING, PARAM_SEC, SEC_NETWPIN, PARAM_MAC]) + mac + b"\x00"
    headers = bytes([WSP_LENGTH_QUOTE, len(content_type)]) + content_type
    return bytes([0x01, WSP_PUSH, len(headers)]) + headers + document


def subscribers():
    for line in vty_command(HLR_VTY_PORT, "show subscribers all"):
        if match := SUBSCRIBER_LINE.match(line):
            yield match.group(1)


def main():
    document = provisioning_document() if stack.provision_settings() else None
    provisioned = set()
    reference = 0
    while True:
        try:
            for imsi in subscribers():
                if imsi in provisioned:
                    continue
                msisdn = stack.imsi_to_msisdn(imsi)
                vty_command(HLR_VTY_PORT, f"subscriber imsi {imsi} update aud3g xor-3g k {stack.sim_ki()}")
                vty_command(HLR_VTY_PORT, f"subscriber imsi {imsi} update msisdn {msisdn}")
                if document:
                    reference = (reference + 1) & 0xFF
                    send_wap_push(msisdn, wap_push(imsi, document), reference)
                provisioned.add(imsi)
                print(f"provisioned {imsi} ({msisdn})", flush=True)
        except (OSError, RuntimeError) as error:
            print(f"provisioning failed: {error}", flush=True)
        time.sleep(POLL_INTERVAL_S)


if __name__ == "__main__":
    main()
