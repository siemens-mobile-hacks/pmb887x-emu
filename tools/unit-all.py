#!/usr/bin/env python3

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from fnmatch import fnmatchcase
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import threading
import tomllib

from unit import LOCK_HELD_ENV, acquire_run_lock, build_units


ROOT = Path(__file__).resolve().parent.parent
UNIT_RUNNER = ROOT / "tools" / "unit.py"
DEFAULT_CONFIG = ROOT / "bsp" / "unit" / "tests.toml"
DEFAULT_TIMEOUT = 120


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Run BSP unit tests from a TOML configuration",
        epilog=(
            "examples:\n"
            "  unit-all.py\n"
            "  unit-all.py -d siemens-el71 dsp-hle-gsm-channel-scan\n"
            "  unit-all.py --no-build stm dmac"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("tests", nargs="*", help="test names or glob patterns; all tests by default")
    parser.add_argument("-c", "--config", type=Path, default=DEFAULT_CONFIG, help="TOML configuration file")
    parser.add_argument(
        "-d", "--device", action="append",
        help="device to run named tests on, or filter for all tests; may be repeated",
    )
    parser.add_argument("-t", "--timeout", type=int, help="override every configured timeout")
    parser.add_argument("-n", "--no-build", action="store_true", help="do not build the emulator")
    parser.add_argument(
        "-j", "--jobs", type=int, default=1,
        help="run this many tests in parallel; output of each test is printed when it finishes (default: 1)",
    )
    parser.add_argument("-l", "--list", action="store_true", help="list selected tests without running them")
    parser.add_argument(
        "--color",
        choices=("auto", "always", "never"),
        default="auto",
        help="color output: auto, always, or never (default: auto)",
    )
    return parser.parse_args(argv)


def load_config(path):
    with path.open("rb") as config_file:
        tests = tomllib.load(config_file)
    settings_path = path.with_name("settings.toml")
    fullflashes = {}
    if settings_path.exists():
        with settings_path.open("rb") as settings_file:
            fullflashes = tomllib.load(settings_file).get("fullflashes", {})

    if not isinstance(tests, dict) or not tests:
        raise ValueError("configuration has no tests")
    if not isinstance(fullflashes, dict) or not all(isinstance(value, str) for value in fullflashes.values()):
        raise ValueError("fullflashes must map devices to paths")

    for name, test in tests.items():
        if not isinstance(test, dict):
            raise ValueError(f"test {name} must be a table")

        test_devices = test.get("devices")
        if not isinstance(test_devices, list) or not test_devices or not all(isinstance(device, str) for device in test_devices):
            raise ValueError(f"test {name} has no devices")

        timeout = test.get("timeout", DEFAULT_TIMEOUT)
        if not isinstance(timeout, int) or timeout <= 0:
            raise ValueError(f"test {name} timeout must be positive")

        emulator_args = test.get("emulator_args", [])
        if not isinstance(emulator_args, list) or not all(isinstance(arg, str) for arg in emulator_args):
            raise ValueError(f"test {name} emulator_args must be an array of strings")

    return {"tests": tests, "fullflashes": fullflashes}


def select_jobs(config, test_names, device_names, timeout_override=None):
    tests = config["tests"]
    selected_tests = []
    for pattern in test_names:
        matches = [name for name in tests if fnmatchcase(name, pattern)]
        if not matches:
            raise ValueError(f"no tests match: {pattern}")
        for name in matches:
            if name not in selected_tests:
                selected_tests.append(name)
    if not test_names:
        selected_tests = list(tests)

    selected_devices = set(device_names or [])

    jobs = []
    skipped = []
    for name in selected_tests:
        test = tests[name]
        if test_names and device_names:
            devices = device_names
        else:
            devices = [device for device in test["devices"] if not selected_devices or device in selected_devices]
        if not devices:
            continue
        if test.get("disabled"):
            skipped.append((name, test.get("reason", "disabled")))
            continue

        timeout = timeout_override or test.get("timeout", DEFAULT_TIMEOUT)
        for device in devices:
            jobs.append({
                "test": name,
                "device": device,
                "fullflash": config["fullflashes"].get(device),
                "timeout": timeout,
                "emulator_args": test.get("emulator_args", []),
            })

    return jobs, skipped


def get_fullflash_path(value, config_path):
    if value is None:
        return None

    path = Path(value).expanduser()
    if not path.is_absolute():
        path = config_path.parent / path
    return path


def build_command(job, config_path, no_build, no_bsp_build=False):
    fullflash = get_fullflash_path(job["fullflash"], config_path)
    if fullflash is not None and not fullflash.is_file():
        raise ValueError(f"fullflash not found: {fullflash}")

    command = [
        sys.executable,
        UNIT_RUNNER,
        "--device", job["device"],
    ]
    if fullflash is not None:
        command.extend(["--fullflash", fullflash])
    command.extend(["--unit", job["test"], "--timeout", str(job["timeout"])])
    if no_build:
        command.append("--no-build")
    if no_bsp_build:
        command.append("--no-bsp-build")
    command.extend(["--", "--headless"])
    command.extend(job["emulator_args"])
    return command


def use_color(mode):
    if mode == "always":
        return True
    if mode == "never" or "NO_COLOR" in os.environ:
        return False
    return sys.stdout.isatty()


def color(text, code, enabled):
    if not enabled:
        return text
    return f"\033[{code}m{text}\033[0m"


def print_failures(failures, colors):
    if not failures:
        print(color("No tests failed.", "1;32", colors))
        return

    headers = ("TEST", "DEVICE", "RESULT")
    rows = [(failure["test"], failure["device"], failure["result"]) for failure in failures]
    widths = [max(len(headers[index]), *(len(row[index]) for row in rows)) for index in range(len(headers))]
    border = "+-" + "-+-".join("-" * width for width in widths) + "-+"

    print(color("Failed tests:", "1;31", colors))
    print(border)
    header = "| " + " | ".join(headers[index].ljust(widths[index]) for index in range(len(headers))) + " |"
    print(color(header, "1;36", colors))
    print(border)
    for row in rows:
        cells = [row[index].ljust(widths[index]) for index in range(len(row))]
        cells[2] = color(cells[2], "1;31", colors)
        print("| " + " | ".join(cells) + " |")
    print(border)


def result_name(returncode):
    if returncode == 124:
        return "TIMEOUT"
    if returncode == 2:
        return "BUILD"
    if returncode < 0:
        return f"SIGNAL {-returncode}"
    return f"EXIT {returncode}"


def failed_check_name(line):
    if not line.startswith(b"not ok "):
        return None
    name = line.partition(b" - ")[2]
    if not name:
        return None
    return re.sub(rb"\x1b\[[0-9;]*m", b"", name).decode("utf-8", "replace").strip()


def job_failure(job, returncode, failed_checks):
    result = result_name(returncode)
    if returncode == 1 and failed_checks:
        result = failed_checks[0]
        if len(failed_checks) > 1:
            result += f" (+{len(failed_checks) - 1})"
    return {"test": job["test"], "device": job["device"], "result": result}


def print_summary(failures, passed, skipped, colors, interrupted):
    print()
    print_failures(failures, colors)
    print(f"Result: {passed} passed, {len(failures)} failed, {len(skipped)} skipped")
    if interrupted:
        return 130
    return 1 if failures else 0


def run_jobs(args, jobs, skipped):
    colors = use_color(args.color)
    failures = []
    passed = 0
    build_pending = not args.no_build
    interrupted = False

    for name, reason in skipped:
        print(color(f"SKIP {name}: {reason}", "1;33", colors))

    for index, job in enumerate(jobs, 1):
        title = f"[{index}/{len(jobs)}] {job['test']} ({job['device']})"
        print(color(title, "1;36", colors), flush=True)
        try:
            command = build_command(job, args.config, not build_pending)
        except ValueError as error:
            print(f"error: {error}", file=sys.stderr)
            failures.append({"test": job["test"], "device": job["device"], "result": "CONFIG"})
            continue

        process = None
        failed_checks = []
        try:
            process = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE)
            for line in process.stdout:
                sys.stdout.buffer.write(line)
                sys.stdout.buffer.flush()
                name = failed_check_name(line)
                if name is not None:
                    failed_checks.append(name)
            returncode = process.wait()
        except KeyboardInterrupt:
            if process is not None:
                process.send_signal(signal.SIGINT)
                process.wait()
            failures.append({"test": job["test"], "device": job["device"], "result": "INTERRUPTED"})
            interrupted = True
            break
        finally:
            if process is not None:
                process.stdout.close()
        build_pending = False

        if returncode == 0:
            passed += 1
        else:
            failures.append(job_failure(job, returncode, failed_checks))

    return print_summary(failures, passed, skipped, colors, interrupted)


