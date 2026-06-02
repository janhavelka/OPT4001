# OPT4001 ESP-IDF Port Implementation

Implementation status:
- `examples/esp_idf/basic/main/main.cpp` owns the native fixed-buffer
  diagnostic CLI and configures stdin for nonblocking polling so `tick()` is not
  intentionally stalled by idle console input.
- `Opt4001IdfI2cTransport.*` maps ESP-IDF I2C/GPIO/timing APIs to the
  framework-neutral driver callbacks.
- The IDF application owns GPIO setup, pullups, ISR attachment, ISR-to-task
  signaling, and pin lifetime for SOT-5X3 INT. The driver core only consumes the
  optional `gpioRead` callback and does not own GPIO/INT hardware.
- The ESP-IDF CMake target compiles only native IDF sources plus the callback
  adapter. The example main component exposes only its local include directory
  and depends on the root `OPT4001` component for public headers.
- `include/OPT4001/Version.h` is committed and checked against `library.json`,
  so clean ESP-IDF checkouts can resolve `OPT4001/OPT4001.h` without a
  PlatformIO pre-generation step.
- Compatibility files (`Arduino.h`, `Wire.h`, Arduino shim sources) are not
  part of the IDF example.

The command contract is enforced by `tools/check_idf_example_contract.py`. The
version-header contract is enforced by `tools/check_version_header_contract.py`.
When adding a CLI command to the Arduino bring-up example, add the matching
native IDF command or explicitly document why parity is not applicable.

CI is configured to attempt pure ESP-IDF builds of this example for `esp32s3`
and `esp32s2` with Espressif's ESP-IDF CI action. Treat that as configured CI
coverage, not local hardware evidence; review the completed workflow logs before
claiming a target build has passed.

Prompt 8 local validation on 2026-06-02 passed the static IDF contract check,
but pure ESP-IDF target builds were not run because `idf.py` was not available
on `PATH` in the validation shell. The attempted commands were
`idf.py --version`, `idf.py -C examples/esp_idf/basic set-target esp32s3 build`,
and `idf.py -C examples/esp_idf/basic set-target esp32s2 build`.

Threshold and interrupt behavior is currently documented at the register
contract level. Physical threshold comparator behavior, SMBus alert response,
open-drain pulse timing, and ISR integration still need ESP-IDF target-hardware
validation.
