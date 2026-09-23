#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import pty
import signal
import subprocess
import sys
import threading
import time
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


def forward_test_output(source, result):
    pending = b""
    while data := source.read(4096):
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
        lines = (pending + data).split(b"\n")
        pending = lines.pop()
        for line in lines:
            if line.startswith(b"not ok "):
                result["failed"] = True
            if line.startswith(b"# result: "):
                result["passed"] = b"PASS" in line and b"(0 failed)" in line


def wait_for_test(test, emulator, timeout):
    deadline = time.monotonic() + timeout
    while True:
        test_status = test.poll()
        if test_status is not None:
            return test_status

        emulator_status = emulator.poll()
        if emulator_status is not None:
            print(f"QEMU exited with status {emulator_status} before the test finished", file=sys.stderr)
            return 1

        remaining = deadline - time.monotonic()
        if remaining <= 0:
            print(f"timeout after {timeout} seconds", file=sys.stderr)
            return 124
        time.sleep(min(0.1, remaining))


def main():
    parser = argparse.ArgumentParser(
        usage="%(prog)s -d DEVICE [-f FULLFLASH] -u UNIT [-t SECONDS] [--no-build] [-- EMU_ARG ...]",
        description="Build QEMU and run one BSP unit test",
        epilog="example: unit.py -d siemens-el71 -f EL71.bin -u stm -- --trace cpu",
    )
    parser.add_argument("-d", "--device", required=True, help="device slug, for example siemens-el71")
    parser.add_argument("-f", "--fullflash", type=Path, help="fullflash image")
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
        return 2

    unit_root = ROOT / "bsp" / "unit"
    build_dir = unit_root / "build" / args.device
    configure_command = [
        "cmake", "-B", build_dir,
        f"-DBOARD={args.device}",
        "-DBOOT=extram",
        "-DTEST_COLOR=ON",
    ]
    if subprocess.run(configure_command, cwd=unit_root).returncode != 0:
        return 2
    if subprocess.run(["cmake", "--build", build_dir, "--target", args.unit], cwd=unit_root).returncode != 0:
        return 2

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
    ]
    if args.fullflash is not None:
        emulator_command.extend(["--fullflash", args.fullflash.expanduser()])
    emulator_command.extend([*emulator_args, "--serial", qemu_serial, "-W"])
    test_command = [
        sys.executable,
        ROOT / "bsp" / "boot.py",
        build_dir / (args.unit + ".bin"),
        "--no-ign",
        "--device", test_serial,
    ]

    emulator = None
    test = None
    output_thread = None
    result = {"failed": False, "passed": False}
    try:
        emulator = subprocess.Popen(emulator_command, cwd=ROOT, start_new_session=True)
        test = subprocess.Popen(test_command, cwd=unit_root, start_new_session=True, stdout=subprocess.PIPE)
        output_thread = threading.Thread(target=forward_test_output, args=(test.stdout, result))
        output_thread.start()
        status = wait_for_test(test, emulator, args.timeout)
    finally:
        stop(test)
        stop(emulator)
        if output_thread is not None:
            output_thread.join()
        for descriptor in (qemu_master, qemu_slave, test_master, test_slave):
            os.close(descriptor)

    if status != 0:
        return status
    return 0 if result["passed"] and not result["failed"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
