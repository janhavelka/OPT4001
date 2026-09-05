#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import json
import pathlib
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import Iterable

ROOT = pathlib.Path(__file__).resolve().parents[1]

ARDUINO_CTIME_VALUES = [str(i) for i in range(12)]
IDF_CTIME_VALUES = ["600", "1", "2", "3", "6", "12", "25", "50", "100", "200", "400", "800"]

FAIL_PATTERNS = [
    re.compile(r"\bUNKNOWN COMMAND\b", re.IGNORECASE),
    re.compile(r"\bUsage:\b", re.IGNORECASE),
    re.compile(r"\bExpected value\b", re.IGNORECASE),
    re.compile(r"\bInvalid\b", re.IGNORECASE),
    re.compile(r"\bDEVICE_NOT_FOUND\b", re.IGNORECASE),
    re.compile(r"\bDEVICE_ID_MISMATCH\b", re.IGNORECASE),
    re.compile(r"\bI2C_(?:ERROR|TIMEOUT|BUS|NACK_ADDR|NACK_DATA)\b", re.IGNORECASE),
    # Status names are uppercase; prose such as "Timeout / offline threshold"
    # is ordinary configuration output. State/error fields may omit "Status:".
    re.compile(r"\bTIMEOUT\b"),
    re.compile(r"\bNOT_INITIALIZED\b"),
    re.compile(r"\bINVALID_(?:CONFIG|PARAM)\b"),
    re.compile(r"\bOFFLINE\b"),
    re.compile(r"\bBUSY\b"),
    re.compile(r"\bfail(?:ed|ures)?[=: ]+[1-9]\d*\b", re.IGNORECASE),
]

WARN_PATTERNS = [
    re.compile(r"\bCRC_ERROR\b", re.IGNORECASE),
    re.compile(r"\b(?:MEASUREMENT|CONVERSION)_NOT_READY\b", re.IGNORECASE),
]

DATA_COMMANDS = {"read", "lux", "mlux", "ulux", "burst", "fifo", "readburst", "slot"}

EXPECTED_PATTERNS = {
    "version": [re.compile(r"\b(?:OPT4001|version|commit)\b", re.IGNORECASE)],
    "ver": [re.compile(r"\b(?:OPT4001|version|commit)\b", re.IGNORECASE)],
    "scan": [re.compile(r"\b(?:scan|0x[0-9a-f]{2}|found|none)\b", re.IGNORECASE)],
    "probe": [re.compile(r"\b(?:probe|DEVICE_ID|Status:\s*OK|OK)\b", re.IGNORECASE)],
    "id": [re.compile(r"\b(?:DEVICE_ID|didh|raw=0x|match=)\b", re.IGNORECASE)],
    "cfg": [re.compile(r"\b(?:settings|config|snapshot|range|mode|addr)\b", re.IGNORECASE)],
    "state": [re.compile(r"\b(?:state|READY|DEGRADED|OFFLINE|UNINIT)\b", re.IGNORECASE)],
    "drv": [re.compile(r"\b(?:state|health|READY|DEGRADED|OFFLINE|UNINIT)\b", re.IGNORECASE)],
    "read": [re.compile(r"\b(?:lux|adc|counter|Status:\s*OK|CRC_ERROR)\b", re.IGNORECASE)],
    "lux": [re.compile(r"\b(?:lux|Status:\s*OK|CRC_ERROR)\b", re.IGNORECASE)],
    "mlux": [re.compile(r"\b(?:milli-lux|Status:\s*OK|CRC_ERROR)\b", re.IGNORECASE)],
    "ulux": [re.compile(r"\b(?:micro-lux|Status:\s*OK|CRC_ERROR)\b", re.IGNORECASE)],
    "flags": [re.compile(r"\b(?:flags|status|ready|window|Status:\s*OK)\b", re.IGNORECASE)],
    "selftest": [re.compile(r"\b(?:selftest|pass|fail|skip|Status:\s*OK)\b", re.IGNORECASE)],
    "readburst": [re.compile(r"\b(?:burst|fifo|counter|CRC|Status:\s*OK)\b", re.IGNORECASE)],
    "burst": [re.compile(r"\b(?:burst|fifo|Status:\s*OK)\b", re.IGNORECASE)],
    "fifo": [re.compile(r"\b(?:fifo|counter|CRC|Status:\s*OK)\b", re.IGNORECASE)],
    "slot": [re.compile(r"\b(?:sample|counter|CRC|Status:\s*OK)\b", re.IGNORECASE)],
    "stress": [re.compile(r"\b(?:stress|success|fail|Status:\s*OK)\b", re.IGNORECASE)],
    "stress_mix": [re.compile(r"\b(?:stress|success|fail|Status:\s*OK)\b", re.IGNORECASE)],
    "recover": [re.compile(r"\b(?:recover|Status:\s*OK|READY)\b", re.IGNORECASE)],
    "resetreapply": [re.compile(r"\b(?:reset|reapply|Status:\s*OK|READY)\b", re.IGNORECASE)],
}


