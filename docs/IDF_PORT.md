# OPT4001 ESP-IDF v6.0.1 Port Audit

Last audited: 2026-05-17

This started as a readiness audit and now records the ESP-IDF implementation
target for branch `idf-port`. See `docs/IDF_PORT_IMPLEMENTATION.md` for the
implemented file-level summary and validation notes.

Official ESP-IDF references for the future port:
- I2C master driver: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html
- ESP-IDF v6.0 peripheral migration guide: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/migration-guides/release-6.x/6.0/peripherals.html

## Current Framework/Library State

- `library.json` version is `1.0.0`; the package declares `arduino` and
  `espidf` framework support on `espressif32`.
- `platformio.ini` builds the Arduino CLI example for ESP32-S3 and ESP32-S2 and
  includes a native Unity test environment.
- Public API is under `include/OPT4001/` and is already callback-based at the
  I2C boundary.
- `include/OPT4001/Config.h` exposes `I2cWriteFn`, `I2cWriteReadFn`,
  optional `GpioReadFn`, `NowMsFn`, and `YieldFn`.
- `include/OPT4001/OPT4001.h` exposes `Status begin(const Config&)`,
  `void tick(uint32_t nowMs)`, `void end()`, package-aware address validation,
  sample/burst reads, one-shot and continuous modes, INT/threshold
  configuration, general-call reset helpers, raw register access, and
  four-state health tracking.
- `src/OPT4001.cpp` routes I2C through `_i2cWriteReadRaw`, `_i2cWriteRawTo`,
  `_i2cWriteRaw`, `_i2cWriteReadTracked`, `_i2cWriteTrackedTo`, and
  `_i2cWriteTracked`; health is updated from tracked wrappers.
- The library core no longer includes `<Arduino.h>` and no longer calls
  `millis()` or `yield()` from `_nowMs()` / `_cooperativeYield()`; applications
  should provide timing/yield hooks when needed.
- Arduino-only glue lives in `examples/common/I2cTransport.h`,
  `I2cScanner.h`, `BoardConfig.h`, and the CLI example.

Readiness verdict: the driver core is framework-neutral and IDF component/example
scaffolding is present. Final readiness still requires an ESP-IDF 6.0.1 build,
hardware validation, and explicit validation of the general-call reset adapter
path.

## Portability Blockers

- ESP-IDF compilation has not been verified in this shell because `idf.py` was
  unavailable.
- General-call address `0x00` support remains a target-hardware validation item.
- Hardware validation remains outstanding.
- `softReset()` and `resetAndReapply()` use a general-call write through
  `_i2cWriteTrackedTo(cmd::GENERAL_CALL_ADDRESS, ...)`; the IDF adapter must
  not assume a single fixed address handle.
- INT support depends on `Config::gpioRead`; IDF examples must configure GPIO
  externally and pass a callback.
- Arduino examples use `Serial`, `String`, `Wire`, `millis()`, `delay()`, and
  `yield()`; keep them Arduino-only.
- IDF v6.0.1 warning profiles can expose implicit conversions in CRC/sample
  decoding, threshold conversion, and range math.

## Exact Files/APIs To Change Later

- `src/OPT4001.cpp`
  - Remove the unconditional `#include <Arduino.h>`.
  - Keep `_i2cWriteReadRaw()`, `_i2cWriteRawTo()`, `_i2cWriteRaw()`,
    `_i2cWriteReadTracked()`, `_i2cWriteTrackedTo()`, and `_i2cWriteTracked()`
    as the only transport path.
  - Replace `_nowMs()` and `_cooperativeYield()` fallbacks with a portability
    boundary:
    - Arduino build: may call `millis()` and `yield()`.
    - ESP-IDF build: use `Config::nowMs` and `Config::cooperativeYield`, or
      guarded defaults using `esp_timer_get_time()` and `taskYIELD()`.
  - Do not add direct `i2c_master_*` calls to register helpers.
- `include/OPT4001/Config.h`
  - Preserve callback signatures and package/address fields.
  - Document that pure IDF users should set `nowMs`, `cooperativeYield`, and
    `gpioRead` when INT is used.
  - Do not include IDF driver headers in the public core header.
- `include/OPT4001/OPT4001.h`
  - Preserve namespace, class name, enums, `Status`, sample structures,
    threshold APIs, general-call reset APIs, and health APIs.
