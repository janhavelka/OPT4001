#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
IDF_MAIN = ROOT / "examples" / "esp_idf" / "basic" / "main"


def fail(msg: str) -> None:
    print(f"IDF example contract FAILED: {msg}")
    raise SystemExit(1)


def read_required(path: pathlib.Path, label: str) -> str:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")
    return path.read_text(encoding="utf-8", errors="replace")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        fail(f"{label} missing '{needle}'")


def main() -> int:
    arduino_cli = read_required(
        ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp",
        "shared Arduino bring-up CLI",
    )
    cmake = read_required(IDF_MAIN / "CMakeLists.txt", "ESP-IDF main CMakeLists.txt")
    wrapper = read_required(IDF_MAIN / "main.cpp", "ESP-IDF app_main wrapper")
    arduino_h = read_required(IDF_MAIN / "Arduino.h", "ESP-IDF Arduino compatibility header")
    wire_h = read_required(IDF_MAIN / "Wire.h", "ESP-IDF Wire compatibility header")
    shim = read_required(IDF_MAIN / "Opt4001IdfArduinoShim.cpp", "ESP-IDF native shim")
    callback_adapter = read_required(
        IDF_MAIN / "Opt4001IdfI2cTransport.cpp",
        "ESP-IDF callback transport adapter",
    )
    callback_adapter_h = read_required(
        IDF_MAIN / "Opt4001IdfI2cTransport.h",
        "ESP-IDF callback transport adapter header",
    )

    for command in (
        "help / ?",
        "version / ver",
        "scan",
        "probe",
        "recover",
        "resetreapply",
        "selftest",
        "stress_mix",
        "threshold raw <low> <high>",
        "int ready|fifo",
        "regs <start> <len>",
    ):
        require(arduino_cli, command, "shared CLI source")

    for needle in (
        "../../../01_basic_bringup_cli/main.cpp",
        "Opt4001IdfArduinoShim.cpp",
        "Opt4001IdfI2cTransport.cpp",
        "esp_driver_i2c",
        "esp_driver_gpio",
        "esp_driver_uart",
    ):
        require(cmake, needle, "ESP-IDF CMake")

    for needle in ("setup();", "loop();", "vTaskDelay"):
        require(wrapper, needle, "ESP-IDF app_main wrapper")

    for needle in ("class String", "class HardwareSerial", "millis()", "delay("):
        require(arduino_h, needle, "ESP-IDF compatibility header")

    for needle in ("driver/i2c_master.h", "class TwoWire", "extern TwoWire Wire"):
        require(wire_h, needle, "ESP-IDF Wire shim header")

    for needle in (
        "uart_driver_install",
        "uart_read_bytes",
        "esp_timer_get_time",
        "gpio_get_level",
        "i2c_new_master_bus",
        "i2c_master_probe",
        "i2c_master_transmit",
        "i2c_master_transmit_receive",
        "i2c_master_receive",
    ):
        require(shim, needle, "ESP-IDF native shim")

    for needle in ("driver/i2c_master.h", "i2c_master_transmit_receive"):
        require(callback_adapter + "\n" + callback_adapter_h, needle, "ESP-IDF callback adapter")

    forbidden_wrapper_markers = ("ESP_LOGI", "readBlockingLux", "readFlags")
    for marker in forbidden_wrapper_markers:
        if marker in wrapper:
            fail(f"ESP-IDF wrapper still contains old one-shot demo marker '{marker}'")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
