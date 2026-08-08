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
| Native behavior | `.\scripts\pio.cmd test -e native` passes 133/133 fake-transport, parser, and compatibility tests through the required Windows wrapper. |
| Framework-neutral consumer | `.\scripts\pio.cmd run -e native_core_no_arduino` compiles and links without Arduino include paths. |
| Arduino ESP32 builds | `.\scripts\pio.cmd run -e esp32s3dev` and `.\scripts\pio.cmd run -e esp32s2dev` |
| Package import | `tools/check_clean_consumer_package.py` |
| ESP-IDF static boundary | `tools/check_idf_example_contract.py` |
| Pure ESP-IDF builds | [GitHub Actions run 31227037444](https://github.com/janhavelka/OPT4001/actions/runs/31227037444) passed for ESP32-S2 and ESP32-S3 with ESP-IDF v6.0.1. |

### 2026-08-08 Current Audit Evidence (1.2.2)

- SBOS993A capability/register coverage and CLI parity remain recorded in
  `docs/reports/feature-matrix-20260808.md`.
- Mature-peer naming, compatibility decisions, and proven code/artifact cleanup
  are recorded in `docs/reports/naming-audit-20260808.md`.
- Invalid `Err` and `DriverState` name fallbacks now use the cross-library
  `"UNKNOWN"` spelling; native coverage asserts every valid name and invalid
  casts without changing enum values or valid diagnostic strings.
- Official-wrapper PlatformIO native execution passes **133/133**
  fake-transport, fixed-line-parser, and aggregate-compatibility tests; HIL
  parser tests pass **15/15** without opening a serial port.
- Core, CLI, native-IDF boundary, public-doc, readiness-claim,
  generated-version, whitespace, and Doxygen gates pass.
- The strict framework-neutral native-core link, package pack/clean consumer,
  and pinned pioarduino 55.03.311 ESP32-S2/S3 builds pass locally. No local
  `idf.py` installation is available, so the pure ESP-IDF compiler evidence is
  the pinned CI run above. No physical claim is inferred from build results.

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

No hardware validation is recorded because board, package, operator, serial,
optical, INT-capture, and fault-fixture evidence has not been provided. No
serial/HIL transcript is presented as completed evidence.

