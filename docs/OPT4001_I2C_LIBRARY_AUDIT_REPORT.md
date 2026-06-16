# OPT4001 I2C Library Audit Report

Date: 2026-06-16

## Scope

This pass hardened packaging, transport-error fidelity, failed-`begin()`
diagnostics, offline status taxonomy, partial-configuration dirty reporting,
and TunnelMonitor-facing sample-path guidance. The companion poll-chunking pass
owns instruction budgets, staged sample/config jobs, and conversion/readiness
gate sequencing.

## Subagent Findings

- API/timing explorer reviewed `begin()`, `probe()`, `tick()`, `readBurst()`,
  `readSample()`, `tryReadSample()`, setters, side effects, and freshness
  semantics. It found no core framework-boundary violations, but identified two
  sample-path issues: INT freshness evidence should not force a FLAGS
  clear-on-read, and FIFO shadow slots should not require a fresh current
  sample token.
- Test/package explorer reviewed package exports, public headers, native tests,
  and clean-consumer behavior. `include/OPT4001/Version.h` was already tracked,
  but clean package import needed an explicit compile guard and tighter package
  export exclusions.

## Decisions And Fixes

- Packaging: `Version.h` remains checked in as the generated public header.
  `library.json` now excludes repository-local CI, docs extracts, prompts,
  tests, tools, and transient package artifacts from published packages. A new
  `tools/check_clean_consumer_package.py` packs the library, installs that
  tarball into a temporary native PlatformIO consumer, includes every public
  header, and links a core driver symbol.
- Failed `begin()` semantics: validation failures reset cached config/runtime
  to defaults. Probe or apply failures after valid config retain the normalized
  config for diagnostics and later `probe()`, while the lifecycle remains
  `UNINIT`. A partial multi-register apply marks `hardwareConfigDirty()` and
  preserves the original dirty status.
- Probe diagnostics: `probe()` remains raw and health-neutral. Transport
  statuses such as timeout, bus error, address NACK, and data NACK are returned
  with detail intact; only successful reads with unexpected fixed-pattern
  device ID bits return `DEVICE_ID_MISMATCH`.
- Offline taxonomy: `Err::OFFLINE` was added at the end of `Err` to avoid
  renumbering existing values. Normal public I2C APIs now return `OFFLINE`
  without touching the bus while health is latched offline. Real conversion/job
  contention still returns `BUSY`.
- Partial configuration: existing dirty diagnostics are retained and covered by
  targeted failure-position tests. Dirty state survives unrelated reads and is
  cleared only by successful `recover()` or `resetAndReapply()`.
- Sample path: `readBurst()` is documented as the preferred low-level primitive
  for TunnelMonitor-style shared `I2cTask` integration because it returns raw
  exponent, mantissa, ADC code, counter, CRC, lux, newest sample, and FIFO
  history in one fixed RESULT/FIFO block transfer when burst mode is enabled.
  `readSampleSlot(0)` keeps fresh-current-sample semantics; slots 1-3 are now
  direct FIFO shadow reads with independent CRC state and no fresh token gate.
  Freshness checks no longer read/clear FLAGS when configured INT evidence is
  already sufficient.

## Tests Added

- Clean package import/link test through `tools/check_clean_consumer_package.py`.
- Failed `begin()` probe/apply tests for retained diagnostic config, `UNINIT`
  lifecycle, untracked health, and dirty-state reporting.
- Error mapping matrix coverage preserving transport status/detail through
  `probe()`.
- Offline guard tests expecting `Err::OFFLINE` without bus traffic.
- FIFO shadow slot test proving slots 1-3 do not require or clear fresh
  evidence.
- INT freshness test proving asserted INT evidence does not clear FLAGS.

## Verification

- `python -m py_compile tools\check_clean_consumer_package.py tools\check_readiness_claims.py tools\check_public_api_docs.py tools\hil_opt4001_runner.py`
- `python tools\check_public_api_docs.py`
- `python tools\check_readiness_claims.py`
- `python scripts\generate_version.py check`
- `python tools\check_clean_consumer_package.py`
- `python -m platformio test -e native` passed with 111/111 native tests.
- `python -m platformio run -e esp32s3dev`
- `python -m platformio run -e esp32s2dev`

PlatformIO emitted its existing warning about multiple installed PIO cores; it
did not fail any validation command.
