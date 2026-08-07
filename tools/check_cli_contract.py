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
    "CommandHandler.h",
    "TransportAdapter.h",
    "BusDiag.h",
    "CliShell.h",
    "CliStyle.h",
    "HealthView.h",
    "HealthDiag.h",
]

MANDATORY_COMMANDS = [
    "help",
    "version",
    "ver",
    "scan",
    "init",
    "begin",
    "end",
    "addr",
    "pkg",
    "probe",
    "recover",
    "drv",
    "health",
    "state",
    "read",
    "readblocking",
    "tryread",
    "trylux",
    "start",
    "poll",
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

    for cmd in MANDATORY_COMMANDS:
        if re.search(rf"\b{re.escape(cmd)}\b", arduino_text) is None:
            fail(f"Arduino CLI missing mandatory command '{cmd}'")
        if f'"{cmd}"' not in idf_text:
            fail(f"IDF CLI missing mandatory command '{cmd}'")

    for cli_name, source in (("Arduino", arduino_text), ("IDF", idf_text)):
        for token in ("errorName", "driverStateName"):
            if token not in source:
                fail(f"{cli_name} CLI must reuse library-owned {token} mapping")

    for token in ("\\033[31m", "\\033[32m", "\\033[33m", "\\033[36m"):
        if token not in idf_text:
            fail(f"IDF CLI missing ANSI color token {token!r}")

    for token in ("STRESS_COUNT_MAX", "WATCH_COUNT_MAX", "REGISTER_DUMP_MAX_BYTES"):
        if token not in idf_text:
            fail(f"IDF CLI missing bounded diagnostic limit '{token}'")

    for token in ("driver/i2c_master.h", "i2c_master_probe", "getchar()", "char input["):
        if token not in idf_text:
            fail(f"native ESP-IDF CLI missing '{token}'")

    for token in ("Opt4001IdfI2cTransport.cpp", "esp_driver_i2c", "esp_timer"):
        if token not in cmake_text:
            fail(f"ESP-IDF CMake missing '{token}'")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
