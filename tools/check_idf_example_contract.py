#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
IDF_MAIN = ROOT / "examples" / "esp_idf" / "basic" / "main"

MANDATORY_COMMANDS = [
    "help",
    "version",
    "ver",
    "scan",
    "verbose",
    "init",
    "begin",
    "end",
    "drv",
    "online",
    "probe",
    "recover",
    "reset",
    "resetreapply",
    "read",
    "lux",
    "mlux",
    "ulux",
    "sample",
    "sampleage",
    "start",
    "ready",
    "flags",
    "status",
    "status_raw",
    "flags_raw",
    "clearflags",
    "burst",
    "fifo",
    "cfg",
    "settings",
    "snapshot",
    "range",
    "ctime",
    "mode",
    "quickwake",
    "crc",
    "latch",
    "pol",
    "fault",
    "int",
    "threshold",
    "id",
    "config",
    "intcfg",
    "reg",
    "rreg",
    "wreg",
    "regs",
    "raw",
    "scale",
    "diag",
    "selftest",
    "stress",
    "stress_mix",
    "watch",
    "demo",
]

REQUIRED_IDF_TOKENS = [
    'extern "C" void app_main',
    '#include "driver/i2c_master.h"',
    "i2c_new_master_bus",
    "i2c_master_bus_add_device",
    "i2c_master_probe",
    "Opt4001IdfI2cTransport.cpp",
    "opt4001IdfI2cWrite",
    "opt4001IdfI2cWriteRead",
    "opt4001IdfNowMs",
    "vTaskDelay",
    "getchar()",
    "char input[",
]

FORBIDDEN_PATTERNS = [
    r"ArduinoCompat",
    r"IdfArduinoCompat",
    r"Arduino\.h",
    r"Wire\.h",
    r"\bString\b",
    r"\bSerial\b",
    r"\bTwoWire\b",
    r"01_basic_bringup_cli/main\.cpp",
    r"driver/i2c\.h",
    r"i2c_cmd_link",
    r"i2c_driver_install",
    r"esp_driver_uart",
]


def fail(msg: str) -> None:
    print(f"IDF example contract FAILED: {msg}")
    raise SystemExit(1)


def read(path: pathlib.Path, label: str) -> str:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")
    return path.read_text(encoding="utf-8", errors="replace")


def main() -> int:
    main_text = read(IDF_MAIN / "main.cpp", "native ESP-IDF main")
    cmake_text = read(IDF_MAIN / "CMakeLists.txt", "ESP-IDF CMake")
    transport_text = read(IDF_MAIN / "Opt4001IdfI2cTransport.cpp", "native transport")
    combined = "\n".join([main_text, cmake_text, transport_text])

    for token in REQUIRED_IDF_TOKENS:
        if token not in combined:
            fail(f"required native ESP-IDF token missing: {token}")

    for component in ("OPT4001", "esp_driver_i2c", "esp_driver_gpio", "esp_timer", "freertos"):
        if re.search(rf"\b{re.escape(component)}\b", cmake_text) is None:
            fail(f"ESP-IDF CMake missing required component '{component}'")

    for pattern in FORBIDDEN_PATTERNS:
        if re.search(pattern, combined):
            fail(f"forbidden Arduino/legacy token present: {pattern}")

    for stale in ("Arduino.h", "Wire.h", "Opt4001IdfArduinoShim.cpp"):
        if (IDF_MAIN / stale).exists():
            fail(f"stale compatibility file remains: {stale}")

    for command in MANDATORY_COMMANDS:
        if re.search(rf'"{re.escape(command)}"', main_text) is None:
            fail(f"native CLI missing command '{command}'")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
