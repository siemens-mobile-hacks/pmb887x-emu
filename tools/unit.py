#!/usr/bin/env python3

import argparse
import fcntl
import os
from pathlib import Path
import pty
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import tty


ROOT = Path(__file__).resolve().parent.parent
UNIT_ROOT = ROOT / "bsp" / "unit"
LOCK_PATH = ROOT / "build" / "unit-tests.lock"
LOCK_HELD_ENV = "PMB887X_UNIT_LOCK_HELD"
AUDIO_EXPECT_PREFIX = b"# AUDIO-EXPECT: "
AUDIO_CAPTURE_RATE_HZ = 48000
BSP_ROOT = ROOT / "bsp"
E2E_ROOT = BSP_ROOT / "e2e"
E2E_RUNNERS = {
    ".ts": [E2E_ROOT / "node_modules" / ".bin" / "tsx"],
    ".py": [sys.executable, "-u"],
}


def terminate(signum, _frame):
    raise SystemExit(128 + signum)


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
            if line.startswith(AUDIO_EXPECT_PREFIX):
                result["audio_expect"].append(line[len(AUDIO_EXPECT_PREFIX):].decode().strip())


def forward_e2e_output(source):
    for line in source:
        sys.stdout.buffer.write(b"# " + line.rstrip(b"\n") + b"\n")
        sys.stdout.buffer.flush()


def print_e2e_result(unit, passed):
    if passed:
        print(f"ok 1 - {unit}\n1..1\n# result: PASS (0 failed)", flush=True)
    else:
        print(f"not ok 1 - {unit}\n1..1\n# result: FAIL (1 failed)", flush=True)


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


def check_audio(audio_path, expectations):
    import audio_check

    if not expectations:
        print("not ok - audio capture requested, but the test printed no AUDIO-EXPECT lines")
        return False
    if not audio_path.is_file():
        print(f"not ok - QEMU wrote no audio to {audio_path}")
        return False

    passed, messages = audio_check.check_recording(audio_path, expectations)
    for message in messages:
        print(f"# audio: {message}")
    if not passed:
        print(f"not ok - recorded audio matches AUDIO-EXPECT, kept {audio_path}")
    return passed


def acquire_run_lock():
    LOCK_PATH.parent.mkdir(parents=True, exist_ok=True)
    lock = open(LOCK_PATH, "w")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        lock.close()
        print("error: another unit.py or unit-all.py run is in progress", file=sys.stderr)
        return None
    return lock


def unit_build_dir(device):
    return UNIT_ROOT / "build" / device


def build_units(device, units, jobs=None):
    build_dir = unit_build_dir(device)
    configure_command = [
        "cmake", "-B", build_dir,
        f"-DBOARD={device}",
        "-DBOOT=extram",
        "-DTEST_COLOR=ON",
    ]
    build_command = ["cmake", "--build", build_dir, "--target", *units]
    if jobs is not None:
        build_command.extend(["--parallel", str(jobs)])
    if subprocess.run(configure_command, cwd=UNIT_ROOT).returncode != 0:
        return False
    return subprocess.run(build_command, cwd=UNIT_ROOT).returncode == 0


def split_test_name(test):
    kind, _, name = test.rpartition("/")
    return kind or "unit", name


def find_e2e_test(name):
    for suffix in E2E_RUNNERS:
        path = E2E_ROOT / name / (name + suffix)
        if path.is_file():
            return path
    return None


def install_typescript_dependencies():
    install_marker = BSP_ROOT / "node_modules" / ".modules.yaml"
    lockfile = BSP_ROOT / "pnpm-lock.yaml"
    if install_marker.is_file() and lockfile.stat().st_mtime_ns <= install_marker.stat().st_mtime_ns:
        return True
    command = ["pnpm", "--dir", BSP_ROOT, "install", "--frozen-lockfile", "--prefer-offline"]
    return subprocess.run(command, cwd=ROOT).returncode == 0


