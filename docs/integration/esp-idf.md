# ESP-IDF Integration

The repository root can be used as an ESP-IDF component. The diagnostic example
in `examples/esp_idf/basic` is a native IDF application, not an Arduino wrapper.

## Boundary

- Entry point: `app_main()`.
- I2C: `driver/i2c_master.h` with `i2c_new_master_bus()`,
  `i2c_master_bus_add_device()`, `i2c_master_probe()`,
  `i2c_master_transmit()`, and `i2c_master_transmit_receive()`.
- CLI input: fixed C buffer using `getchar()`.
- CLI polling: stdin is configured with `O_NONBLOCK` so idle diagnostic input
  does not intentionally stall the driver's periodic `tick()` call.
- Timing/yield: `esp_timer_get_time()` and FreeRTOS task APIs through example
  transport callbacks.
- Forbidden in IDF examples: `Arduino.h`, `Wire.h`, `String`, `Serial`,
  `TwoWire`, `ArduinoCompat`, `IdfArduinoCompat`, and compiling Arduino example
  sources into the IDF example.

The driver core remains framework-neutral. Hardware access is injected through
`Config::i2cWrite`, `Config::i2cWriteRead`, optional `Config::gpioRead`,
`Config::nowMs`, and `Config::cooperativeYield`.

## Component Use

Add the repository root as a component directory:

```cmake
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../OPT4001")
```

The application owns the I2C bus, locking, timeout policy, reset-line handling,
INT GPIO setup, ISR attachment, ISR-to-task signaling, and pin lifetime. The
driver only consumes the configured callbacks.

`include/OPT4001/Version.h` is generated from `library.json` and committed, so a
clean ESP-IDF checkout can include `OPT4001/OPT4001.h` without running
PlatformIO or a Python pre-generation step.

## Transport Status Mapping

The example adapter preserves `esp_err_t` in `Status::detail`.

| IDF result | OPT4001 status | Notes |
| --- | --- | --- |
| `ESP_OK` | `OK` | Transaction succeeded. |
| `ESP_ERR_TIMEOUT` | `I2C_TIMEOUT` | Timeout is distinguishable. |
| `ESP_ERR_INVALID_RESPONSE` | `I2C_ERROR` | Transaction-level NACK/invalid response; address vs data phase is not exposed by this transaction API. |
| `ESP_ERR_INVALID_ARG` | `INVALID_PARAM` | Adapter/API argument failure. |
| `ESP_ERR_INVALID_STATE` | `I2C_BUS` | Driver or bus state fault. |
| Other `esp_err_t` | `I2C_BUS` | Raw value is kept in `detail`. |

`probe()` reads the full `DEVICE_ID` register pattern through this transport. A
successful I2C read with an unexpected ID returns `DEVICE_ID_MISMATCH`; timeout,
bus, and generic transaction failures are preserved as transport statuses.

## Example Contracts

`examples/esp_idf/basic/main/main.cpp` owns the diagnostic CLI. The transport
adapter maps ESP-IDF I2C, GPIO, timing, and yield APIs to the framework-neutral
callbacks. Compatibility files such as `Arduino.h`, `Wire.h`, and Arduino shim
sources are not part of the IDF example.

The command contract is enforced by:

```sh
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
python tools/check_version_header_contract.py
python tools/check_readiness_claims.py
```

When adding an Arduino CLI command, add the matching native IDF command or
document why parity is not applicable.

## Builds

Run real ESP-IDF builds when ESP-IDF is installed:

```sh
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

CI is configured to attempt the same target builds with Espressif's ESP-IDF CI
action. Treat configured CI as pending until completed workflow logs are
reviewed.

