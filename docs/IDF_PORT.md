# OPT4001 ESP-IDF Port

Last audited: 2026-05-19

This branch makes the OPT4001 library usable from Arduino and ESP-IDF while
keeping the driver core framework-neutral. The repository root is an ESP-IDF
component, and `examples/esp_idf/basic` is a full interactive CLI build rather
than a reduced sample.

Official references:
- I2C master driver: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html
- ESP-IDF v6 peripheral migration guide: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/migration-guides/release-6.x/6.0/peripherals.html

## Current State

- `library.json` declares `arduino` and `espidf` framework support on
  `espressif32`.
- Root `CMakeLists.txt` registers `src/OPT4001.cpp` and `include/` as an
  ESP-IDF component.
- `include/` and `src/` do not include Arduino, Wire, ESP-IDF I2C, GPIO,
  FreeRTOS, or UART headers.
- The core driver receives I2C through `Config::i2cWrite` and
  `Config::i2cWriteRead`; optional time, yield, and INT GPIO access are also
  callback-based.
- Arduino-specific glue remains in `examples/common/` and
  `examples/01_basic_bringup_cli/`.
- ESP-IDF-specific glue remains in `examples/esp_idf/basic/main/`.

## CLI Parity Model

The ESP-IDF example compiles the same CLI implementation used by the Arduino
example:

```cmake
"../../../01_basic_bringup_cli/main.cpp"
```

`examples/esp_idf/basic/main/main.cpp` only provides `app_main()`, calls the
shared `setup()` once, and then calls the shared `loop()` from a FreeRTOS task
loop. As a result, the user-visible CLI contract is shared:

- command names and aliases
- help sections and usage text
- argument defaults and ranges
- prompt, colors, and output structure
- chip feature coverage
- diagnostics, health/error reporting, probe, recover, reset, and reset-reapply
- self-test, stress, stress-mix, demo/watch workflows
- raw register reads/writes and raw block access

`tools/check_cli_contract.py` validates the Arduino CLI command surface and the
IDF shared-source wiring. `tools/check_idf_example_contract.py` validates the
IDF-specific parity and native-driver markers.

## Native ESP-IDF Glue

The IDF CLI uses local compatibility headers only inside
`examples/esp_idf/basic/main/`:

- `Arduino.h` provides the small `String`, `Serial`, timing, delay/yield, and
  GPIO subset required by the shared CLI.
- `Wire.h` / `Opt4001IdfArduinoShim.cpp` provide the small `TwoWire` subset
  required by `examples/common/I2cTransport.h`.
- The shim uses `driver/i2c_master.h`, `driver/gpio.h`, `driver/uart.h`,
  `esp_timer_get_time()`, and FreeRTOS delay/yield APIs.
- `Opt4001IdfI2cTransport.*` remains as a direct callback-adapter reference for
  IDF applications that do not want the CLI compatibility shim.

The new I2C master driver path uses:

- `i2c_new_master_bus()`
- `i2c_master_bus_add_device()`
- `i2c_master_probe()` for CLI bus scans
- `i2c_master_transmit()` for writes
- `i2c_master_transmit_receive()` for register reads
- `i2c_master_receive()` for direct reads where applicable

No legacy `driver/i2c.h`, command-link API, or driver-install API should be
introduced.

## General-Call Reset

`softReset()` and `resetAndReapply()` write `0x06` to general-call address
`0x00`. The IDF CLI shim attempts to create an IDF device handle for address
`0x00` on demand, and the direct callback adapter can route `0x00` writes
through `generalCallDev` when the application configures it.

This path is intentionally exposed by the CLI for parity, but it remains a
target validation item. If a specific ESP-IDF version or target rejects address
`0x00` handles, production applications should implement the reset path with
defined I2C operations or disable bus-wide reset in their application layer. Do
not silently drop the general-call write.

## Static Audits

Core framework-boundary check:

```bash
python tools/check_core_timing_guard.py
```

The guard scans `include/` and `src/` for Arduino headers/identifiers, direct
timing calls, Wire/Serial usage, and ESP-IDF framework headers.

Manual spot checks:

```bash
rg "<Arduino.h>|<Wire.h>|millis\\(|delay\\(|yield\\(|String\\b|Serial\\b|Wire\\b" include src
rg "driver/i2c.h|i2c_cmd_link|i2c_driver_install" .
```

The first command should return no matches. The second command should not find
legacy I2C usage.

## Build And Validation Plan

Host and Arduino regression:

```bash
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
pio test -e native
pio run -e esp32s3dev
pio run -e esp32s2dev
git diff --check
```

ESP-IDF builds when `idf.py` is available:

```bash
cd examples/esp_idf/basic
idf.py set-target esp32s3 build
idf.py set-target esp32s2 build
```

Hardware validation still required:

- DEVICE_ID readback and package-specific address rules
- PicoStar fixed `0x45` and SOT-5X3 `0x44` through `0x46`
- sample decode, CRC warning/error policy, lux scaling, and sample counter delta
- one-shot forced auto-range, one-shot previous-range, continuous mode,
  quick-wake, and conversion-time budgets
- threshold encode/decode and INT polarity/latch behavior
- `readIntPinAsserted()` with a configured IDF GPIO hook
- `probe()` with no health side effects and `recover()` with tracked health
- `reset` and `resetreapply` general-call behavior on the actual bus
- injected NACK, timeout, CRC error, and bus-error health transitions
