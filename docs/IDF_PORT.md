# OPT4001 ESP-IDF Port

The ESP-IDF example is a native IDF application in `examples/esp_idf/basic`.
It does not compile Arduino example sources and does not provide Arduino
compatibility facades.

Native boundaries:
- Entry point: `app_main()`.
- I2C: `driver/i2c_master.h` with `i2c_new_master_bus()`,
  `i2c_master_bus_add_device()`, `i2c_master_probe()`,
  `i2c_master_transmit()`, and `i2c_master_transmit_receive()`.
- CLI input: fixed C buffer using `getchar()`.
- Timing/yield: `esp_timer_get_time()` and FreeRTOS task APIs through the
  example transport callbacks.
- Forbidden in IDF examples: `Arduino.h`, `Wire.h`, `String`, `Serial`,
  `TwoWire`, `ArduinoCompat`, `IdfArduinoCompat`, and including
  `examples/01_basic_bringup_cli/main.cpp`.

The driver core remains framework-neutral. Hardware access is injected through
`Config::i2cWrite`, `Config::i2cWriteRead`, optional `Config::gpioRead`,
`Config::nowMs`, and `Config::cooperativeYield`.

## Version Header

`include/OPT4001/Version.h` is generated from `library.json` and intentionally
committed. The root ESP-IDF component therefore does not need to run Python or a
PlatformIO pre-script before `OPT4001/OPT4001.h` can resolve its public version
include from a clean checkout.

After changing `library.json` or `scripts/generate_version.py`, run:

```sh
python scripts/generate_version.py sync
python scripts/generate_version.py check
python tools/check_version_header_contract.py
```

PlatformIO may inject build metadata through compiler defines. ESP-IDF and other
CMake/manual consumers get stable `"unknown"` fallback metadata unless their
build system defines `OPT4001_BUILD_DATE`, `OPT4001_BUILD_TIME`,
`OPT4001_BUILD_TIMESTAMP`, `OPT4001_GIT_COMMIT`, or `OPT4001_GIT_STATUS`.

Run the static contract check after touching the IDF example:

```sh
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
python tools/check_version_header_contract.py
```

Run real ESP-IDF builds when ESP-IDF is installed:

```sh
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

The static checks do not prove pure ESP-IDF builds by themselves.
