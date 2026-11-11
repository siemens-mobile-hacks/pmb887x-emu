#!/usr/bin/env python3
"""Run an osmo-bts-trx and an osmo-pcu for every cell and restart each one that exits."""

import os
import signal
import subprocess
import time

import stack

RUN_DIR = "/run/cells"
RESTART_DELAY_S = 1


def write_config(program, template, cell):
    path = os.path.join(RUN_DIR, f"{program}-{cell}.cfg")
    stack.write(path, stack.render(template, stack.cell_values(cell)))
    return [program, "-c", path]


def stop(processes):
    for process in processes.values():
        process.send_signal(signal.SIGTERM)
    for process in processes.values():
        process.wait()
    raise SystemExit(0)


def main():
    os.makedirs(RUN_DIR, exist_ok=True)
    commands = []
    for cell in range(stack.cells()):
        commands.append(write_config("osmo-bts-trx", "osmo-bts-trx.cfg", cell))
        commands.append(write_config("osmo-pcu", "osmo-pcu.cfg", cell))

    processes = {}
    signal.signal(signal.SIGTERM, lambda _signum, _frame: stop(processes))
    for command in commands:
        process = subprocess.Popen(command)
        processes[process.pid] = process

    while True:
        pid, status = os.wait()
        command = processes.pop(pid).args
        print(f"{' '.join(command)} exited with status {status}, restarting", flush=True)
        time.sleep(RESTART_DELAY_S)
        process = subprocess.Popen(command)
        processes[process.pid] = process


if __name__ == "__main__":
    main()