def main():
    signal.signal(signal.SIGTERM, terminate)

    parser = argparse.ArgumentParser(
        usage="%(prog)s -d DEVICE [-f FULLFLASH] -u UNIT [-t SECONDS] [--no-build] [--no-bsp-build] [-- EMU_ARG ...]",
        description="Build QEMU and run one BSP unit test or e2e test",
        epilog=(
            "examples:\n"
            "  unit.py -d siemens-el71 -f EL71.bin -u stm -- --trace STM\n"
            "  unit.py -d siemens-el71 -f EL71.bin -u e2e/gsm-creg"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("-d", "--device", required=True, help="device slug, for example siemens-el71")
    parser.add_argument("-f", "--fullflash", type=Path, help="fullflash image")
    parser.add_argument("-u", "--unit", required=True, help="[unit/]NAME for a BSP unit test, e2e/NAME for an e2e test")
    parser.add_argument("-t", "--timeout", type=int, default=120, help="test timeout in seconds (default: 120)")
    parser.add_argument("--no-build", action="store_true", help="do not build QEMU")
    parser.add_argument("--no-bsp-build", action="store_true", help="do not build the BSP unit test")
    parser.add_argument(
        "--audio-capture", action="store_true", help="record QEMU audio and check it against AUDIO-EXPECT lines"
    )
    parser.add_argument(
        "emulator_args", nargs=argparse.REMAINDER, metavar="EMU_ARG", help="arguments after -- passed to QEMU"
    )
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    emulator_args = args.emulator_args[1:] if args.emulator_args[:1] == ["--"] else args.emulator_args

    kind, test_name = split_test_name(args.unit)
    if kind not in ("unit", "e2e"):
        parser.error(f"unknown test kind {kind!r}; use unit/NAME or e2e/NAME")
    e2e_test = None
    if kind == "e2e":
        e2e_test = find_e2e_test(test_name)
        if e2e_test is None:
            parser.error(f"e2e test not found: {E2E_ROOT / test_name / test_name}.ts or .py")
        if args.audio_capture:
            parser.error("--audio-capture is valid only for BSP unit tests")
        if args.fullflash is None:
            parser.error("e2e tests need --fullflash")

    if os.environ.get(LOCK_HELD_ENV) is None:
        run_lock = acquire_run_lock()
        if run_lock is None:
            return 2

    if not args.no_build and subprocess.run([ROOT / "tools" / "build.sh"], cwd=ROOT).returncode != 0:
        return 2

    build_dir = unit_build_dir(args.device)
    if e2e_test is None and not args.no_bsp_build and not build_units(args.device, [test_name]):
        return 2
    if e2e_test is not None and e2e_test.suffix == ".ts" and not install_typescript_dependencies():
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
    emulator_command.extend(["--serial", qemu_serial])
    if e2e_test is None:
        emulator_command.append("-W")
    emulator_command.extend(emulator_args)
    audio_path = None
    if args.audio_capture:
        audio_path = Path(tempfile.mkdtemp(prefix="unit-audio-")) / "audio.wav"
        if "--" not in emulator_args:
            emulator_command.append("--")
        emulator_command.extend(["-audio", f"driver=wav,path={audio_path},out.frequency={AUDIO_CAPTURE_RATE_HZ}"])
    if e2e_test is None:
        test_command = [
            sys.executable,
            ROOT / "bsp" / "boot.py",
            build_dir / (test_name + ".bin"),
            "--no-ign",
            "--device", test_serial,
        ]
        test_cwd = UNIT_ROOT
        test_stderr = None
    else:
        test_command = [*E2E_RUNNERS[e2e_test.suffix], e2e_test, test_serial]
        test_cwd = e2e_test.parent
        test_stderr = subprocess.STDOUT

    emulator = None
    test = None
    output_thread = None
    result = {"failed": False, "passed": False, "audio_expect": []}
    try:
        emulator = subprocess.Popen(emulator_command, cwd=ROOT, start_new_session=True)
        test = subprocess.Popen(
            test_command, cwd=test_cwd, start_new_session=True, stdout=subprocess.PIPE, stderr=test_stderr
        )
        if e2e_test is None:
            output_thread = threading.Thread(target=forward_test_output, args=(test.stdout, result))
        else:
            output_thread = threading.Thread(target=forward_e2e_output, args=(test.stdout,))
        output_thread.start()
        status = wait_for_test(test, emulator, args.timeout)
    finally:
        stop(test)
        stop(emulator)
        if output_thread is not None:
            output_thread.join()
        for descriptor in (qemu_master, qemu_slave, test_master, test_slave):
            os.close(descriptor)

    if e2e_test is not None:
        print_e2e_result(args.unit, status == 0)
        return status

    test_passed = status == 0 and result["passed"] and not result["failed"]
    audio_passed = audio_path is None or (test_passed and check_audio(audio_path, result["audio_expect"]))
    if audio_path is not None and (audio_passed or not test_passed):
        shutil.rmtree(audio_path.parent)
    if status != 0:
        return status
    return 0 if test_passed and audio_passed else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
