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
    re.compile(r"\bTIMEOUT\b", re.IGNORECASE),
    re.compile(r"\bNOT_INITIALIZED\b", re.IGNORECASE),
    re.compile(r"\bINVALID_(?:CONFIG|PARAM)\b", re.IGNORECASE),
    re.compile(r"\bBUSY\b", re.IGNORECASE),
    re.compile(r"\bfail(?:ed|ures)?[=: ]+[1-9]\d*\b", re.IGNORECASE),
]

WARN_PATTERNS = [
    re.compile(r"\bCRC_ERROR\b", re.IGNORECASE),
    re.compile(r"\bMEASUREMENT_NOT_READY\b", re.IGNORECASE),
]


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


def command_groups(cli: str, stress_count: int) -> dict[str, list[str]]:
    if cli == "arduino":
        ctime = [item for value in ARDUINO_CTIME_VALUES for item in (f"ctime {value}", "read")]
        return {
            "smoke": ["version", "scan", "probe", "id", "cfg", "state", "read", "lux", "flags", "selftest"],
            "ctime": ctime,
            "stress": [f"stress {stress_count}", f"stress_mix {stress_count}"],
            "fifo": ["measure auto 8 cont 1", "burst 1", "readburst", "slot 0", "slot 1", "slot 2", "slot 3"],
            "int": ["int latch 1", "int pol low", "int faults 1", "threshold 1 1000", "int th 1 1000", "intpin", "flags", "int ready", "int fifo"],
            "fault": ["drv", "state", "probe", "recover", "resetreapply", "cfg", "selftest"],
        }

    ctime = [item for value in IDF_CTIME_VALUES for item in (f"ctime {value}", "read")]
    return {
        "smoke": ["version", "scan", "probe", "id", "cfg", "drv", "read", "lux", "flags", "selftest"],
        "ctime": ctime,
        "stress": [f"stress {stress_count}", f"stress_mix {stress_count}"],
        "fifo": ["range 12", "mode 3", "ready", "burst", "fifo"],
        "int": ["latch 1", "pol 0", "fault 0", "threshold lux 1 1000", "int threshold", "int pin", "flags", "int ready", "int fifo"],
        "fault": ["drv", "probe", "recover", "resetreapply", "cfg", "selftest"],
    }


def selected_commands(args: argparse.Namespace) -> list[str]:
    groups = command_groups(args.cli, args.stress_count)
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


def classify_output(output: str) -> tuple[str, str]:
    for pattern in FAIL_PATTERNS:
        match = pattern.search(output)
        if match is not None:
            return "FAIL", match.group(0)
    for pattern in WARN_PATTERNS:
        match = pattern.search(output)
        if match is not None:
            return "WARN", match.group(0)
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


def run_command(ser, command: str, timeout_s: float, idle_s: float) -> CommandResult:
    started = time.monotonic()
    ser.write((command + "\n").encode("utf-8"))
    ser.flush()
    output = read_response(ser, timeout_s=timeout_s, idle_s=idle_s)
    duration = time.monotonic() - started
    if output == "":
        return CommandResult(command, output, "FAIL", "no response before timeout", duration)
    status, reason = classify_output(strip_ansi(output))
    return CommandResult(command, output, status, reason, duration)


def write_log(path: pathlib.Path, args: argparse.Namespace, results: Iterable[CommandResult]) -> None:
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
        "",
    ]

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
                "results": [result.__dict__ for result in results],
            },
            indent=2,
        ),
        encoding="utf-8",
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Bounded serial HIL runner for the OPT4001 diagnostic CLI."
    )
    parser.add_argument("--port", help="Serial port, for example COM6 or /dev/ttyUSB0.")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate.")
    parser.add_argument("--cli", choices=("arduino", "idf"), default="arduino", help="CLI command dialect.")
    parser.add_argument(
        "--group",
        choices=("all-safe", "smoke", "ctime", "stress", "fifo"),
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
    parser.add_argument("--stress-count", type=int, default=10, help="Read count for stress groups.")
    parser.add_argument("--command-timeout", type=float, default=5.0, help="Seconds to wait per command.")
    parser.add_argument("--stress-timeout", type=float, default=30.0, help="Seconds to wait for each stress command.")
    parser.add_argument("--overall-timeout", type=float, default=0.0, help="Abort after this many seconds; 0 disables.")
    parser.add_argument("--idle-timeout", type=float, default=0.25, help="Response idle seconds before next command.")
    parser.add_argument("--boot-wait", type=float, default=2.0, help="Seconds to wait after opening serial.")
    parser.add_argument("--log-dir", default=str(ROOT / "hil_logs"), help="Directory for transcript logs.")
    parser.add_argument("--operator", default="", help="Operator name recorded in the log.")
    parser.add_argument("--board", default="", help="Board name recorded in the log.")
    parser.add_argument("--target", default="", help="MCU target recorded in the log.")
    parser.add_argument("--package", default="", help="Sensor package recorded in the log.")
    parser.add_argument("--address", default="", help="Sensor address/wiring recorded in the log.")
    parser.add_argument("--dry-run", action="store_true", help="Print selected commands without opening serial.")
    parser.add_argument("--list-commands", action="store_true", help="List selected commands and exit.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
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

    serial = import_serial()
    log_dir = pathlib.Path(args.log_dir)
    log_dir.mkdir(parents=True, exist_ok=True)
    timestamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = log_dir / f"opt4001_hil_{args.cli}_{timestamp}.md"

    results: list[CommandResult] = []
    with serial.Serial(args.port, args.baud, timeout=0.1, write_timeout=1.0) as ser:
        time.sleep(args.boot_wait)
        _ = read_response(ser, timeout_s=0.5, idle_s=args.idle_timeout)
        started = time.monotonic()
        for command in commands:
            if args.overall_timeout > 0 and (time.monotonic() - started) >= args.overall_timeout:
                results.append(CommandResult(command, "", "FAIL", "overall timeout reached", 0.0))
                break
            print(f"> {command}")
            result = run_command(ser, command, command_timeout(args, command), args.idle_timeout)
            results.append(result)
            print(f"  {result.status}: {result.reason}")

    write_log(log_path, args, results)

    counts = {"PASS": 0, "WARN": 0, "FAIL": 0}
    for result in results:
        counts[result.status] = counts.get(result.status, 0) + 1

    print("")
    print(f"Log: {log_path}")
    print(f"Summary: PASS={counts['PASS']} WARN={counts['WARN']} FAIL={counts['FAIL']}")
    return 1 if counts["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
