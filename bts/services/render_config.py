#!/usr/bin/env python3
"""Render the BSC, MSC or SGSN config for the stack settings."""

import sys

import stack


def bsc_config():
    blocks = "".join(stack.render("osmo-bsc-bts.cfg", stack.cell_values(cell)) for cell in range(stack.cells()))
    return stack.render("osmo-bsc.cfg", stack.operator_values() | {"BTS": blocks.rstrip("\n")})


def msc_config():
    return stack.render("osmo-msc.cfg", stack.operator_values())


def sgsn_config():
    auth = stack.SGSN_CIPHERING if stack.gprs_ciphering() else stack.SGSN_NO_CIPHERING
    return stack.render("osmo-sgsn.cfg", {"AUTH": auth})


CONFIGS = {"bsc": bsc_config, "msc": msc_config, "sgsn": sgsn_config}


def main():
    name, path = sys.argv[1:]
    stack.write(path, CONFIGS[name]())


if __name__ == "__main__":
    main()
