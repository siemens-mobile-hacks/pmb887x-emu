"""Stack settings from the environment (.env.default and .env) and rendering of the Osmocom config templates.

Templates in /etc/osmocom use @NAME@ placeholders. Every cell gets its own
ARFCN, IPA unit, BVCI, NSEI and ports; trxmux uses the same port layout.
"""

import os
import re

CONFIG_DIR = "/etc/osmocom"
FIRST_ARFCN = 5
ARFCN_STEP = 5
FIRST_UNIT_ID = 1800
FIRST_BVCI = 2
FIRST_NSEI = 101
FIRST_NSVC_PORT = 23010
TRX_BASE_PORT = 6700
TRX_CELL_PORT_STRIDE = 10
TRX_BTS_PORT_OFFSET = 100
MAX_CELLS = TRX_BTS_PORT_OFFSET // TRX_CELL_PORT_STRIDE
FIRST_PCU_VTY_PORT = 42400
FIRST_BTS_VTY_PORT = FIRST_PCU_VTY_PORT + MAX_CELLS
FIRST_BTS_CTRL_PORT = 42380
MSISDN_SUBSCRIBER_DIGITS = 9

SGSN_CIPHERING = """ auth-policy remote
 gsup remote-ip 127.0.0.1
 gsup remote-port 4222
 encryption gea 0 3"""
SGSN_NO_CIPHERING = " auth-policy accept-all"


def cells():
    count = int(os.environ["BTS_CELLS"])
    if not 1 <= count <= MAX_CELLS:
        raise ValueError(f"BTS_CELLS must be 1..{MAX_CELLS}, not {count}")
    return count


def is_enabled(name):
    return os.environ[name].lower() in ("1", "true", "yes", "on")


def gprs_ciphering():
    return is_enabled("GPRS_CIPHERING")


def provision_settings():
    return is_enabled("PROVISION_SETTINGS")


def sim_ki():
    return os.environ["SIM_KI"]


def operator_values():
    return {"OPERATOR_MCC": os.environ["OPERATOR_MCC"], "OPERATOR_MNC": os.environ["OPERATOR_MNC"]}


def imsi_to_msisdn(imsi):
    return os.environ["MSISDN_PREFIX"] + str(int(imsi[-MSISDN_SUBSCRIBER_DIGITS:]))


def cell_values(cell):
    trx_port = TRX_BASE_PORT + cell * TRX_CELL_PORT_STRIDE
    return {
        "CELL": cell,
        "ARFCN": FIRST_ARFCN + cell * ARFCN_STEP,
        "UNIT_ID": FIRST_UNIT_ID + cell,
        "BVCI": FIRST_BVCI + cell,
        "NSEI": FIRST_NSEI + cell,
        "NSVC_PORT": FIRST_NSVC_PORT + cell,
        "TRX_REMOTE_PORT": trx_port,
        "TRX_LOCAL_PORT": trx_port + TRX_BTS_PORT_OFFSET,
        "BTS_VTY_PORT": FIRST_BTS_VTY_PORT + cell,
        "BTS_CTRL_PORT": FIRST_BTS_CTRL_PORT + cell,
        "PCU_VTY_PORT": FIRST_PCU_VTY_PORT + cell,
    }


def render(name, values):
    with open(os.path.join(CONFIG_DIR, name)) as template:
        text = template.read()
    return re.sub(r"@([A-Z_]+)@", lambda match: str(values[match.group(1)]), text)


def write(path, text):
    with open(path, "w") as output:
        output.write(text)
