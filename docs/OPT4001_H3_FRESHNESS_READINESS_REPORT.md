# OPT4001 H3 Freshness and Readiness Report

## Old Behavior

Continuous-mode readiness could be synthesized from elapsed configured time
alone. Once `_conversionReady` was set, repeated reads could keep returning the
same output-register contents as if they were new samples. One-shot reads could
also accept stale internal readiness after the nominal budget even when hardware
had not asserted completion. Blocking helpers used `Config::nowMs` when present,
but the contract did not require it before starting a conversion.

## New Definitions

| Term | Definition |
| --- | --- |
| Latest sample | Current `RESULT` register contents. It may be the same sample as a previous read. |
| Fresh sample | Newly observed conversion since the previous accepted fresh sample. Evidence is `CONVERSION_READY_FLAG`, configured SOT-5X3 INT assertion, or sample-counter advance. |
| Cached sample | Driver RAM copy from a successful decode, exposed through `getLastSample()`, `hasSample()`, `sampleTimestampMs()`, and `sampleAgeMs()`. It is not freshness proof. |
| Not ready | No fresh evidence is available. Fresh reads return `MEASUREMENT_NOT_READY`; try APIs return `OK` with `didRead=false`; readiness checks return `ready=false`. |

The 4-bit sample counter wraps modulo 16. Counter changes are freshness evidence,
including `15 -> 0`; identical counters are treated as not fresh. Counter-only
detection cannot distinguish a duplicate sample from a missed full modulo-16
wrap, so applications that need every conversion must poll faster than 16
conversions.

## API Changes

- Added `readLatestSample(Sample&)` for explicit current-register reads without
  freshness proof.
- Added `tryReadFreshSample(Sample&, bool&)` as an explicit fresh polling name.
  Existing `tryReadSample()` delegates to it for compatibility.
- Added `readFreshBlocking(...)`; existing `readBlocking(...)` delegates to it.
- `readSample()`, `readBurst()`, `tryReadSample()`, `tryReadFreshSample()`,
  `readBlocking()`, `readFreshBlocking()`, and lux helpers are fresh-sample APIs.
- Fresh reads consume readiness on `OK` and on `CRC_ERROR`, while still caching
  and returning the decoded sample data.
- Blocking reads return `INVALID_CONFIG` before starting a conversion if
  `Config::nowMs` is missing.

## Implementation Notes

- `tick()` and `conversionReady()` now treat elapsed time only as a poll gate.
  Completion requires hardware evidence.
- `readFlags()` captures conversion-ready evidence before hardware clears
  register `0x0C`; `readFlagsRaw()`, `clearFlags()`, and
  `clearConversionReadyFlag()` discard pending internal readiness evidence.
- Continuous mode restarts its timing window after a fresh sample is consumed.
- One-shot mode does not treat nominal elapsed time or hardware mode alone as
  completion; it requires the conversion-ready flag, optional INT evidence, or a
  counter advance after a previous fresh sample.
- INT evidence is optional and only used for SOT-5X3 when the application has
  configured `gpioRead`, `intPin`, output direction, and an every-conversion or
  FIFO-full interrupt function. PicoStar does not require or expose INT.

## Tests

Native tests were added or adjusted for:

- continuous first fresh sample,
- repeated same counter not fresh,
- counter wrap `15 -> 0`,
- same lux with changed counter,
- one-shot nominal elapsed without flag not ready,
- one-shot flag set returns sample,
- forced-auto one-shot not read early with a shorter normal one-shot timeout,
- fresh read advancing/consuming readiness,
- one-shot to continuous transition clearing stale readiness,
- blocking read missing `nowMs` returning `INVALID_CONFIG` before a write,
- CRC error on a fresh sample returning `CRC_ERROR` while consuming/caching it,
- existing try/lux tests updated so repeated calls use new fresh counters.

## Validation Results

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | Passed: `Core framework guard PASSED`. |
| `python tools/check_cli_contract.py` | Passed: `CLI contract PASSED`. |
| `python tools/check_idf_example_contract.py` | Passed: `IDF example contract PASSED`. |
| `python tools/check_version_header_contract.py` | Passed: `Version header contract PASSED`; reported `Version.h` up to date. |
| `python scripts/generate_version.py check` | Passed: reported `Version.h` up to date. |
| `python -m platformio test -e native` | Passed: 72 tests succeeded. PlatformIO warned that obsolete Core v6.1.18 is active while v6.1.19 also exists. |
| `python -m platformio run -e esp32s3dev` | Passed: `esp32s3dev SUCCESS`; RAM 6.9%, flash 33.1%. Same PlatformIO Core warning. |
| `python -m platformio run -e esp32s2dev` | Passed: `esp32s2dev SUCCESS`; RAM 11.3%, flash 32.6%. Same PlatformIO Core warning. |
| `python -m platformio pkg pack` | Passed: wrote `OPT4001-1.0.0.tar.gz`; artifact removed after validation. |
| `idf.py --version` | Not run successfully: PowerShell reported `idf.py` is not recognized. |
| `idf.py -C examples/esp_idf/basic set-target esp32s3 build` | Not run successfully: PowerShell reported `idf.py` is not recognized. |
| `idf.py -C examples/esp_idf/basic set-target esp32s2 build` | Not run successfully: PowerShell reported `idf.py` is not recognized. |

## Remaining Limitations

- Local pure ESP-IDF builds were attempted but not executed because `idf.py` is
  not available on `PATH` in this shell.
- Hardware validation is still pending, including real conversion-ready flag
  timing, auto-range overflow delay, SOT-5X3 INT pulse behavior, FIFO-full INT
  behavior, and PicoStar package behavior.
- Prompt 6 remains responsible for numeric correctness and independent vectors.
