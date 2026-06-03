# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

- No changes since `1.0.0`.

## [1.0.0] - 2026-06-03

### Added

- Production-oriented OPT4001 driver with injected I2C transport, tracked health
  state, and framework-neutral public headers/source.
- PicoStar/YMN and SOT-5X3/DTS package support with address validation and
  package-specific lux scaling.
- Power-down, continuous, one-shot, and forced-auto-range measurement flows.
- Decoded sample, FIFO/burst readout, CRC verification policy, raw register
  helpers, threshold helpers, interrupt presets, and sample-cache helpers.
- Explicit public contracts for lifecycle, freshness, blocking bounds, FIFO CRC
  aggregation, dirty hardware/cache state, FLAGS/INT behavior, transport
  errors, and numeric conversion bounds.
- Arduino ESP32-S2/S3 diagnostic CLI example and reusable example-local
  integration helpers.
- ESP-IDF component metadata, root CMake component support, and a native
  `examples/esp_idf/basic` diagnostic application using `app_main`,
  fixed-buffer CLI input, ESP-IDF timer/GPIO APIs, and
  `driver/i2c_master.h` transport glue.
- Guard scripts for core framework/timing boundaries, CLI command contracts,
  ESP-IDF example boundaries, version-header reproducibility, readiness claims,
  and public API documentation.
- Hardware-validation procedure, pending hardware-validation log, and optional
  bounded serial HIL runner for future board evidence capture.
- Documentation index for release-facing docs, validation evidence, reference
  material, and historical hardening reports.

### Changed

- Removed Arduino `millis()` and `yield()` fallbacks from the driver core;
  applications provide `Config::nowMs` and optional `Config::cooperativeYield`
  when blocking helpers need wall-clock time or cooperative scheduling.
- Standardized health behavior on latched `OFFLINE`: normal public I2C APIs
  return `BUSY` and do not touch the bus until `recover()` succeeds.
- Reworked the ESP-IDF example from an Arduino compatibility path into a native
  ESP-IDF diagnostic CLI while preserving command parity through repo-local
  contract checks.
- Expanded the Arduino diagnostic CLI with version, reset/recover, decoded
  config/ID, threshold, FIFO, INT, helper-conversion, health, stress, self-test,
  watch, and status commands.
- Made `include/OPT4001/Version.h` deterministic, generated from
  `library.json`, and intentionally tracked so clean manual, CMake, and ESP-IDF
  consumers can include `OPT4001/OPT4001.h` without PlatformIO generation.
- Updated README, metadata, SECURITY, assumptions, and reports to distinguish
  local source/build evidence from pending real-device and pure ESP-IDF build
  evidence.
- Narrowed Doxygen inputs to maintained API and reference documents so
  generated docs do not promote historical audit reports as primary API pages.

### Fixed

- Guarded raw-register APIs before successful `begin()`, after `end()`, and
  while the driver is latched `OFFLINE`.
- Preserved probe and startup transport error detail while keeping `probe()`
  free of health side effects.
- Validated full `DEVICE_ID` fixed-pattern fields instead of accepting only a
  partial DIDH match.
- Tied fresh-sample reads to flag, INT, or counter evidence and prevented stale
  samples from being reported as fresh.
- Propagated readiness-poll transport errors through `tryRead*()` and blocking
  helper paths instead of flattening them into not-ready or timeout results.
- Added finite polling caps for blocking read helpers using injected time.
- Hardened raw lux, threshold, range, conversion-time, and CRC paths with
  independent vectors, invalid-input handling, and wider intermediates.
- Decoded all four burst/FIFO slots after transfer success and preserved
  per-slot CRC fields while returning aggregate CRC status.
- Marked hardware/cache dirty state after partial multi-register write failures
  and cleared it only after successful reapply or recovery.

### Validation

- `python tools/check_core_timing_guard.py` passed.
- `python tools/check_cli_contract.py` passed.
- `python tools/check_idf_example_contract.py` passed.
- `python tools/check_version_header_contract.py` passed.
- `python tools/check_readiness_claims.py` passed.
- `python tools/check_public_api_docs.py` passed.
- `python scripts/generate_version.py check` passed.
- `python -m platformio test -e native` passed with 96/96 native
  fake-transport tests.
- `python -m platformio run -e esp32s3dev` passed.
- `python -m platformio run -e esp32s2dev` passed.
- `python -m platformio pkg pack` passed; generated package artifact was
  removed after validation.

### Known Limitations

- No completed real-device smoke, optical reference, address-pin matrix, INT
  capture, FIFO physical timing/order, or fault/recovery logs are included.
- Hardware/HIL validation was explicitly deferred because board, package,
  serial, wiring, optical, INT capture, and operator approval metadata were not
  provided.
- Pure ESP-IDF builds are configured in CI, but no completed local or CI
  ESP-IDF build log was captured for `1.0.0`.
- General-call reset is bus-wide; applications and HIL fixtures must explicitly
  approve that path before running reset/fault validation.
- Optical compensation remains application-specific and should be characterized
  with the final product enclosure/window and reference meter setup.

[Unreleased]: https://github.com/janhavelka/OPT4001/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/janhavelka/OPT4001/releases/tag/v1.0.0