- Add root `CMakeLists.txt`.
- Add IDF-only adapter/example files under a new path such as
  `examples/esp_idf/basic/`.
- Do not edit Arduino examples/common helpers except for separate Arduino
  regression fixes during the implementation PR.

## Proposed Architecture Preserving Arduino Compatibility

- Keep the OPT4001 core callback-based and framework-neutral.
- Keep the Arduino `Wire` adapter in `examples/common/I2cTransport.h`.
- Add an IDF I2C adapter outside the driver core. It owns IDF bus/device
  handles and supplies callbacks to `OPT4001::Config`.
- The adapter must be address-aware:
  - normal operations use the configured device address (`0x45` fixed for
    PicoStar, `0x44` to `0x46` for SOT-5X3);
  - general-call reset uses address `0x00`.
- Keep bus setup, pins, pull-ups, clock speed, and bus lifetime in the
  application/example.
- Preserve health behavior:
  - `probe()` uses raw I2C and does not update health;
  - register/sample helpers use tracked wrappers;
  - validation and "not ready" poll results are not transport failures;
  - `recover()` uses tracked operations.
- Keep Arduino and IDF examples separate.

## IDF Transport Adapter Contract

The adapter should use the ESP-IDF v6.0.1 new I2C master driver only:

```cpp
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct Opt4001IdfI2c {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  i2c_master_dev_handle_t generalCallDev = nullptr;
  uint8_t address = 0x45;
  gpio_num_t intPin = GPIO_NUM_NC;
};
```

Callback behavior:
- `i2cWrite(addr, data, len, timeoutMs, user)` calls
  `i2c_master_transmit()` on the handle matching `addr`.
- `i2cWriteRead(addr, txData, txLen, rxData, rxLen, timeoutMs, user)` calls
  `i2c_master_transmit_receive()` on the configured OPT4001 device handle.
- The callbacks must be synchronous from the driver point of view. Do not
  register `i2c_master_register_event_callbacks()` on these handles unless the
  adapter waits for completion before returning; tracked wrappers update health
  immediately after the callback returns.
- Supported write addresses are the configured device address and general-call
  address `0x00`. Validate on IDF v6.0.1 that a device handle with address
  `0x00` is accepted. If it is not, use
  `i2c_master_execute_defined_operations()` / `I2C_DEVICE_ADDRESS_NOT_USED` in
  the application-owned adapter, or compile-disable `softReset()` /
  `resetAndReapply()` for pure IDF. Do not silently drop general-call reset.
- Supported read address is the configured device address only.
- Map `ESP_OK` to `OPT4001::Status::Ok()`.
- Map `ESP_ERR_TIMEOUT` to `OPT4001::Err::I2C_TIMEOUT`.
- Map `ESP_ERR_INVALID_RESPONSE` to an I2C NACK-related status. The simple
  ESP-IDF master APIs do not distinguish address and data phase, so prefer
  `OPT4001::Err::I2C_ERROR` with `Status.detail = ESP_ERR_INVALID_RESPONSE`
  unless a custom adapter can prove the phase.
- Map other adapter or bus failures to `OPT4001::Err::I2C_BUS` or
  `OPT4001::Err::I2C_ERROR`; preserve raw `esp_err_t` in `Status::detail`.
- Clamp or reject `timeoutMs` before passing it to ESP-IDF's signed
  `xfer_timeout_ms`; never allow overflow to become `-1` because `-1` waits
  forever.
- `gpioRead(pin, user)` should call `gpio_get_level()`.
- `nowMs(user)` should return `esp_timer_get_time() / 1000`.
- `cooperativeYield(user)` should call `taskYIELD()` or `vTaskDelay(1)`.

## Component/CMake Layout

Recommended component layout:

```text
OPT4001/
  CMakeLists.txt
  include/OPT4001/*.h
  src/OPT4001.cpp
  examples/esp_idf/basic/
    CMakeLists.txt
    main/CMakeLists.txt
    main/main.cpp
    main/Opt4001IdfI2cTransport.cpp
```

Core-only component:

```cmake
idf_component_register(
  SRCS "src/OPT4001.cpp"
  INCLUDE_DIRS "include"
  REQUIRES esp_timer freertos
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
```

