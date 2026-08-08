#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_COMMON = [
    "BoardConfig.h",
    "BuildConfig.h",
    "Log.h",
    "I2cTransport.h",
    "I2cScanner.h",
    "TransportAdapter.h",
    "BusDiag.h",
    "CliShell.h",
    "CliLineBuffer.h",
    "CliText.h",
    "CliStyle.h",
    "HealthView.h",
    "HealthDiag.h",
]

MANDATORY_COMMANDS = [
    "help",
    "version",
    "ver",
    "scan",
    "discover",
    "color",
    "init",
    "begin",
    "end",
    "bind",
    "unbind",
    "attach",
    "powerdown",
    "addr",
    "pkg",
    "probe",
    "recover",
    "drv",
    "health",
    "healthmon",
    "state",
    "read",
    "readblocking",
    "tryread",
    "trylux",
    "start",
    "poll",
    "job",
    "drdy",
    "readburst",
    "slot",
    "sample",
    "sampleage",
    "lux",
    "mlux",
    "ulux",
    "watch",
    "stop",
    "diag",
    "scale",
    "timing",
    "cfg",
    "settings",
    "snapshot",
    "range",
    "ctime",
    "mode",
    "measure",
    "qwake",
    "crc",
    "burst",
    "config",
    "intcfg",
    "status",
    "flags",
    "threshold",
    "thcalc",
    "thdecode",
    "int",
    "intpin",
    "id",
    "identify",
    "dump",
    "reg",
    "wreg",
    "regs",
    "raw",
    "raw2lux",
    "adc2lux",
    "reset",
    "resetreapply",
    "selftest",
    "selfcheck",
    "verbose",
    "stress",
    "stress_mix",
]


def fail(msg: str) -> None:
    print(f"CLI contract FAILED: {msg}")
    raise SystemExit(1)


def ensure_exists(path: pathlib.Path, label: str) -> None:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")


def ensure_missing(path: pathlib.Path, label: str) -> None:
    if path.exists():
        fail(f"forbidden {label} still present: {path.as_posix()}")