def build_parallel_jobs(args, jobs):
    if not args.no_build and subprocess.run([ROOT / "tools" / "build.sh"], cwd=ROOT).returncode != 0:
        return False
    units_by_device = {}
    for job in jobs:
        units = units_by_device.setdefault(job["device"], [])
        if job["test"] not in units:
            units.append(job["test"])
    for device, units in units_by_device.items():
        if not build_units(device, units, os.cpu_count()):
            return False
    return True


def run_job_buffered(command, processes, lock):
    process = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    with lock:
        processes.add(process)
    try:
        output = process.stdout.read()
        return process.wait(), output
    finally:
        process.stdout.close()
        with lock:
            processes.discard(process)


def run_jobs_parallel(args, jobs, skipped):
    colors = use_color(args.color)
    failures = []
    passed = 0
    interrupted = False

    for name, reason in skipped:
        print(color(f"SKIP {name}: {reason}", "1;33", colors))
    if not build_parallel_jobs(args, jobs):
        failures = [{"test": job["test"], "device": job["device"], "result": "BUILD"} for job in jobs]
        return print_summary(failures, passed, skipped, colors, interrupted)

    processes = set()
    lock = threading.Lock()
    pool = ThreadPoolExecutor(max_workers=args.jobs)
    futures = {}
    for job in jobs:
        try:
            command = build_command(job, args.config, True, True)
        except ValueError as error:
            print(f"error: {error}", file=sys.stderr)
            failures.append({"test": job["test"], "device": job["device"], "result": "CONFIG"})
            continue
        futures[pool.submit(run_job_buffered, command, processes, lock)] = job

    try:
        for finished, future in enumerate(as_completed(futures), 1):
            job = futures[future]
            returncode, output = future.result()
            title = f"[{finished}/{len(futures)}] {job['test']} ({job['device']})"
            print(color(title, "1;36", colors), flush=True)
            sys.stdout.buffer.write(output)
            sys.stdout.buffer.flush()
            if returncode == 0:
                passed += 1
                continue
            failed_checks = [name for line in output.splitlines(keepends=True)
                if (name := failed_check_name(line)) is not None]
            failures.append(job_failure(job, returncode, failed_checks))
    except KeyboardInterrupt:
        interrupted = True
        pool.shutdown(wait=False, cancel_futures=True)
        with lock:
            for process in processes:
                process.send_signal(signal.SIGINT)
    pool.shutdown(wait=True)

    return print_summary(failures, passed, skipped, colors, interrupted)


def main(argv=None):
    args = parse_args(argv)
    try:
        if args.timeout is not None and args.timeout <= 0:
            raise ValueError("--timeout must be positive")
        if args.jobs <= 0:
            raise ValueError("--jobs must be positive")
        config = load_config(args.config)
        jobs, skipped = select_jobs(config, args.tests, args.device, args.timeout)
    except (OSError, tomllib.TOMLDecodeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    if args.list:
        for job in jobs:
            fullflash = job["fullflash"] or "-"
            print(f"{job['test']}\t{job['device']}\t{job['timeout']}\t{fullflash}")
        for name, reason in skipped:
            print(f"{name}\tSKIP\t{reason}")
        return 0

    if not jobs and not skipped:
        print("error: no tests match the selection", file=sys.stderr)
        return 2

    run_lock = acquire_run_lock()
    if run_lock is None:
        return 2
    os.environ[LOCK_HELD_ENV] = "1"
    if args.jobs > 1:
        return run_jobs_parallel(args, jobs, skipped)
    return run_jobs(args, jobs, skipped)


if __name__ == "__main__":
    sys.exit(main())