If an IDF adapter is shipped inside the component, include its source and add
`PRIV_REQUIRES esp_driver_i2c esp_driver_gpio esp_timer freertos`. If the
adapter lives only in the example, put those requirements in the example
component instead.

## Example Plan

- Keep the existing Arduino CLI example as the Arduino reference.
- Add `examples/esp_idf/basic`:
  - create an I2C master bus with `i2c_new_master_bus()`;
  - add the OPT4001 device with `i2c_master_bus_add_device()`;
  - add or explicitly validate the general-call `0x00` handle if reset APIs are
    demonstrated;
  - fill `OPT4001::Config` with IDF callbacks, package variant, address,
    `nowMs`, `cooperativeYield`, and optional INT GPIO;
  - call `begin()`;
  - log DEVICE_ID decode, one sample, lux, FLAGS, and health counters.
- Add a second IDF example only after basic success:
  - configure thresholds and INT output;
  - demonstrate `tryReadSample()` in a bounded FreeRTOS loop;
  - demonstrate burst/FIFO reads.

## Test/Validation Plan

- Static checks:
  - `rg "<Arduino.h>|<Wire.h>|millis\\(|delay\\(|yield\\(" include src`
    should find no unguarded Arduino dependencies in the ESP-IDF build path.
  - `rg "driver/i2c.h|i2c_cmd_link|i2c_driver_install" .` should not find
    legacy I2C driver usage in IDF code.
- Arduino regression:
  - `pio test -e native`
  - `pio run -e esp32s3dev`
  - `pio run -e esp32s2dev`
- IDF build:
  - `idf.py set-target esp32s3 build` from `examples/esp_idf/basic`
  - `idf.py set-target esp32s2 build` from `examples/esp_idf/basic`
- Hardware validation:
  - `begin()` verifies DEVICE_ID and package-specific address rules.
  - Validate PicoStar fixed `0x45` and SOT-5X3 `0x44` through `0x46` configs
    when hardware is available.
  - Validate sample decode, CRC warnings/errors, lux scaling, and sample counter
    delta.
  - Validate one-shot forced auto-range, one-shot previous-range, continuous
    mode, quick wake, and conversion-time budgets.
  - Validate threshold encode/decode and INT polarity/latch behavior.
  - Validate general-call reset explicitly and document any IDF driver
    limitation around address `0x00`.
  - Inject NACK, timeout, CRC error, and bus errors and verify health/recovery.

## ESP-IDF v6.0.1 Migration Hazards

- Do not use legacy `<driver/i2c.h>` or command-link APIs. New code must use
  `<driver/i2c_master.h>` and declare `esp_driver_i2c`.
- `ESP_ERR_INVALID_RESPONSE` is the new-driver NACK indication; map it
  consistently and keep the numeric detail.
- ESP-IDF components must declare split driver dependencies explicitly.
- The I2C callback timeout is milliseconds for the new master API. Do not pass
  FreeRTOS ticks by mistake.
- General-call address `0x00` is required for `softReset()`; verify the IDF
  master device-handle behavior on v6.0.1 before claiming reset support. If a
  handle cannot represent `0x00`, use defined I2C operations with
  `I2C_DEVICE_ADDRESS_NOT_USED` in the adapter or make reset APIs unavailable
  in the pure-IDF surface.
- IDF v6 warning-as-error profiles can fail on integer shifts, narrow
  conversions, and float conversions in sample/threshold code. Fix warnings
  before CI gating.
- Keep bus ownership outside the OPT4001 driver.

## Ordered Implementation Checklist

1. Add the root `CMakeLists.txt` for the core component.
2. Remove or compile-guard the Arduino include and timing/yield fallbacks in
   `src/OPT4001.cpp`.
3. Build the core component under IDF with callback stubs.
4. Add the IDF I2C/GPIO adapter using `<driver/i2c_master.h>` and
   `driver/gpio.h`.
5. Add explicit support or documented handling for general-call address `0x00`.
6. Add `examples/esp_idf/basic` and build for ESP32-S3.
7. Build the same example for ESP32-S2.
8. Run PlatformIO native and Arduino example builds.
9. Validate identity, samples, lux conversion, CRC, thresholds, INT, and reset
   on hardware.
10. Inject I2C and CRC failures and verify status/health/recovery behavior.
11. Add optional IDF component manifest only after both Arduino and IDF builds
    pass.
