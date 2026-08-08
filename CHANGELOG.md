# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.2.1] - 2026-08-08

### Changed

- Aligned private transport-address helpers and example health/status output
  with the stable library-owned naming used by mature workspace I2C drivers.
- Reduced example-only health and CLI helpers to the paths exercised by the
  diagnostic applications, while retaining typed dirty-state visibility.
- Replaced completed implementation prompts and an empty NOT-RUN HIL report
  with durable feature, naming, integration, and validation documentation.

### Fixed

- Removed an unreachable private readiness helper whose power-down inference
  was not part of the driver's hardware-evidence freshness path.
- Removed the unused legacy example command parser, avoiding a second parser
  with silent truncation semantics beside the bounded shared line buffer.
- Updated current validation evidence, CI links, commands, generated metadata,
  and Doxygen inputs without claiming physical validation.
- Kept example health-rate formatting correct when two saturated lifetime
  counters would overflow a 32-bit intermediate.

### Validation

- Static framework, CLI, native-IDF, public-doc, version, readiness, package,
  and HIL-parser contracts pass; native fake-transport tests remain 133/133.
- The framework-neutral native consumer, clean packed-library consumer,
  pioarduino 55.03.311 ESP32-S2/S3 builds, and pure ESP-IDF v6.0.1 CI baseline
  pass. Hardware, optical, INT, FIFO timing/order, address-strap, and injected
  electrical-fault evidence remain open.

## [1.2.0] - 2026-08-08

### Added

- Bus-silent `bind()` / `unbind()` lifecycle and a five-instruction
  `startAttach()` path for sole-owner I2C task integration.
- Explicit error-reporting `powerDown()` and bus-silent `cancelPollJob()` with
  partial-write dirty-state provenance.
- DEVICE_ID-qualified `discover`, complete poll-job commands, runtime color,
  native-IDF health monitoring, and bounded `selfcheck` parity in both CLIs.
- Shared example-only fixed line/text storage with overlong-line discard and
  recovery regressions.
- SBOS993A device/core/CLI/test feature matrix in
  `docs/reports/feature-matrix-20260808.md`.

### Changed

- Diagnostic `stress` and `stress_mix` are finite cooperative owner-loop
  sessions. Measurement phases use `poll(..., 1)` so a service iteration
  performs at most one transport callback.
- The version generator now synchronizes `idf_component.yml`, Doxygen project
  metadata, and the supported security branch from `library.json`.
- CLI contracts verify help and handler visibility, fixed-buffer overflow
  recovery, qualified discovery, owner jobs, health/color parity, and absence
  of superseded blocking stress handlers.

### Fixed

- Prevented both CLIs from silently dispatching truncated overlong commands.
- Removed Arduino command-path `String` allocation and aligned Arduino/native
  IDF parser and diagnostic behavior.
- Preserved the documented legacy `end()` power-down attempt while providing a
  separate bus-silent release API instead of redefining shutdown semantics.

### Validation

- PlatformIO native execution passes 133 fake-transport/parser/API-compatibility
  tests. The strict framework-neutral native-core link, package pack/clean
  consumer, current ESP32-S2/S3 builds, and pure ESP-IDF v6.0.1 CI pass; this
  release does not claim new hardware validation.

## [1.1.1] - 2026-08-08

### Changed

- Non-burst raw register-window reads now issue one bounded transaction per
  16-bit register, preserving contiguous byte semantics without hardware
  pointer auto-increment.
- Arduino and native ESP-IDF CLIs now share the documented conversion-start
  acceptance/readiness flow, latest-register `raw` semantics, register-byte
  formatting, and stronger executable command-contract checks.
- Native ESP-IDF `stress_mix` and `selftest` now exercise bounded mixed
  operations instead of aliases/minimal placeholders.
- Lux-to-threshold conversion rounds to the nearest representable register
  quantum.

### Fixed

- Rejected overflowing `size_t` register windows before transport instead of
  truncating the register span and forwarding an oversized read.