def main() -> int:
    common_dir = ROOT / "examples" / "common"
    arduino_cli = ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp"
    idf_main_dir = ROOT / "examples" / "esp_idf" / "basic" / "main"
    idf_main = idf_main_dir / "main.cpp"
    idf_cmake = idf_main_dir / "CMakeLists.txt"

    ensure_exists(common_dir, "common example directory")
    ensure_exists(arduino_cli, "Arduino bring-up CLI")
    ensure_exists(idf_main, "native ESP-IDF CLI")
    ensure_exists(idf_cmake, "ESP-IDF example component CMake")

    for stale in ("Arduino.h", "Wire.h", "Opt4001IdfArduinoShim.cpp"):
        ensure_missing(idf_main_dir / stale, f"ESP-IDF compatibility file {stale}")

    for name in REQUIRED_COMMON:
        ensure_exists(common_dir / name, f"common helper {name}")

    arduino_text = arduino_cli.read_text(encoding="utf-8", errors="replace")
    idf_text = idf_main.read_text(encoding="utf-8", errors="replace")
    cmake_text = idf_cmake.read_text(encoding="utf-8", errors="replace")

    arduino_dispatch = arduino_text[arduino_text.find("void processCommand") :]
    idf_dispatch = idf_text[idf_text.find("void processCommand") :]
    for cmd in MANDATORY_COMMANDS:
        arduino_handler = re.search(
            rf'cmd\s*(?:==\s*"{re.escape(cmd)}"|\.startsWith\("{re.escape(cmd)}(?:\s|"))',
            arduino_dispatch,
        )
        if arduino_handler is None:
            fail(f"Arduino CLI missing mandatory command '{cmd}'")
        if re.search(rf'strcmp\(cmd,\s*"{re.escape(cmd)}"\)', idf_dispatch) is None:
            fail(f"IDF CLI missing mandatory command '{cmd}'")

    arduino_help = arduino_text[arduino_text.find("void printHelp()") :
                                arduino_text.find("void processCommand")]
    idf_help = idf_text[idf_text.find("void printHelp()") :
                        idf_text.find("void printHealth()")]
    for cmd in MANDATORY_COMMANDS:
        if f'"{cmd}' not in arduino_help and f'/ {cmd}' not in arduino_help:
            fail(f"Arduino help does not expose mandatory command '{cmd}'")
        if f'"{cmd}' not in idf_help and f'/ {cmd}' not in idf_help:
            fail(f"IDF help does not expose mandatory command '{cmd}'")

    for cli_name, source in (("Arduino", arduino_text), ("IDF", idf_text)):
        for token in ("errorName", "driverStateName"):
            if token not in source:
                fail(f"{cli_name} CLI must reuse library-owned {token} mapping")
        if "conversionStartAccepted" not in source:
            fail(f"{cli_name} CLI must accept the driver's IN_PROGRESS conversion-start contract")
        if re.search(
            r"startConversion\s*\([^;]*;\s*if\s*\(\s*!\s*\w+\.ok\(\)",
            source,
            flags=re.DOTALL,
        ):
            fail(f"{cli_name} CLI incorrectly treats IN_PROGRESS conversion start as failure")

    for token in ("waitForFreshSampleReady", 'cmd == "raw"', 'cmd == "clearflags"'):
        if token not in arduino_dispatch:
            fail(f"Arduino CLI missing executable readiness/parity token '{token}'")

    for token in ("startStressSession(count", "runSelfTest()", "readLatestSample(sample)"):
        if token not in idf_dispatch and token != "readLatestSample(sample)":
            fail(f"IDF CLI missing executable depth token '{token}'")
        if token == "readLatestSample(sample)" and token not in idf_text:
            fail("IDF raw command must use latest-register semantics")

    for token in ("\\033[31m", "\\033[32m", "\\033[33m", "\\033[36m"):
        if token not in idf_text:
            fail(f"IDF CLI missing ANSI color token {token!r}")

    for token in ("STRESS_COUNT_MAX", "WATCH_COUNT_MAX", "REGISTER_DUMP_MAX_BYTES"):
        if token not in idf_text:
            fail(f"IDF CLI missing bounded diagnostic limit '{token}'")

    for token in ("driver/i2c_master.h", "i2c_master_probe", "getchar()", "char input["):
        if token not in idf_text:
            fail(f"native ESP-IDF CLI missing '{token}'")

    for cli_name, source in (("Arduino", arduino_text), ("IDF", idf_text)):
        for token in (
            "FixedLineBuffer", "LineResult::TOO_LONG", "complete line discarded",
            "startAttach", "startReadSample", "startReadBurst",
            "startConfigureMeasurement", "startResetAndReapply",
            "cancelPollJob", "lastPollStatus", "getLastBurst",
            "startStressSession", "serviceStress", "poll(",
        ):
            if token not in source:
                fail(f"{cli_name} CLI missing executable owner/CLI token '{token}'")
        if re.search(r"\bString\b", source):
            fail(f"{cli_name} CLI must not allocate Arduino String for command parsing")
        if re.search(r"void\s+runStress(?:Mix)?\s*\(", source):
            fail(f"{cli_name} CLI retains superseded blocking stress implementation")
        for local_mapping in ("errToStr", "stateToStr", "errToString", "stateToString"):
            if re.search(rf"\b{local_mapping}\s*\(", source):
                fail(f"{cli_name} CLI retains duplicate enum-name helper '{local_mapping}'")

    if "snap.bound" not in arduino_text or "s.bound" not in idf_text:
        fail("both settings commands must expose bus-binding state")

    for token in ("DEVICE_ID", "candidate.bind", "candidate.probe"):
        if token not in arduino_text:
            fail(f"Arduino discover must be protocol-qualified: missing '{token}'")
    for token in ("DEVICE_ID", "sensor.bind", "sensor.probe", "i2c_master_bus_rm_device"):
        if token not in idf_text:
            fail(f"IDF discover must be protocol-qualified and clean up handles: missing '{token}'")

    for token in ("healthmon", "serviceHealthMonitor", "colorEnabled"):
        if token not in idf_text:
            fail(f"IDF parity missing '{token}'")

    for cli_name, help_text in (("Arduino", arduino_help), ("IDF", idf_help)):
        if '"stop", "Stop active watch/stress session"' not in help_text:
            fail(f"{cli_name} stop help must cover both cooperative session types")

    for token in ("Opt4001IdfI2cTransport.cpp", "esp_driver_i2c", "esp_timer"):
        if token not in cmake_text:
            fail(f"ESP-IDF CMake missing '{token}'")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
