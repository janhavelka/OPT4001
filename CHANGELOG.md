# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- ESP-IDF component metadata, root `CMakeLists.txt`, and a native
  `examples/esp_idf/basic` application using `app_main`, fixed-buffer CLI
  input, and ESP-IDF `driver/i2c_master.h` transport glue.
- IDF port implementation notes documenting the framework-neutral core boundary,
  general-call reset caveat, and validation limitations.
- `tools/check_idf_example_contract.py` to enforce native ESP-IDF example
  boundaries and command coverage without Arduino compatibility facades.
- `softReset()` and `resetAndReapply()` to align OPT4001 reset handling with the stronger sibling libraries while preserving the datasheet's general-call reset behavior.
- Sample-cache helpers: `getLastSample()`, `sampleTimestampMs()`, and `sampleAgeMs()`.
- `hasSample()` / `SettingsSnapshot::hasSample` and `readIntPinAsserted(bool&)` for cache and INT GPIO diagnostics.
- `readDeviceId()` and `setVerifyCrc()` typed helpers.
- Decoded helper structs and register helpers: `DeviceIdInfo`, `ConfigurationInfo`,
  `IntConfigurationInfo`, `readRegisters()`, `readSampleSlot()`,
  `getThresholdsLux()`, `clearConversionReadyFlag()`, full-scale / resolution helpers,
  and sample-counter delta math.
- End-user convenience helpers: `configureMeasurement()`, `tryReadSample()`,
  `tryReadLux()`, `restoreDefaultThresholds()`, and interrupt preset helpers for
  threshold, conversion-ready, and FIFO-full modes.
- Broader native coverage for reset, CRC-policy, sample-cache, and device-ID paths.
- Native coverage for latched `OFFLINE` no-bus-touch behavior and readiness error propagation through `tryRead*()` / `readBlocking()` paths.
- CLI coverage for the convenience-helper layer: `tryread`, `trylux`, `measure`,
  `threshold default`, `begin`, `intpin`, and interrupt preset commands.
- Non-blocking CLI watch mode with `watch`, `watch force`, and `stop` for live
  diagnostic observation of continuous and repeated one-shot measurement flows.
- Root `AGENTS.md` production guidelines for future driver work.
- Readiness-claims and public-API documentation guard scripts for CI.
- Hardware-validation procedure outline for future captured board evidence.

### Changed

- Removed Arduino `millis()` and `yield()` fallbacks from the driver core.
- Removed the ESP-IDF Arduino compatibility shim from the IDF example; command
  parity is now maintained by a repo-local native command contract.
  Applications should provide `Config::nowMs` and `Config::cooperativeYield`
  when blocking helpers need wall-clock time or cooperative scheduling.
- Declared `espidf` framework support in PlatformIO metadata while keeping the
  Arduino example functionality equivalent through example-local hooks.
- Reworked the ESP-IDF example from a short log-only app into a full interactive
  CLI parity build with matching commands, help text, colorized output,
  diagnostics, health reporting, probe/recover/reset flows, stress/self-test
  workflows, and raw register access.
- Doxyfile project metadata now matches `library.json` and references the
  maintained docs tree instead of removed template files.
- Reference documentation now separates compact ambient-light notes from full PDF/application-note extractions under `docs/extracted-md/` and `docs/pdf-extracted-md/`.
- Expanded the bring-up CLI to cover version info, reset flows, config/intcfg readback, cached samples, FIFO burst reads, interrupt setup, and self-test.
- Expanded the bring-up CLI again to cover decoded ID/config output, address switching,
  raw register-block reads, per-slot history reads, scaled lux helpers, threshold lux
  reporting, and scale/timing diagnostics.
- Standardized the bring-up CLI on the stronger family patterns for colored health
  reporting, `state` / `status` diagnostics, richer stress and stress-mix summaries,
  deeper selftest coverage, and raw threshold programming.
- Extended the bring-up CLI with consolidated `diag` reporting, optional periodic
  `healthmon` monitoring, helper-conversion commands (`adc2lux`, `raw2lux`,
  `thcalc`, `thdecode`), and stronger chip-specific stress/selftest auditing for
  counter continuity and conversion math.
- Tightened the compact health view and health monitor output so the state colors and
  one-line summaries match the verbose health reporting semantics more closely.
- Improved CLI readback UX so key setters now echo the resulting decoded chip state
  instead of only returning a status code.
- Corrected the modeled `FLAGS` semantics so write-to-clear affects only
  `CONVERSION_READY_FLAG`, while full sticky-flag clearing follows the datasheet's
  clear-on-read behavior.
- Corrected the scaled non-blocking read helpers so `CRC_ERROR` still returns the
  decoded lux / milli-lux / micro-lux outputs to the caller.
- Tightened the CLI contract check so the richer diagnostic commands remain present.
- Updated README and assumptions to document the bus-wide reset path and the intentionally omitted controller/application-layer behaviors.
- Hardened cached configuration setters so failed I2C write sequences roll back the cached driver state.
- Documented bounded blocking-read polling, public raw-register bounds, and stricter threshold lux input validation.
- Added `conversionReady(bool&)` so readiness checks can report transport errors while preserving the existing `bool conversionReady()` convenience helper.
- Failed `begin()` and startup probe paths now clear stale sample/config state and avoid seeding runtime health counters with setup traffic.
- Health behavior is now standardized on latched `OFFLINE`: normal public I2C operations return `BUSY` with `Driver is offline; call recover()` and do not touch I2C until `recover()` succeeds.
- Explicit recovery/reset bypass internals now use the shared `ScopedOfflineI2cAllowance` / `_reassertOfflineLatch()` procedure so failed recovery attempts that begin from `OFFLINE` keep the latch asserted.

### Fixed

- Made `recover()` record Device ID mismatches in health tracking instead of returning a semantic error with stale health state.
- Added a finite poll cap to blocking read helpers so a stalled injected millisecond source returns `TIMEOUT`.
- Rejected reserved raw-register addresses and register blocks before touching the bus.
- Rejected NaN and infinite lux threshold inputs before converting to packed threshold registers.
- Readiness-poll I2C errors are no longer flattened into `false`, not-ready, or timeout results by `tryRead*()` and blocking read helpers.
- Readiness claims, package metadata, SECURITY supported versions, and IDF
  example documentation now distinguish tested source/build coverage from
  pending pure ESP-IDF and target-hardware validation.
- The native ESP-IDF diagnostic CLI now configures stdin for nonblocking polling
  so driver `tick()` is not intentionally tied to blocking console input.
- The ESP-IDF example main component no longer exposes the repository root in
  its include directories.

## [1.0.0] - 2026-04-14

### Added

- Production-oriented OPT4001 driver with injected I2C transport and tracked health state.
- Support for PicoStar and SOT-5X3 package variants with address validation and package-specific lux scaling.
- Power-down, continuous, one-shot, and one-shot forced auto-range measurement flows.
- Decoded sample, burst FIFO readout, CRC verification, raw register helpers, and threshold / interrupt configuration.
- ESP32-S2 / ESP32-S3 bring-up CLI example and reusable `examples/common` integration helpers.
- Native tests plus CLI and timing contract checks.

[Unreleased]: https://github.com/janhavelka/OPT4001/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/janhavelka/OPT4001/releases/tag/v1.0.0
