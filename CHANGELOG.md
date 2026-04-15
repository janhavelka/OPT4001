# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `softReset()` and `resetAndReapply()` to align OPT4001 reset handling with the stronger sibling libraries while preserving the datasheet's general-call reset behavior.
- Sample-cache helpers: `getLastSample()`, `sampleTimestampMs()`, and `sampleAgeMs()`.
- `readDeviceId()` and `setVerifyCrc()` typed helpers.
- Broader native coverage for reset, CRC-policy, sample-cache, and device-ID paths.

### Changed

- Expanded the bring-up CLI to cover version info, reset flows, config/intcfg readback, cached samples, FIFO burst reads, interrupt setup, and self-test.
- Tightened the CLI contract check so the richer diagnostic commands remain present.
- Updated README and assumptions to document the bus-wide reset path and the intentionally omitted controller/application-layer behaviors.

## [1.0.0] - 2026-04-14

### Added

- Production-grade OPT4001 driver with injected I2C transport and tracked health state.
- Support for PicoStar and SOT-5X3 package variants with address validation and package-specific lux scaling.
- Power-down, continuous, one-shot, and one-shot forced auto-range measurement flows.
- Decoded sample, burst FIFO readout, CRC verification, raw register helpers, and threshold / interrupt configuration.
- ESP32-S2 / ESP32-S3 bring-up CLI example and reusable `examples/common` integration helpers.
- Native tests plus CLI and timing contract checks.

[Unreleased]: https://github.com/janhavelka/OPT4001/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/janhavelka/OPT4001/releases/tag/v1.0.0
