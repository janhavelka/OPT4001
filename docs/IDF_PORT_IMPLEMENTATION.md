# OPT4001 ESP-IDF Port Implementation

Implemented on branch `idf-port`.

## Core Boundary

- `include/` and `src/` are framework-neutral and do not include Arduino,
  `Wire`, `Serial`, ESP-IDF I2C, FreeRTOS, or GPIO headers.
- The driver receives all I2C access through `Config::i2cWrite` and
  `Config::i2cWriteRead`; optional INT reads use `Config::gpioRead`.
- `Config::nowMs` and `Config::cooperativeYield` remain optional. If `nowMs` is
  not supplied, health timestamps use `0`; if `cooperativeYield` is not supplied,
  the driver performs no scheduler call.

## ESP-IDF Additions

- Root `CMakeLists.txt` registers the library as an ESP-IDF component.
- `idf_component.yml` declares component-manager metadata for ESP-IDF 6.x.
- `examples/esp_idf/basic` compiles the same interactive bring-up CLI source as
  `examples/01_basic_bringup_cli`, so Arduino and ESP-IDF expose the same
  command names, aliases, help text, arguments, colorized output, diagnostics,
  health reporting, probe/recover/reset flows, stress/self-test workflows, and
  raw register access.
- The IDF CLI uses a local compatibility layer for the small `Serial`, `String`,
  `Wire`, timing, delay/yield, and GPIO subset required by the shared CLI. That
  layer is backed by ESP-IDF UART, GPIO, `esp_timer`, FreeRTOS, and the new
  `driver/i2c_master.h` API.
- `Opt4001IdfI2cTransport.*` remains as a direct native-IDF callback adapter for
  applications that do not want to use the CLI compatibility layer.

## General-Call Reset Note

`softReset()` and `resetAndReapply()` write to the OPT4001 general-call address
`0x00`. The shared IDF CLI exposes `reset` and `resetreapply`; its `Wire` shim
attempts to create an address-`0x00` device handle on demand. The direct
callback adapter also supports routing `0x00` writes through `generalCallDev`
when the application configures that handle.

This environment could not verify ESP-IDF address-`0x00` device-handle
behavior. Validate this on target IDF; if a normal handle cannot represent
`0x00`, implement the reset path with defined I2C operations in the application
adapter.

## Validation

- Static check target: `rg "<Arduino.h>|<Wire.h>|millis\\(|delay\\(|yield\\(" include src`
  should return no matches.
- Arduino examples remain under `examples/01_basic_bringup_cli` and continue to
  provide `Wire`, `millis()`, and `yield()` through example-local callbacks.
- The ESP-IDF example compiles the same CLI source through local compatibility
  headers and native IDF implementations.
- `python tools/check_cli_contract.py`
- `python tools/check_idf_example_contract.py`
- `python tools/check_core_timing_guard.py`
- IDF builds were not run in this environment because `idf.py` was not on PATH.

## Remaining Hardware Work

- Build `examples/esp_idf/basic` for ESP32-S3 and ESP32-S2 with ESP-IDF 6.0.1.
- Validate device ID, SOT-5X3 and PicoStar address rules, sample decode, CRC
  policy, lux scaling, FLAGS clear behavior, INT GPIO integration, and
  health/recovery behavior on hardware.
- Validate the general-call reset adapter path explicitly before claiming
  IDF reset support in production examples.
