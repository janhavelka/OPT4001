# Validation Status

Current classification: source-level hardened and diagnostic-build tested.

The repository currently has native fake-transport tests, Arduino/PlatformIO
ESP32-S2/S3 build coverage, static contract checks, package-consumer checks, and
reviewed pure ESP-IDF v6.0.1 ESP32-S2/S3 CI builds. It does not claim completed
real-device hardware, optical, INT, FIFO timing/order, address-pin, or
fault/recovery evidence unless a future log captures it.

## Current Evidence

| Area | Evidence |
| --- | --- |
| Core portability | `tools/check_core_timing_guard.py` |
| Public contracts | `tools/check_public_api_docs.py` |
| Readiness wording and metadata | `tools/check_readiness_claims.py` |
| Version header | `scripts/generate_version.py check` and `tools/check_version_header_contract.py` |
| Native behavior | `python -m platformio test -e native` |
| Arduino ESP32 builds | `python -m platformio run -e esp32s3dev` and `python -m platformio run -e esp32s2dev` |
| Package import | `tools/check_clean_consumer_package.py` |
| ESP-IDF static boundary | `tools/check_idf_example_contract.py` |
| Pure ESP-IDF builds | [GitHub Actions run 31218427198](https://github.com/janhavelka/OPT4001/actions/runs/31218427198) passed for ESP32-S2 and ESP32-S3 with ESP-IDF v6.0.1. |

## Pending Evidence

| Area | Status |
| --- | --- |
| Real OPT4001 smoke test | Pending board/operator metadata and logs. |
| Optical accuracy / cover-glass correction | Pending application-specific optical setup and reference meter data. |
| SOT-5X3 address-pin matrix | Pending hardware wiring matrix. |
| INT threshold, every-conversion, and FIFO-full behavior | Pending board or logic-analyzer capture. |
| FIFO physical timing/order | Pending hardware matrix. |
| SMBus alert arbitration | Pending controller-level validation. |
| Fault/recovery paths | Pending controlled tests for NACK, timeout, unplug/replug, brownout, stuck bus, OFFLINE latch, and manual `recover()`. |

## Hardware Log Template

Fill these fields before running hardware or HIL tests.

| Field | Value |
| --- | --- |
| Date/time |  |
| Operator |  |
| Board name/revision |  |
| MCU target |  |
| Firmware commit |  |
| Library commit |  |
| OPT4001 package variant | PicoStar/YMN or SOT-5X3/DTS |
| I2C address wiring |  |
| INT pin connected/captured |  |
| Reference lux meter model/calibration |  |
| Optical setup |  |
| Serial port and baud |  |
| I2C pullups / bus speed |  |
| Logic analyzer model/sample rate |  |
| Pass/fail notes |  |

No hardware validation was run in the 2026-06-02 hardening session because board,
package, operator, serial, optical, INT capture, and fault-test metadata were
not provided. No serial/HIL logs were captured.

