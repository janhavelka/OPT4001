# OPT4001 ESP-IDF Port Implementation

Implementation status:
- `examples/esp_idf/basic/main/main.cpp` owns the native fixed-buffer CLI.
- `Opt4001IdfI2cTransport.*` maps ESP-IDF I2C/GPIO/timing APIs to the
  framework-neutral driver callbacks.
- The ESP-IDF CMake target compiles only native IDF sources plus the callback
  adapter.
- Compatibility files (`Arduino.h`, `Wire.h`, Arduino shim sources) are not
  part of the IDF example.

The command contract is enforced by `tools/check_idf_example_contract.py`.
When adding a CLI command to the Arduino bring-up example, add the matching
native IDF command or explicitly document why parity is not applicable.
