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
    "scan",
    "probe",
    "recover",
    "drv",
    "state",
    "read",
    "addr",
    "diag",
    "healthmon",
    "scale",
    "adc2lux",
    "raw2lux",
    "tryread",
    "trylux",
    "measure",
    "slot",
    "config",
    "intcfg",
    "status",
    "flags",
    "thcalc",
    "thdecode",
    "regs",
    "reset",
    "resetreapply",
    "watch",
    "stop",
    "selftest",
    "verbose",
    "stress",
    "stress_mix",
]

MANDATORY_SUBSTRINGS = [
    "threshold default",
    "int ready",
    "int fifo",
    "int th ",
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
    bringup_main = ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp"
    idf_main_dir = ROOT / "examples" / "esp_idf" / "basic" / "main"
    idf_cmake = idf_main_dir / "CMakeLists.txt"
    idf_wrapper = idf_main_dir / "main.cpp"
    idf_arduino = idf_main_dir / "Arduino.h"
    idf_wire = idf_main_dir / "Wire.h"
    idf_shim = idf_main_dir / "Opt4001IdfArduinoShim.cpp"

    ensure_exists(common_dir, "common example directory")
    ensure_exists(bringup_main, "bringup CLI example")
    ensure_exists(idf_cmake, "ESP-IDF example component CMake")
    ensure_exists(idf_wrapper, "ESP-IDF app_main wrapper")
    ensure_exists(idf_arduino, "ESP-IDF CLI compatibility Arduino.h")
    ensure_exists(idf_wire, "ESP-IDF CLI compatibility Wire.h")
    ensure_exists(idf_shim, "ESP-IDF CLI native shim")

    ensure_missing(ROOT / "examples" / "00_smoke_boot", "deprecated example 00_smoke_boot")
    ensure_missing(
        ROOT / "examples" / "03_feature_walkthrough",
        "deprecated example 03_feature_walkthrough",
    )

    for name in REQUIRED_COMMON:
        ensure_exists(common_dir / name, f"common helper {name}")

    text = bringup_main.read_text(encoding="utf-8", errors="replace")

    for cmd in MANDATORY_COMMANDS:
        if re.search(rf"\b{re.escape(cmd)}\b", text) is None:
            fail(f"mandatory command '{cmd}' missing in {bringup_main.as_posix()}")

    for cmd in MANDATORY_SUBSTRINGS:
        if cmd not in text:
            fail(f"mandatory CLI phrase '{cmd}' missing in {bringup_main.as_posix()}")

    if re.search(r"\bcfg\b", text) is None and re.search(r"\bsettings\b", text) is None:
        fail("either 'cfg' or 'settings' command must be present")

    cmake_text = idf_cmake.read_text(encoding="utf-8", errors="replace")
    wrapper_text = idf_wrapper.read_text(encoding="utf-8", errors="replace")
    shim_text = idf_shim.read_text(encoding="utf-8", errors="replace")
    wire_text = idf_wire.read_text(encoding="utf-8", errors="replace")

    for needle in (
        "../../../01_basic_bringup_cli/main.cpp",
        "Opt4001IdfArduinoShim.cpp",
        "esp_driver_i2c",
        "esp_driver_uart",
    ):
        if needle not in cmake_text:
            fail(f"ESP-IDF CMake does not compile shared CLI/native dependency: {needle}")

    for needle in ("setup();", "loop();", "vTaskDelay"):
        if needle not in wrapper_text:
            fail(f"ESP-IDF app_main wrapper missing '{needle}'")

    for needle in (
        "driver/i2c_master.h",
        "i2c_new_master_bus",
        "i2c_master_transmit_receive",
        "i2c_master_probe",
        "uart_read_bytes",
        "esp_timer_get_time",
        "gpio_get_level",
    ):
        haystack = shim_text + "\n" + wire_text
        if needle not in haystack:
            fail(f"ESP-IDF CLI shim missing native implementation marker: {needle}")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