@dataclass
class CommandResult:
    command: str
    output: str
    status: str
    reason: str
    duration_s: float


def git_value(args: list[str]) -> str:
    try:
        return subprocess.check_output(["git", *args], cwd=ROOT, text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def import_serial():
    try:
        import serial  # type: ignore
    except ImportError:
        print("pyserial is required. Install it with: python -m pip install pyserial")
        raise SystemExit(2)
    return serial


def command_key(command: str) -> str:
    return command.strip().split(maxsplit=1)[0].lower()


def command_groups(cli: str, stress_count: int, benchmark_command: str,
                   benchmark_count: int) -> dict[str, list[str]]:
    benchmark = [benchmark_command for _ in range(benchmark_count)]
    if cli == "arduino":
        ctime = [item for value in ARDUINO_CTIME_VALUES for item in (f"ctime {value}", "read")]
        return {
            "smoke": ["version", "scan", "probe", "id", "cfg", "state", "read", "lux", "selftest"],
            "ctime": ctime,
            "stress": [f"stress {stress_count}", f"stress_mix {stress_count}"],
            "benchmark": benchmark,
            "status": ["flags", "status_raw"],
            "fifo": ["measure auto 8 cont 1", "burst 1", "readburst", "slot 0", "slot 1", "slot 2", "slot 3"],
            "int": ["int latch 1", "int pol low", "int faults 1", "threshold 1 1000", "int th 1 1000", "intpin", "flags", "int ready", "int fifo"],
            "fault": ["drv", "state", "probe", "recover", "resetreapply", "cfg", "selftest"],
        }

    ctime = [item for value in IDF_CTIME_VALUES for item in (f"ctime {value}", "read")]
    return {
        "smoke": ["version", "scan", "probe", "id", "cfg", "drv", "read", "lux", "selftest"],
        "ctime": ctime,
        "stress": [f"stress {stress_count}", f"stress_mix {stress_count}"],
        "benchmark": benchmark,
        "status": ["flags", "status_raw"],
        "fifo": ["range 12", "mode 3", "ready", "burst", "fifo"],
        "int": ["latch 1", "pol 0", "fault 0", "threshold lux 1 1000", "int threshold", "int pin", "flags", "int ready", "int fifo"],
        "fault": ["drv", "probe", "recover", "resetreapply", "cfg", "selftest"],
    }


def selected_commands(args: argparse.Namespace) -> list[str]:
    groups = command_groups(args.cli, args.stress_count,
                            args.benchmark_command, args.benchmark_count)
    selected = list(args.group) if args.group else ["smoke"]
    if "all-safe" in selected:
        selected = ["smoke", "ctime", "stress", "fifo"]

    commands: list[str] = []
    for group in selected:
        if group in ("int", "fault"):
            continue
        commands.extend(groups[group])

    if args.include_int:
        commands.extend(groups["int"])
    if args.include_fault:
        commands.extend(groups["fault"])
    return commands


def classify_output(output: str, command: str | None = None,
                    strict_expected: bool = False) -> tuple[str, str]:
    for pattern in FAIL_PATTERNS:
        match = pattern.search(output)
        if match is not None:
            return "FAIL", match.group(0)
    for pattern in WARN_PATTERNS:
        match = pattern.search(output)
        if match is not None:
            if (strict_expected and command is not None and
                    command_key(command) in DATA_COMMANDS and
                    "NOT_READY" in match.group(0).upper()):
                return "UNKNOWN", match.group(0)
            return "WARN", match.group(0)
    if strict_expected and command is not None:
        patterns = EXPECTED_PATTERNS.get(command_key(command), [])
        if patterns and not any(pattern.search(output) for pattern in patterns):
            return "UNKNOWN", "missing expected response token"
    return "PASS", "response captured"


def read_response(ser, timeout_s: float, idle_s: float) -> str:
    deadline = time.monotonic() + timeout_s
    last_data = time.monotonic()
    chunks: list[bytes] = []

    while time.monotonic() < deadline:
        waiting = getattr(ser, "in_waiting", 0)
        data = ser.read(waiting or 1)
        if data:
            chunks.append(data)
            last_data = time.monotonic()
            continue
        if chunks and (time.monotonic() - last_data) >= idle_s:
            break

    return b"".join(chunks).decode("utf-8", errors="replace")


def command_timeout(args: argparse.Namespace, command: str) -> float:
    if command.startswith("stress"):
        return args.stress_timeout
    return args.command_timeout


ANSI_RE = re.compile(r"\x1B\[[0-?]*[ -/]*[@-~]")


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text)


