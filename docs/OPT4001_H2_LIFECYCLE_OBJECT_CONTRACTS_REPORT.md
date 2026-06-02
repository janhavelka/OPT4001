# OPT4001 H2 Lifecycle and Object Contracts Report

Date: 2026-06-02
Branch: `hardening/opt4001-industry-readiness`
Finding: H2 - Public raw register APIs can touch I2C while driver is uninitialized

## Old Behavior

- `readRegisters()` checked `_initialized` before parameter validation and I2C.
- `readRegister16()` and `writeRegister16()` validated register addresses but did
  not check `_initialized`, then called tracked I2C wrappers.
- A failed `begin()` could leave transport callbacks cached in `_config` while
  `_initialized == false`, so public single-register APIs could still touch I2C.
- `end()` set `_initialized = false` and left callbacks cached, so public
  single-register APIs could touch the bus after shutdown.
- `OPT4001` was implicitly copyable/movable even though it owns mutable runtime
  state, cached samples, health counters, callback pointers, and user contexts.
- Public headers did not state the thread-safety, ISR-safety, callback
  non-reentrancy, shared-bus serialization, lifecycle, offline, or blocking
  contracts clearly enough.

## New Behavior

- `readRegister16()` returns `Err::NOT_INITIALIZED` without I2C while `UNINIT`.
- `writeRegister16()` returns `Err::NOT_INITIALIZED` without I2C while `UNINIT`.
- The same guard applies before `begin()`, after failed `begin()`, and after
  `end()`.
- Internal register access now uses private tracked helpers:
  `_readRegister16Tracked()` and `_writeRegister16Tracked()`.
- `begin()`, `recover()`, `resetAndReapply()`, and normal typed APIs can use
  internal tracked register access without depending on public lifecycle APIs.
- Public raw-register access while `OFFLINE` returns `Err::BUSY` without I2C,
  matching normal public operation behavior.
- `probe()` remains a raw transport diagnostic that does not require
  `_initialized` and does not update health counters.
- `end()` remains a compatibility best-effort `void` API and is now documented as
  such.

## API Changes

- `OPT4001` remains default constructible.
- `OPT4001` copy construction, copy assignment, move construction, and move
  assignment are deleted.
- No new status-returning shutdown API was added. The existing `void end()` API
  is preserved and documented as best-effort.
- Public header comments now document:
  - non-thread-safe and non-ISR-safe instance contract,
  - callback non-reentrancy,
  - external serialization for shared buses,
  - lifecycle requirements for public I2C APIs,
  - offline behavior and explicit recovery/reset exceptions,
  - best-effort `end()`,
  - bounded blocking helper behavior.

## Test Matrix

| Test | Coverage |
| --- | --- |
| `test_raw_register_access_before_begin_is_guarded_without_bus_io` | Public `readRegister16()` and `writeRegister16()` before `begin()` return `NOT_INITIALIZED`, preserve output value, and do not touch fake bus. |
| `test_raw_register_access_after_failed_begin_is_guarded_without_bus_io` | Failed probe during `begin()` leaves raw public APIs guarded with no additional bus I/O. |
| `test_raw_register_access_after_end_is_guarded_without_bus_io` | After `end()`, raw public APIs return `NOT_INITIALIZED` with no bus I/O. |
| `test_raw_register_access_offline_matches_normal_operation_without_bus_io` | Raw public APIs return `BUSY` and do not touch bus while `OFFLINE`. |
| `test_probe_without_begin_uses_raw_transport_when_cached_config_present` | `probe()` can use cached callbacks while not initialized and does not update health. |
| Static assertions in `test/test_basic.cpp` | `OPT4001` is default constructible and not copy/move constructible or assignable. |

## Exact Command Results

| Command | Result |
| --- | --- |
| `git status --short` | Clean before Prompt 3 edits. |
| `git branch --show-current` | `hardening/opt4001-industry-readiness`. |
| `python tools\check_core_timing_guard.py` | `Core framework guard PASSED`. |
| `python tools\check_cli_contract.py` | `CLI contract PASSED`. |
| `python tools\check_idf_example_contract.py` | `IDF example contract PASSED`. |
| `python tools\check_version_header_contract.py` | `Version header contract PASSED`; generator reported `Version.h` up to date. |
| `python scripts\generate_version.py check` | `Up to date: C:\Users\Honza\Documents\Projects\OPT4001\include\OPT4001\Version.h`. |
| `python -m platformio test -e native` | Passed; `55 test cases: 55 succeeded in 00:00:02.368`. |
| `python -m platformio run -e esp32s3dev` | Passed; `esp32s3dev SUCCESS`; RAM `6.9%`, Flash `33.1%`. |
| `python -m platformio run -e esp32s2dev` | Passed; `esp32s2dev SUCCESS`; RAM `11.3%`, Flash `32.5%`. |
| `python -m platformio pkg pack` | Passed; wrote `OPT4001-1.0.0.tar.gz`; artifact removed after validation. |
| `idf.py --version` | Failed: `idf.py` is not recognized as a command in this shell. |
| `idf.py -C examples/esp_idf/basic set-target esp32s3 build` | Failed: `idf.py` is not recognized as a command in this shell. |
| `idf.py -C examples/esp_idf/basic set-target esp32s2 build` | Failed: `idf.py` is not recognized as a command in this shell. |

PlatformIO printed the existing obsolete-core warning for local Core `6.1.18`;
the warning did not fail tests or builds.

## Remaining Limitations

- Pure ESP-IDF builds were not executed locally because `idf.py` is not
  installed or not on `PATH`.
- `end()` is still best-effort and does not report whether the power-down write
  reached hardware. The behavior is documented rather than changed to avoid
  introducing a new shutdown API in Prompt 3.
- Probe/device-ID diagnostic precision remains for Prompt 4.
- Sample freshness, numeric bounds, FIFO/partial-state behavior, documentation
  honesty, and hardware validation remain for later prompts.

## H2 Status

H2 is fixed for public raw-register lifecycle safety. Public `readRegister16()`
and `writeRegister16()` no longer touch I2C while uninitialized, after failed
`begin()`, or after `end()`, and their offline behavior matches the documented
normal public operation policy.