- Made blocking deadlines correct across the full `uint32_t` timeout range and
  retained truthful in-flight one-shot state after a host timeout.
- Made sample full-scale/resolution helpers return NaN for invalid hardware
  result exponents instead of silently treating them as auto-range.
- Prevented cache-only package/CRC setters from changing or losing policy while
  a staged poll job owns the driver state.
- Fixed Arduino one-shot watch and IDF burst/scaled-read helpers that treated
  the documented `IN_PROGRESS` start result as failure.
- Fixed Arduino burst, slot-0, milli-lux, and micro-lux commands that consumed
  the fresh token in a priming read before the requested operation.

### Validation

- Added native regressions for overflow rejection, non-burst register windows,
  wide blocking timeouts, retained timeout state, poll-job cache admission,
  invalid sample exponents, and threshold quantization boundaries.
- This patch remains source/build validation only; hardware, optical, INT,
  address-strap, FIFO-timing, and injected-fault gates remain open.

## [1.1.0] - 2026-08-07

### Added

- Poll-chunked job API: `poll(nowMs, maxInstructions)`, `pollBusy()`,
  `lastPollStatus()`, `startReadSample()`, `startReadBurst()`,
  `getLastBurst()`, `startConfigureMeasurement()`, and
  `startResetAndReapply()`.
- Native tests for shared status+burst read budgets, delay gates, chunked
  config apply budgets, and failure-stop status reporting.
- Stable library-owned `errorName()` / `driverStateName()` and `toString()`
  helpers, including defined fallback names for invalid enum casts.
- Native tests for coherent sample transfers, FLAGS raw-access side effects,
  PicoStar INT restrictions, and invalid numeric-helper inputs.

### Changed

- `readBurst()` now shares the tracked RESULT/FIFO burst block decoder used by
  poll-chunked sample jobs and caches the most recently completed burst frame.
- `tryReadSample()` is documented as a synchronous diagnostic/convenience path,
  not an instruction-budgeted poll job.
- Arduino and native ESP-IDF diagnostics now share library-owned status/state
  names. The native IDF CLI adds bounded address/package selection, blocking and
  poll-friendly reads, FIFO slots, watch/stop, decoded configuration, threshold
  conversion, interrupt presets, timing/scaling, raw registers, and colorized
  sectioned help.
- PlatformIO pins the Arduino platform to pioarduino `55.03.311` and native
  tests to `platformio/native@1.2.1` for reproducible workspace builds.
- CI uses the stable Ubuntu 24.04 runner, PlatformIO Core `6.1.19`, and
  full-commit action pins; guard/package/HIL parser gates are enforced in the
  workflow.

### Fixed

- Read RESULT and RESULT_LSB_CRC in one transaction when burst mode is enabled,
  preventing mixed fields when a conversion completes between two reads.
- Kept generic FLAGS reads/writes synchronized with cached freshness evidence,
  without incorrectly marking cached hardware configuration dirty.
- Rejected impossible PicoStar INT GPIO hooks and interrupt-output presets
  before bus I/O.
- Made invalid range, conversion-time, and one-shot budget helpers honor their
  documented `NaN`/zero contracts instead of returning plausible values.
- Corrected native ESP-IDF conversion-time parsing so `ctime 0..11` maps to the
  enum values shown by the CLI rather than ambiguous rounded millisecond values.

### Validation

- Source/datasheet audit and open HIL gates are recorded in
  `docs/reports/source-audit-20260807.md`.
- GitHub Actions run `31218427198` passed native tests, package/contract gates,
  Arduino ESP32-S2/S3 builds, and pure ESP-IDF v6.0.1 ESP32-S2/S3 builds.

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

[Unreleased]: https://github.com/janhavelka/OPT4001/compare/v1.1.1...HEAD
[1.1.1]: https://github.com/janhavelka/OPT4001/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/janhavelka/OPT4001/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/janhavelka/OPT4001/releases/tag/v1.0.0