def run_command(ser, command: str, timeout_s: float, idle_s: float,
                strict_expected: bool = False) -> CommandResult:
    started = time.monotonic()
    ser.write((command + "\n").encode("utf-8"))
    ser.flush()
    output = read_response(ser, timeout_s=timeout_s, idle_s=idle_s)
    duration = time.monotonic() - started
    if output == "":
        return CommandResult(command, output, "FAIL", "no response before timeout", duration)
    status, reason = classify_output(strip_ansi(output), command, strict_expected)
    return CommandResult(command, output, status, reason, duration)


def result_counts(results: Iterable[CommandResult]) -> dict[str, int]:
    counts = {"PASS": 0, "WARN": 0, "FAIL": 0, "UNKNOWN": 0, "NOT_RUN": 0}
    for result in results:
        counts[result.status] = counts.get(result.status, 0) + 1
    return counts


def write_log(path: pathlib.Path, args: argparse.Namespace,
              results: Iterable[CommandResult], boot_output: str = "") -> None:
    results = list(results)
    counts = result_counts(results)
    commit = git_value(["rev-parse", "--short", "HEAD"])
    dirty = "yes" if git_value(["status", "--short"]) else "no"
    lines = [
        "# OPT4001 HIL Runner Log",
        "",
        f"timestamp: {dt.datetime.now(dt.timezone.utc).isoformat()}",
        f"git_commit: {commit}",
        f"git_dirty: {dirty}",
        f"operator: {args.operator or ''}",
        f"board: {args.board or ''}",
        f"target: {args.target or ''}",
        f"package: {args.package or ''}",
        f"address: {args.address or ''}",
        f"port: {args.port}",
        f"baud: {args.baud}",
        f"cli: {args.cli}",
        f"groups: {', '.join(args.group or ['smoke'])}",
        f"include_int: {args.include_int}",
        f"include_fault: {args.include_fault}",
        f"strict_expected: {args.strict_expected}",
        "",
        "## Boot Transcript",
        "",
        "```text",
        boot_output.rstrip(),
        "```",
        "",
        "## Summary",
        "",
        "| PASS | WARN | FAIL | UNKNOWN | NOT_RUN |",
        "| ---: | ---: | ---: | ---: | ---: |",
        f"| {counts['PASS']} | {counts['WARN']} | {counts['FAIL']} | {counts['UNKNOWN']} | {counts['NOT_RUN']} |",
        "",
        "| Command | Status | Reason | Duration (s) |",
        "| --- | --- | --- | ---: |",
    ]

    for result in results:
        lines.append(
            f"| `{result.command}` | {result.status} | {result.reason} | {result.duration_s:.3f} |"
        )

    lines.append("")

    for result in results:
        lines.extend([
            f"## {result.command}",
            "",
            f"status: {result.status}",
            f"reason: {result.reason}",
            f"duration_s: {result.duration_s:.3f}",
            "",
            "```text",
            result.output.rstrip(),
            "```",
            "",
        ])

    path.write_text("\n".join(lines), encoding="utf-8")

    json_path = path.with_suffix(".json")
    json_path.write_text(
        json.dumps(
            {
                "timestamp": dt.datetime.now(dt.timezone.utc).isoformat(),
                "git_commit": commit,
                "git_dirty": dirty,
                "operator": args.operator,
                "board": args.board,
                "target": args.target,
                "package": args.package,
                "address": args.address,
                "port": args.port,
                "baud": args.baud,
                "cli": args.cli,
                "groups": args.group or ["smoke"],
                "include_int": args.include_int,
                "include_fault": args.include_fault,
                "strict_expected": args.strict_expected,
                "boot_output": boot_output,
                "counts": counts,
                "results": [result.__dict__ for result in results],
            },
            indent=2,
        ),
        encoding="utf-8",
    )


