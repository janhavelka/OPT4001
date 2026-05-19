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
- `examples/esp_idf/basic` demonstrates application-owned bus/device setup with
  the new `driver/i2c_master.h` API, `esp_timer_get_time()` timing, a FreeRTOS
  yield hook, and optional GPIO input handling for INT.

## General-Call Reset Note

`softReset()` and `resetAndReapply()` write to the OPT4001 general-call address
`0x00`. The example adapter routes `0x00` writes through `generalCallDev` when
the application configures that handle, but the basic example does not create or
exercise that handle because this environment could not verify ESP-IDF v6.0.1
address-`0x00` device-handle behavior. Validate this on target IDF; if a normal
handle cannot represent `0x00`, implement the reset path with defined I2C
operations in the application adapter.

## Validation

- Static check target: `rg "<Arduino.h>|<Wire.h>|millis\\(|delay\\(|yield\\(" include src`
  should return no matches.
- Arduino examples remain under `examples/01_basic_bringup_cli` and continue to
  provide `Wire`, `millis()`, and `yield()` through example-local callbacks.
- IDF builds were not run in this environment because `idf.py` was not on PATH.

## Remaining Hardware Work

- Build `examples/esp_idf/basic` for ESP32-S3 and ESP32-S2 with ESP-IDF 6.0.1.
- Validate device ID, SOT-5X3 and PicoStar address rules, sample decode, CRC
  policy, lux scaling, FLAGS clear behavior, INT GPIO integration, and
  health/recovery behavior on hardware.
- Validate the general-call reset adapter path explicitly before claiming
  IDF reset support in production examples.
