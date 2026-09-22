#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import pty
import signal
import subprocess
import sys
import threading
import tty


ROOT = Path(__file__).resolve().parent.parent


def stop(process):
    if process is None:
        return
    if process.poll() is not None:
        return

    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait()


def bridge(source, target):
    try:
        while data := os.read(source, 4096):
            while data:
                written = os.write(target, data)
                data = data[written:]
    except OSError:
        pass


def main():
    parser = argparse.ArgumentParser(
        usage="%(prog)s -d DEVICE -f FULLFLASH -u UNIT [-t SECONDS] [--no-build] [-- EMU_ARG ...]",
        description="Build QEMU and run one BSP unit test",
        epilog="example: unit.py -d siemens-el71 -f EL71.bin -u stm -- --trace cpu",
    )
    parser.add_argument("-d", "--device", required=True, help="device slug, for example siemens-el71")
    parser.add_argument("-f", "--fullflash", required=True, type=Path, help="fullflash image")
    parser.add_argument("-u", "--unit", required=True, help="BSP unit-test target")
    parser.add_argument("-t", "--timeout", type=int, default=120, help="test timeout in seconds (default: 120)")
    parser.add_argument("--no-build", action="store_true", help="do not build QEMU")
    parser.add_argument(
        "emulator_args", nargs=argparse.REMAINDER, metavar="EMU_ARG", help="arguments after -- passed to QEMU"
    )
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    emulator_args = args.emulator_args[1:] if args.emulator_args[:1] == ["--"] else args.emulator_args

    if not args.no_build and subprocess.run([ROOT / "tools" / "build.sh"], cwd=ROOT).returncode != 0:
        return 1

    unit_root = ROOT / "bsp" / "unit"
    build_dir = unit_root / "build" / args.device
    configure_command = [
        "cmake", "-B", build_dir,
        f"-DBOARD={args.device}",
        "-DBOOT=extram",
        "-DTEST_COLOR=ON",
    ]
    if subprocess.run(configure_command, cwd=unit_root).returncode != 0:
        return 1
    if subprocess.run(["cmake", "--build", build_dir, "--target", args.unit], cwd=unit_root).returncode != 0:
        return 1

    qemu_master, qemu_slave = pty.openpty()
    test_master, test_slave = pty.openpty()
    tty.setraw(qemu_slave)
    tty.setraw(test_slave)
    qemu_serial = os.ttyname(qemu_slave)
    test_serial = os.ttyname(test_slave)
    threading.Thread(target=bridge, args=(qemu_master, test_master), daemon=True).start()
    threading.Thread(target=bridge, args=(test_master, qemu_master), daemon=True).start()

    emulator_command = [
        ROOT / "build" / "pmb887x-emu",
        "--device", args.device,
        "--fullflash", args.fullflash.expanduser(),
        *emulator_args,
        "--serial", qemu_serial,
        "-W",
    ]
    test_command = [
        sys.executable,
        ROOT / "bsp" / "chaos-boot.py",
        f"--exec={build_dir / (args.unit + '.bin')}",
        "--boot-speed=115200",
        "--speed=115200",
        "--no-ign",
        f"--device={test_serial}",
    ]

    emulator = None
    test = None
    try:
        emulator = subprocess.Popen(emulator_command, cwd=ROOT, start_new_session=True)
        test = subprocess.Popen(test_command, cwd=unit_root, start_new_session=True)
        return test.wait(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        print(f"timeout after {args.timeout} seconds", file=sys.stderr)
        return 124
    finally:
        stop(test)
        stop(emulator)
        for descriptor in (qemu_master, qemu_slave, test_master, test_slave):
            os.close(descriptor)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