def parser_self_test() -> bool:
    cases = [
        ("Status: DEVICE_NOT_FOUND", None, False, "FAIL"),
        ("Status: I2C_TIMEOUT", None, False, "FAIL"),
        ("Status: CRC_ERROR", None, False, "WARN"),
        ("selftest fail=0 OK", None, False, "PASS"),
        ("DEVICE_ID raw=0x0121 didh=0x121 match=true", "id", True, "PASS"),
        ("unrelated prompt text", "probe", True, "UNKNOWN"),
    ]
    ok = True
    for text, command, strict, expected in cases:
        status, reason = classify_output(strip_ansi(text), command, strict)
        if status != expected:
            print(
                f"parser self-test failed: text={text!r} command={command!r} "
                f"strict={strict} expected={expected} got={status} reason={reason}"
            )
            ok = False

    args = parse_args(["--dry-run"])
    commands = selected_commands(args)
    for required in ("version", "scan", "probe", "id", "cfg", "state"):
        if required not in commands:
            print(f"parser self-test failed: missing dry-run command {required}")
            ok = False
    return ok


def positive_float(value: str) -> float:
    parsed = float(value)
    if parsed <= 0.0:
        raise argparse.ArgumentTypeError("value must be > 0")
    return parsed


def non_negative_float(value: str) -> float:
    parsed = float(value)
    if parsed < 0.0:
        raise argparse.ArgumentTypeError("value must be >= 0")
    return parsed


def positive_int(value: str) -> int:
    parsed = int(value, 0)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("value must be > 0")
    return parsed


def non_negative_int(value: str) -> int:
    parsed = int(value, 0)
    if parsed < 0:
        raise argparse.ArgumentTypeError("value must be >= 0")
    return parsed


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Bounded serial HIL runner for the OPT4001 diagnostic CLI."
    )
    parser.add_argument("--port", help="Serial port, for example COM6 or /dev/ttyUSB0.")
    parser.add_argument("--baud", type=positive_int, default=115200, help="Serial baud rate.")
    parser.add_argument("--cli", choices=("arduino", "idf"), default="arduino", help="CLI command dialect.")
    parser.add_argument(
        "--group",
        choices=("all-safe", "smoke", "ctime", "stress", "benchmark", "fifo", "status"),
        action="append",
        default=None,
        help="Command group to run. Repeat for multiple groups.",
    )
    parser.add_argument("--include-int", action="store_true", help="Run SOT-5X3 INT commands; disabled by default.")
    parser.add_argument(
        "--include-fault",
        action="store_true",
        help="Run recovery/reset commands. Requires operator confirmation because reset is bus-wide.",
    )
    parser.add_argument(
        "--confirm-faults",
        default="",
        help="Required exact value I_ACCEPT_BUS_RESET_RISK when --include-fault is used.",
    )
    parser.add_argument("--stress-count", type=positive_int, default=10, help="Read count for stress groups.")
    parser.add_argument("--stress-duration-s", type=non_negative_float, default=0.0, help="Optional overall timeout for stress sessions; 0 disables.")
    parser.add_argument("--benchmark-count", type=positive_int, default=50, help="Command count for the benchmark group.")
    parser.add_argument("--benchmark-command", default="read", help="CLI command repeated by the benchmark group.")
    parser.add_argument("--command-timeout", "--timeout-s", dest="command_timeout", type=positive_float, default=5.0, help="Seconds to wait per command.")
    parser.add_argument("--stress-timeout", type=positive_float, default=30.0, help="Seconds to wait for each stress command.")
    parser.add_argument("--overall-timeout", type=non_negative_float, default=0.0, help="Abort after this many seconds; 0 disables.")
    parser.add_argument("--idle-timeout", type=non_negative_float, default=0.25, help="Response idle seconds before next command.")
    parser.add_argument("--boot-wait", type=non_negative_float, default=2.0, help="Seconds to wait after opening serial.")
    parser.add_argument("--reconnect-attempts", type=non_negative_int, default=0, help="Bounded serial reopen attempts after no-response commands.")
    parser.add_argument("--reconnect-wait", type=non_negative_float, default=1.0, help="Seconds to wait before each reconnect attempt.")
    parser.add_argument("--log-dir", default=str(ROOT / "hil_logs"), help="Directory for transcript logs.")
    parser.add_argument("--operator", default="", help="Operator name recorded in the log.")
    parser.add_argument("--board", default="", help="Board name recorded in the log.")
    parser.add_argument("--target", default="", help="MCU target recorded in the log.")
    parser.add_argument("--package", default="", help="Sensor package recorded in the log.")
    parser.add_argument("--address", default="", help="Sensor address/wiring recorded in the log.")
    parser.add_argument("--strict-expected", action="store_true", help="Return UNKNOWN when known commands lack expected response tokens.")
    parser.add_argument("--verbose", action="store_true", help="Echo captured command transcripts to stdout.")
    parser.add_argument("--parser-self-test", action="store_true", help="Run classifier and dry-run self-tests without opening serial.")
    parser.add_argument("--dry-run", action="store_true", help="Print selected commands without opening serial.")
    parser.add_argument("--list-commands", action="store_true", help="List selected commands and exit.")
    return parser.parse_args(argv)


def open_serial(serial, args: argparse.Namespace):
    return serial.Serial(args.port, args.baud, timeout=0.1, write_timeout=1.0)


def drain_boot(ser, args: argparse.Namespace) -> str:
    if args.boot_wait > 0.0:
        time.sleep(args.boot_wait)
    return read_response(ser, timeout_s=0.5, idle_s=args.idle_timeout)


def run_with_reconnect(serial, args: argparse.Namespace, ser, command: str) -> tuple[object, CommandResult]:
    result = run_command(
        ser, command, command_timeout(args, command), args.idle_timeout,
        args.strict_expected
    )
    attempts = 0
    while result.reason == "no response before timeout" and attempts < args.reconnect_attempts:
        attempts += 1
        print(f"  reconnect attempt {attempts}/{args.reconnect_attempts}")
        ser.close()
        if args.reconnect_wait > 0.0:
            time.sleep(args.reconnect_wait)
        try:
            ser = open_serial(serial, args)
            _ = drain_boot(ser, args)
        except Exception as exc:  # pyserial raises platform-specific exceptions.
            return ser, CommandResult(
                command, "", "FAIL", f"serial reconnect failed: {exc}", 0.0
            )
        result = run_command(
            ser, command, command_timeout(args, command), args.idle_timeout,
            args.strict_expected
        )
        result.reason = f"{result.reason} after reconnect attempt {attempts}"
    return ser, result


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.parser_self_test:
        if parser_self_test():
            print("Parser self-test PASSED")
            return 0
        return 1

    if args.include_fault:
        if args.confirm_faults != "I_ACCEPT_BUS_RESET_RISK":
            print("Refusing --include-fault without --confirm-faults I_ACCEPT_BUS_RESET_RISK")
            return 2
        print("Fault/reset group enabled. Confirmed by operator flag.")

    commands = selected_commands(args)
    if not commands:
        print("No commands selected.")
        return 2

    if args.list_commands or args.dry_run:
        for command in commands:
            print(command)
        return 0

    if not args.port:
        print("--port is required unless --dry-run or --list-commands is used.")
        return 2

    if args.stress_duration_s > 0.0 and args.overall_timeout == 0.0:
        args.overall_timeout = args.stress_duration_s

    serial = import_serial()
    log_dir = pathlib.Path(args.log_dir)
    log_dir.mkdir(parents=True, exist_ok=True)
    timestamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = log_dir / f"opt4001_hil_{args.cli}_{timestamp}.md"

    results: list[CommandResult] = []
    ser = open_serial(serial, args)
    boot_output = ""
    try:
        boot_output = drain_boot(ser, args)
        if args.verbose and boot_output:
            print("=== boot transcript ===")
            print(boot_output.rstrip())
        started = time.monotonic()
        for command in commands:
            if args.overall_timeout > 0 and (time.monotonic() - started) >= args.overall_timeout:
                results.append(CommandResult(command, "", "FAIL", "overall timeout reached", 0.0))
                break
            print(f"> {command}")
            ser, result = run_with_reconnect(serial, args, ser, command)
            results.append(result)
            print(f"  {result.status}: {result.reason}")
            if args.verbose and result.output:
                print(result.output.rstrip())
    finally:
        ser.close()

    write_log(log_path, args, results, boot_output)

    counts = result_counts(results)

    print("")
    print(f"Log: {log_path}")
    print(
        f"Summary: PASS={counts['PASS']} WARN={counts['WARN']} "
        f"FAIL={counts['FAIL']} UNKNOWN={counts['UNKNOWN']} NOT_RUN={counts['NOT_RUN']}"
    )
    return 1 if counts["FAIL"] or counts["UNKNOWN"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
