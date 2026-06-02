# OPT4001 Industry-Readiness Exploration Report

Date: 2026-06-01
Repository: `C:/Users/Honza/Documents/Projects/OPT4001`
Branch: `audit/opt4001-industry-readiness-exploration`
Audit mode: exploration/report-only

## Executive Summary

The repository is best described as **engineering-grade but blocked from a production/industry-grade claim**.

The core design is materially stronger than a prototype: the driver is framework-neutral, uses injected non-owning I2C callbacks, exposes structured `Status`, has health tracking, has a substantial native fake-bus test suite, and Arduino ESP32-S2/S3 builds passed locally.

The production claim is blocked by several concrete issues:

- Fresh checkout/manual/ESP-IDF builds are not reproducible because `include/OPT4001/Version.h` is included by the public API but ignored by git and generated only by the PlatformIO flow.
- Public `readRegister16()` and `writeRegister16()` can touch I2C while the driver is `UNINIT`.
- Continuous/readiness state can over-report stale or premature samples instead of being tied to `CONVERSION_READY_FLAG`, INT, or counter freshness.
- `probe()` collapses transport diagnostics and only checks the low 12-bit DIDH field, not the full documented ID register pattern.
- Hardware, optical, interrupt, FIFO, address-pin, and pure ESP-IDF builds are not validated with recorded evidence.

No implementation fixes were made in this pass.

## Repository Map

Important paths:

| Area | Paths | Notes |
| --- | --- | --- |
| Public headers | `include/OPT4001/CommandTable.h`, `Config.h`, `OPT4001.h`, `Status.h`, local generated `Version.h` | `Version.h` exists in the working tree but is ignored and not tracked. |
| Source | `src/OPT4001.cpp` | Single implementation file. |
| Arduino examples | `examples/01_basic_bringup_cli/main.cpp`, `examples/common/*.h` | Bring-up/diagnostic CLI using Arduino APIs and `Wire` only in examples. |
| ESP-IDF example | `examples/esp_idf/basic/CMakeLists.txt`, `examples/esp_idf/basic/main/*` | Native IDF by inspection: `app_main`, `driver/i2c_master.h`, fixed C buffers. |
| Tests | `test/test_basic.cpp`, `test/stubs/Arduino.h`, `test/stubs/Wire.h` | Native Unity fake-bus tests. |
| Tools | `tools/check_core_timing_guard.py`, `tools/check_cli_contract.py`, `tools/check_idf_example_contract.py` | Static contract checks. |
| Version tooling | `scripts/generate_version.py`, `library.json`, ignored `include/OPT4001/Version.h` | PlatformIO pre-script generates version header. |
| PlatformIO metadata | `platformio.ini`, `library.json` | Arduino ESP32-S3/S2 and native environments. |
| ESP-IDF metadata | `CMakeLists.txt`, `idf_component.yml` | Root component metadata exists; no CMake version-generation step. |
| CI | `.github/workflows/ci.yml` | Builds Arduino ESP32-S3/S2, native tests, CLI contract, package; does not build pure IDF or run IDF example contract. |
| Docs | `README.md`, `CHANGELOG.md`, `ASSUMPTIONS.md`, `docs/*.md`, `docs/*.pdf` | Includes local datasheet and extracted summaries. |
| Security metadata | `SECURITY.md` | Supported version still says `0.3.x` while package is `1.0.0`. |

Classification: **framework-neutral core plus Arduino and ESP-IDF adapters/examples**.

## Datasheet / Documentation Sources

Local sources used:

- `docs/OPT4001_datasheet.pdf`
- `docs/OPT4001_datasheet.md`
- `docs/pdf-extracted-md/OPT4001_datasheet.md`
- `docs/extracted-md/00_document_inventory.md`
- `docs/extracted-md/01_chip_overview.md`
- `docs/extracted-md/02_pinout_and_signals.md`
- `docs/extracted-md/03_electrical_and_timing.md`
- `docs/extracted-md/04_protocol_commands_and_transactions.md`
- `docs/extracted-md/05_register_map.md`
- `docs/extracted-md/06_modes_interrupts_status_and_faults.md`
- `docs/extracted-md/07_initialization_reset_and_operational_notes.md`
- `docs/extracted-md/08_variant_differences_and_open_questions.md`
- `docs/AN_high_speed_resolution.md`
- `docs/AN_light_detection.md`
- `docs/AN_picostar_package.md`

Key local datasheet references used:

- Package differences, address, INT, LSB: `docs/OPT4001_datasheet.md:43`.
- VDD recommended operating range and 5.5 V input distinction: `docs/OPT4001_datasheet.md:73`, `docs/OPT4001_datasheet.md:106`.
- I2C address table and ADDR=SCL caveat: `docs/OPT4001_datasheet.md:169`.
- Register map/defaults: `docs/OPT4001_datasheet.md:252`.
- Mode behavior and one-shot trigger timing: `docs/OPT4001_datasheet.md:363`.
- Auto-range overflow can extend conversion completion: `docs/OPT4001_datasheet.md:428`.
- Conversion times: `docs/OPT4001_datasheet.md:453`.
- Result/lux math: `docs/OPT4001_datasheet.md:480`.
- CRC equations: `docs/OPT4001_datasheet.md:518`.
- Threshold math: `docs/OPT4001_datasheet.md:552`.
- INT modes: `docs/OPT4001_datasheet.md:591`.
- FIFO behavior: `docs/OPT4001_datasheet.md:626`.
- Flags clear behavior: `docs/OPT4001_datasheet.md:780`.
- Overload and optical placement/window notes: `docs/OPT4001_datasheet.md:813`.

## Supported Feature Inventory

| Feature | Present in API | Present in docs | Tested | Notes |
| --- | --- | --- | --- | --- |
| Basic lux read | Yes | Yes | Yes, fake bus | `readLux()`, `readBlockingLux()` exist. Hardware evidence absent. |
| Raw result read | Yes | Yes | Yes, fake bus | `Sample` exposes raw regs, exponent, mantissa, ADC codes, counter, CRC. |
| Package variant selection | Yes | Yes | Partial | PicoStar and SOT-5X3 represented; tests reject some invalid combos only. |
| I2C address selection | Partial | Yes | Partial | Validation supports `0x44/0x45/0x46`; no full valid matrix or address-routing fake test. |
| Conversion-time selection | Yes | Yes | Partial | 12 enum values exist; tests do not independently sweep all values. |
| Continuous mode | Yes | Yes | Partial/Fail | API exists, but readiness is software-time/sticky rather than hardware flag/counter based. |
| One-shot/trigger mode | Register one-shot yes; INT hardware trigger app-owned | Yes | Partial | Register-trigger supported. INT pulse generation intentionally not owned by core. |
| FIFO | Yes | Yes | Partial | Burst and slot APIs exist; CRC/freshness/empty semantics incomplete. |
| Interrupt thresholds | Yes | Yes | Partial | Register helpers exist; no hardware interrupt validation. |
| Standby/shutdown | Yes | Yes | Partial | `POWER_DOWN`, `end()`, and reset paths exist; reset wait/hardware evidence absent. |
| Recovery | Yes | Yes | Partial | Health and `recover()` exist; successful offline-to-ready recovery test missing. |

## Architecture Assessment

Strengths:

- Core headers and `src/` are framework-neutral by inspection and by `python tools/check_core_timing_guard.py`.
- No direct `Arduino.h`, `Wire.h`, ESP-IDF headers, FreeRTOS, `String`, `Serial`, heap allocation, logging, global bus, or pin ownership in `include/` or `src/`.
- `Config` injects `i2cWrite`, `i2cWriteRead`, `nowMs`, `cooperativeYield`, and `gpioRead` callbacks (`include/OPT4001/Config.h:12`).
- I2C wrappers pass the configured timeout to callbacks (`src/OPT4001.cpp:1669`, `src/OPT4001.cpp:1682`).
- Structured status codes cover OK, invalid config/param, timeout, measurement not ready, CRC, I2C NACK/timeout/bus, busy, and health-related errors (`include/OPT4001/Status.h:10`).
- Health tracking is centralized in tracked wrappers (`src/OPT4001.cpp:1697`, `src/OPT4001.cpp:1782`).

Blocking architecture gaps:

- Public `readRegister16()` / `writeRegister16()` do not check `_initialized`, unlike `readRegisters()` (`src/OPT4001.cpp:1431`, `src/OPT4001.cpp:1444`, `src/OPT4001.cpp:1457`).
- Copy/move semantics are implicit. `OPT4001` owns mutable runtime state and copied callback/user pointers, but the class does not delete or define copy/move behavior (`include/OPT4001/OPT4001.h:125`, `include/OPT4001/OPT4001.h:350`).
- Public headers lack the thread-safety, ISR-safety, reentrancy, callback-recursion, and blocking contracts that production users need.
- `Version.h` is public API but is ignored and not generated by root CMake, blocking clean manual/ESP-IDF consumption.

## OPT4001 Device-Correctness Assessment

Mostly aligned:

- PicoStar and SOT-5X3 variants are represented (`include/OPT4001/Config.h:33`).
- PicoStar address is restricted to `0x45`; SOT-5X3 supports `0x44`, `0x45`, `0x46` (`src/OPT4001.cpp:16`).
- Register addresses/defaults match local documentation (`include/OPT4001/CommandTable.h:22`).
- Result format decode follows exponent + 20-bit mantissa + counter + CRC (`src/OPT4001.cpp:1759`).
- Package-specific lux LSBs match local datasheet summaries (`include/OPT4001/CommandTable.h:150`).
- Conversion time enums cover 600 us through 800 ms (`include/OPT4001/Config.h:53`, `include/OPT4001/CommandTable.h:134`).
- Quick wake, thresholds, INT config, flags, FIFO burst, and general-call reset are modeled.

Not production-ready:

- Continuous mode readiness is not tied to the device's `CONVERSION_READY_FLAG`, INT, or sample counter; the code can mark ready by elapsed nominal time and leave readiness sticky after reads (`src/OPT4001.cpp:203`, `src/OPT4001.cpp:447`, `src/OPT4001.cpp:489`).
- `probe()` and `recover()` validate only DIDH low 12 bits, not DIDL/reserved fixed bits (`src/OPT4001.cpp:253`, `src/OPT4001.cpp:268`). The decoded helper checks DIDL, but the presence path does not (`src/OPT4001.cpp:1473`).
- `probe()` collapses transport diagnostics into `DEVICE_NOT_FOUND` for most I2C failures (`src/OPT4001.cpp:253`).
- Threshold conversion accepts 4-bit threshold exponents but returns `uint32_t`, which can wrap for register-valid threshold encodings (`src/OPT4001.cpp:1656`).
- Hardware INT, FIFO-full pulse, threshold interrupt behavior, address-pin combinations, under-glass behavior, and reference lux accuracy are not validated.

## Lux Conversion and Numeric Correctness

Valid-path sample math matches the local datasheet:

- `MANTISSA = (RESULT_MSB << 8) | RESULT_LSB`
- `ADC_CODES = MANTISSA << EXPONENT`
- PicoStar lux = `ADC_CODES * 312.5e-6`
- SOT-5X3 lux = `ADC_CODES * 437.5e-6`

Evidence:

- Datasheet summary: `docs/OPT4001_datasheet.md:480`.
- Implementation: `src/OPT4001.cpp:1518`, `src/OPT4001.cpp:1759`.
- Constants: `include/OPT4001/CommandTable.h:150`.

Gaps:

- `rawToLux(uint8_t exponent, uint32_t mantissa)` shifts without validating exponent or mantissa bounds (`src/OPT4001.cpp:1522`). Caller-supplied exponent >= 32 is undefined behavior in C++.
- Sample decode extracts a 4-bit exponent from registers and shifts it directly; datasheet result exponent is 0-8, but the code does not reject 9-15 (`src/OPT4001.cpp:1763`).
- `thresholdToAdcCodes()` can wrap beyond 32 bits for valid 4-bit threshold exponents (`src/OPT4001.cpp:1656`).
- `readBurst()` stops decoding later slots after the first CRC warning, so it does not deliver per-sample CRC visibility for all four samples (`src/OPT4001.cpp:587`).
- Rounding policy is implicit: micro-lux and milli-lux helpers round to nearest; threshold packing rounds to nearest ADC code before truncating into exponent/result (`src/OPT4001.cpp:687`, `src/OPT4001.cpp:1626`).
- Tests use implementation-derived CRC values and synthetic vectors, not independent datasheet/hardware vectors (`test/test_basic.cpp:194`, `test/test_basic.cpp:528`).

## Timing and Determinism

I2C transaction count below treats one callback invocation as one transaction. The practical worst-case bus bound depends on whether the injected transport honors `_config.i2cTimeoutMs`.

| API | Blocks? | I2C transactions | Other waits | Worst-case bound | Notes |
| --- | --- | ---: | --- | --- | --- |
| `begin()` | Yes | 5 success path | None | 5 x `i2cTimeoutMs` | 1 raw ID read + 4 config writes. |
| `tick()` | Sometimes | 0 normally, 1 for elapsed one-shot | None | 1 x timeout | One-shot after budget polls CONFIG. |
| `end()` | Sometimes | 1 raw write | None | 1 x timeout | Best-effort, status discarded. |
| `probe()` | Yes | 1 raw read | None | 1 x timeout | Collapses most transport errors. |
| `recover()` | Yes | 5 success path | None | 5 x timeout | Tracks failures after initialization. |
| `softReset()` | Yes | 1 general-call write | No post-reset wait | 1 x timeout | Bus-wide side effect. |
| `resetAndReapply()` | Yes | 5 success path | No post-reset wait | 5 x timeout | General-call reset plus reapply. |
| `startConversion()` | Yes | 1 write | No conversion wait | 1 x timeout | Returns `IN_PROGRESS`. |
| `conversionReady()` | Sometimes | 0 before budget/continuous time path, 1 one-shot poll | None | 1 x timeout | Continuous path is software-time only. |
| `readSample()` / `readLux()` / scaled reads | Yes | 2 sample reads, optional 1 readiness poll | None | 3 x timeout | CRC error still returns decoded sample for single-sample APIs. |
| `readBurst()` | Yes | 1 burst read or 8 register reads, optional 1 readiness poll | None | 2 to 9 x timeout | Fixed-size; no unbounded FIFO drain. |
| `readSampleSlot()` | Yes | 2 reads, optional 1 readiness poll | None | 3 x timeout | Slot 0 caches latest sample. |
| `readBlocking()` / `readBlockingLux()` | Yes | Conversion write + repeated readiness/sample path | Deadline + poll cap | `timeoutMs` if `nowMs` advances; otherwise finite poll cap | Without `cooperativeYield`, can CPU-spin. |
| `tryReadSample()` / `tryReadLux()` | Sometimes | Single readiness/sample path only | None | Up to 3 x timeout | No loop; not-ready returns OK with `didRead=false`. |
| `readFlags()` / `readFlagsRaw()` / `clearFlags()` | Yes | 1 read | None | 1 x timeout | Reads clear sticky flags on device. |
| `clearConversionReadyFlag()` | Yes | 1 write | None | 1 x timeout | Writes non-zero to FLAGS. |
| `readIntPinAsserted()` | Callback-bound | 0 I2C | GPIO callback | callback-defined | SOT-5X3 only. |
| Setters using `_applyConfig()` | Yes | 4 writes | None | 4 x timeout | Includes low/high thresholds, INT config, CONFIG. |
| `setThresholds()` | Yes | 2 writes | None | 2 x timeout | Partial failure can dirty hardware/cache relation. |
| `getThresholds()` | Yes | 2 reads | None | 2 x timeout | Updates cached thresholds. |
| `readConfiguration()` / `readIntConfiguration()` | Yes | 1 read | None | 1 x timeout | Raw aliases same. |
| `writeConfiguration()` / `writeIntConfiguration()` | Yes | 1 write | None | 1 x timeout | `writeConfiguration()` can start one-shot. |
| `readRegisters()` | Yes | 1 read block | Bounded validation loop | 1 x timeout | Rejects reserved gaps before I2C. |
| `readRegister16()` / `writeRegister16()` | Yes | 1 | None | 1 x timeout | Missing initialized-state guard. |
| Utility methods | No | 0 | Bounded CPU only | CPU bounded | Threshold conversion loop bounded by exponent max. |

Determinism notes:

- No `delay()` in library code.
- Blocking helpers have a deadline plus finite poll cap (`src/OPT4001.cpp:99`, `src/OPT4001.cpp:716`).
- If `Config::nowMs` is absent, `_nowMs()` returns 0 and the poll cap, not wall time, bounds blocking helpers (`src/OPT4001.cpp:1997`).
- One-shot budget accounts for conversion time, standby recovery when quick-wake is off, and forced-auto extra time (`src/OPT4001.cpp:1611`).
- Auto-range overflow can extend hardware completion beyond nominal conversion time per datasheet; continuous readiness does not account for this.

## Status/Error/Health Model

Strong points:

- All fallible public APIs return `Status` or are explicit `void` lifecycle calls.
- Status enum distinguishes invalid config/param, not initialized, timeout, I2C NACK/timeout/bus, CRC error, busy, not ready, device not found, and ID mismatch.
- Tracked wrappers update health centrally and do not count invalid config/param failures (`src/OPT4001.cpp:1697`, `src/OPT4001.cpp:1782`).
- Offline is latched and normal public tracked I2C returns `BUSY` without touching the bus while offline.

Gaps:

- `probe()` collapses NACK/timeout/bus errors into `DEVICE_NOT_FOUND`, losing status precision during `begin()`.
- IDF example transport maps `ESP_ERR_INVALID_RESPONSE` to generic `I2C_ERROR` rather than `I2C_NACK_ADDR` or a NACK-specific status (`examples/esp_idf/basic/main/Opt4001IdfI2cTransport.cpp:19`).
- Offline status is `BUSY`, same enum family as conversion busy; caller must inspect driver state/message.
- Saturation is only visible through `Flags::overload`; sample reads do not attach overload status.
- FIFO empty/stale history is not distinguishable.
- Dirty partial configuration state is not exposed.

## Partial-State and Cache Consistency

Setters commonly roll back `_config` after an I2C failure, which is good for software consistency. However, multi-register operations can already have changed the device before a later write fails. Examples:

- `setThresholds()` writes low threshold first, then high threshold. If high fails, `_config` rolls back while hardware may retain the new low threshold (`src/OPT4001.cpp:1091`).
- `_applyConfig()` writes four registers in sequence; failure after one or more successful writes can leave hardware partially updated (`src/OPT4001.cpp:1853`).

There is no exposed dirty/resync flag, no "configuration may be partially applied" status, and no required readback verification after multi-register updates.

## Thread-Safety and ISR-Safety Contract

The README states the driver is not internally synchronized or ISR-safe and should be called from one task or protected by an application lock (`README.md:154`). That contract is not carried into the public Doxygen declarations in `include/OPT4001/OPT4001.h`.

Production API documentation should explicitly state:

- Driver instances are not thread-safe.
- Public APIs are not ISR-safe.
- Transport callbacks are called synchronously and must not re-enter the same driver instance.
- The application owns bus locking and timeout policy.
- Copying/moving initialized driver instances is unsupported unless explicitly designed.

## ESP-IDF Port Assessment

- Is there a pure ESP-IDF component? **Partial.** Root `CMakeLists.txt` and `idf_component.yml` exist, but `Version.h` generation is missing from CMake and `Version.h` is ignored.
- Is there a pure ESP-IDF example? **Yes by inspection.** `examples/esp_idf/basic` uses `app_main`, `driver/i2c_master.h`, native GPIO/timer/task APIs, and fixed C buffers.
- Does it avoid Arduino dependencies? **Yes by inspection and local contract check.**
- Does it use external bus ownership? **Example owns its bus; library core remains non-owning.** Production shared-bus locking is application-owned and not demonstrated.
- Does it map IDF errors precisely? **Partial.** Timeout maps to `I2C_TIMEOUT`; NACK maps to generic `I2C_ERROR`.
- Does it handle locking/timeouts? **Partial.** Timeouts are passed to IDF transactions; no shared-bus locking example.
- Is it built in CI? **No.** CI does not run `idf.py build` and does not run `check_idf_example_contract.py`.

Additional notes:

- Local `idf.py --version` failed because `idf.py` was not installed, so no pure IDF build was validated.
- The IDF CLI uses `getchar()` in the same loop as `device.tick()`; without nonblocking stdin setup or a separate task, periodic sensor work can stall.
- No `sdkconfig.defaults` was found.

## Arduino Integration Assessment

Arduino integration is in better shape than ESP-IDF:

- PlatformIO Arduino builds for `esp32s3dev` and `esp32s2dev` passed locally.
- CI builds both Arduino environments.
- Arduino example is clearly a bring-up CLI under `examples/01_basic_bringup_cli`.
- Core library does not include Arduino headers.

Gaps:

- The example is diagnostic/bring-up, not production shared-bus application code.
- Example callback behavior should be treated as adapter sample code, not proof of production timeout/locking policy.
- Hardware validation on ESP32-S2/S3 was not run.

## Tests and CI Coverage

Run locally:

| Command | Result |
| --- | --- |
| `python --version` | `Python 3.12.10` |
| `python -m platformio --version` | `PlatformIO Core, version 6.1.18` |
| `python tools/check_core_timing_guard.py` | Passed: `Core framework guard PASSED` |
| `python tools/check_cli_contract.py` | Passed: `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | Passed: `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | Passed: `Version.h` up to date in working tree |
| `python -m platformio test -e native` | Passed: 50 test cases, 50 succeeded |
| `python -m platformio run -e esp32s3dev` | Passed: build success |
| `python -m platformio run -e esp32s2dev` | Passed: build success |
| `python -m platformio pkg pack` | Passed; generated `OPT4001-1.0.0.tar.gz`, then artifact was removed |
| `idf.py --version` | Failed: command not found |

Present in CI:

- PlatformIO Arduino build matrix for `esp32s3dev` and `esp32s2dev`.
- Native PlatformIO Unity tests.
- `tools/check_core_timing_guard.py`.
- `tools/check_cli_contract.py`.
- `pio pkg pack`.

Missing from CI:

- `tools/check_idf_example_contract.py`.
- Pure ESP-IDF `idf.py` build for ESP32-S2 and ESP32-S3.
- Hardware-in-the-loop validation.
- Doxygen/doc generation check.
- Independent datasheet vector/fault-injection coverage gate.

Not run and why:

- `idf.py -C examples/esp_idf/basic set-target esp32s3 build`: `idf.py` is not installed locally.
- `idf.py -C examples/esp_idf/basic set-target esp32s2 build`: `idf.py` is not installed locally.
- Hardware upload/monitor/logic analyzer tests: no hardware validation was requested or available in this audit pass.

Coverage gaps:

- Valid address matrix and callback address routing.
- All 12 conversion times with independent timing oracle.
- Independent CRC vectors.
- Max/min lux vectors, package scaling vectors, threshold reset/boundary vectors.
- NACK address/data, bus arbitration, and partial Wire read/write errors.
- Successful recovery from `OFFLINE`.
- FIFO CRC failure and stale/empty history semantics.
- Null `nowMs` behavior.
- Pure IDF build and runtime behavior.

## Documentation Assessment

Good:

- README documents the broad API surface, callback model, no core framework dependencies, CRC warning behavior, offline behavior, raw register bounds, clear-on-read flags, and reset caveat.
- `ASSUMPTIONS.md` records several interpretation decisions.
- Extracted datasheet docs are useful and repo-local.

Needs correction:

- "Production-grade" claims in `README.md`, `library.json`, and `idf_component.yml` are premature.
- README claims ESP-IDF component builds, but clean ESP-IDF builds are not validated and `Version.h` is not generated by CMake.
- README/IDF docs overstate IDF CLI parity and "validation status".
- Public headers do not contain enough of the operational contract.
- `readFlagsRaw()` lacks the clear-on-read warning carried by `readFlags()`.
- `Sample::lux` lacks a unit comment.
- Hardware/optical validation matrix is missing from committed docs.
- `SECURITY.md` supported version is stale (`0.3.x` vs current `1.0.0`).
- Local datasheet summaries have inconsistencies: FIFO order wording, PicoStar app-note temperature summary, and one full-scale table value.

## Hardware Validation Matrix

None of the following was performed in this audit. All entries are pending.

| Area | Procedure | Expected result | Status |
| --- | --- | --- | --- |
| Probe and ID | Known-good OPT4001 on ESP32-S2 and ESP32-S3. Run `scan`, `begin`, `probe`, `id`, `selftest`. | ACK at expected address; DEVICE_ID `0x0121`; no probe health side effects. | Pending |
| Package/addresses | PicoStar fixed `0x45`; SOT-5X3 ADDR=GND/VDD/SDA plus ADDR=SCL caveat. | Valid combos initialize; invalid combos reject; SCL duplicate documented. | Pending |
| Dark condition | Cover sensor. Run repeated reads. | Low nonnegative lux, sane CRC rate. | Pending |
| Stable indoor light | Measure office light with reference meter. | Stable readings within expected sensor/system tolerance. | Pending |
| High light/saturation | Bright lamp or sunlight; sweep fixed ranges. | Overload flag behavior appears in low fixed ranges; auto-range recovers. | Pending |
| Conversion sweep | Test all 12 conversion times from 600 us to 800 ms. | Sample periods match selected timings within host scheduling limits. | Pending |
| Continuous mode | Run continuous at several conversion times; capture counters/timestamps. | No stale repeats when polling slower than conversion rate; counter deltas plausible. | Pending |
| One-shot regular | Trigger one-shot via register path. | Completion bounded by configured timing budget. | Pending |
| One-shot forced auto | Trigger forced auto-range one-shot. | Extra auto-range budget accounted for. | Pending |
| Quick wake | Compare one-shot with quick wake off/on. | Standby recovery delay behavior matches expectation. | Pending |
| FIFO history | Fast continuous conversion, read burst and slots. | Newest/FIFO counters form plausible four-sample history. | Pending |
| Threshold interrupt | SOT-5X3 INT wired to GPIO/logic analyzer; configure threshold window. | Latch/transparent/fault-count behavior captured. | Pending |
| Every-conversion pulse | Configure INT_CFG=1 and logic analyzer. | Approximately one pulse per conversion. | Pending |
| FIFO-full pulse | Configure INT_CFG=3 and logic analyzer. | Pulse every four conversions. | Pending |
| INT hardware trigger | Configure INT as input and pulse from board layer. | One-shot starts; no completion INT while pin is input. | Pending |
| Unplug/replug | Disconnect sensor during reads, reconnect, recover. | DEGRADED/OFFLINE behavior; no bus touch while offline; recovery returns READY. | Pending |
| Brownout/reset | Power-cycle sensor; run `reset`/`resetreapply`. | Reset defaults observed; cached config reapplied. | Pending |
| Reference lux meter | Bare device beside calibrated meter. | Package scaling and application tolerance recorded. | Pending |
| Under glass/window | Known optical window/cover glass. | Attenuation recorded; application correction validated. | Pending |
| Framework matrix | Repeat subset on Arduino and native IDF, ESP32-S2 and ESP32-S3. | Equivalent behavior or documented target-specific gaps. | Pending |

## Readiness Scorecard

| Area | Rating | Notes |
| --- | --- | --- |
| Core framework neutrality | Strong | Guard passed; no framework headers in core. |
| I2C ownership/injection | Strong | Non-owning callbacks; application owns bus. |
| OPT4001 datasheet correctness | Medium | Register model mostly aligned; readiness, ID validation, thresholds need work. |
| Lux math correctness | Medium | Valid path matches formula; unchecked shifts/threshold overflow/vector gaps remain. |
| Timing/determinism | Medium | Bounded loops; blocking can CPU-spin without hooks; continuous timing is too software-driven. |
| Status/error model | Medium | Structured status exists; probe and IDF NACK precision weak. |
| Partial-state handling | Weak | Rollback exists but hardware/cache dirty state is not exposed. |
| FIFO support | Medium | API exists; CRC/freshness/empty semantics incomplete. |
| Interrupt support | Medium | Register support exists; no hardware validation. |
| Package variant handling | Good | Variants and addresses modeled; full address tests/hardware validation missing. |
| ESP-IDF readiness | Weak | Metadata/example exist; clean build blocked/unproven by ignored `Version.h` and no local/CI IDF build. |
| Arduino readiness | Good | S2/S3 builds passed; example is diagnostic. |
| Tests/fault injection | Medium | 50 native tests pass; independent vectors and error injection gaps remain. |
| Documentation honesty | Medium | Good content, but production/validation/IDF parity claims overstate evidence. |
| Hardware validation | Weak | No hardware or optical validation evidence captured. |

## OPT4001 Device-Specific Checklist

### Package and Electrical Contract

| Item | Result | Notes |
| --- | --- | --- |
| PicoStar/YMN and SOT-5X3/DTS package differences represented or explicitly scoped | PASS | `PackageVariant` exists and controls scaling/address validation. |
| VDD 1.6 V to 3.6 V documented separately from 5.5 V tolerant I/O | PARTIAL | Local datasheet docs have it; README/API docs need clearer integration guidance. |
| Pull-up voltage recommendations documented | PARTIAL | Datasheet/app docs mention open-drain/pullups; README is sparse. |
| ADDR pin support correct for SOT-5X3 | PASS | 0x44/0x45/0x46 supported; ADDR=SCL caveat documented. |
| INT pin support correct for SOT-5X3 and absent/not claimed for PicoStar | PARTIAL | GPIO read is package-gated; register config remains generally available. |
| Optical sensing area/orientation documented | PARTIAL | Local docs cover it; README/API validation workflow lacks it. |
| Storage/temperature optical discoloration warning documented | PARTIAL | Datasheet docs contain warning; PicoStar app-note summary conflicts. |

### I2C/Register Correctness

| Item | Result | Notes |
| --- | --- | --- |
| 7-bit I2C address handling is correct | PASS | Public config uses 7-bit addresses. |
| Register addresses and field masks match datasheet | PASS | No mismatch found in core constants. |
| Reset/default values are known or explicitly not assumed | PASS | Defaults are in `CommandTable.h`. |
| Multi-byte register read/write byte order is correct | PASS | MSB-first implementation matches docs. |
| Burst read support matches datasheet | PASS | 16-byte read from result/FIFO window. |
| Unsupported/reserved bits masked/preserved as required | PARTIAL | CONFIG/INT validation exists; raw writes still allow public register writes within broad bounds. |

### Measurement and Lux Conversion

| Item | Result | Notes |
| --- | --- | --- |
| Raw result format decoded correctly | PASS | Valid-path decode matches datasheet. |
| Mantissa/exponent fields decoded correctly | PASS | Valid path matches; invalid exponent not rejected. |
| Package-dependent lux scale correct | PASS | Constants match local datasheet. |
| Full-scale range/automatic range behavior correct | PARTIAL | Constants exist; auto-range completion timing/freshness not robust. |
| Saturation/overflow detected and reported | PARTIAL | Flag decode exists; sample reads do not surface overload status. |
| Lux unit, precision, rounding, numeric limits documented | PARTIAL | Units/rounding/numeric limits need public header contract. |
| Datasheet vectors or independently calculated vectors exist in tests | FAIL | Tests rely on implementation-derived helpers. |

### Modes and Timing

| Item | Result | Notes |
| --- | --- | --- |
| Continuous mode is correct | FAIL | Readiness can be software-time/sticky and stale. |
| One-shot mode is correct | PARTIAL | Register trigger exists; hardware validation missing. |
| Hardware trigger mode handled only where package supports it | PARTIAL | INT direction can be configured; pulse generation is application-owned. |
| Conversion-time enum covers all 12 times | PASS | 600 us through 800 ms present. |
| Blocking reads bounded and require valid timebase if needed | PARTIAL | Bounded by poll cap; `nowMs` is optional and not required up front. |
| Nonblocking reads distinguish latest/stale/not-ready data | PARTIAL | Not-ready is distinct; stale/fresh contract is weak. |
| Startup/standby wake timing documented | PARTIAL | Budget modeled; reset wait/hardware timing validation absent. |

### FIFO

| Item | Result | Notes |
| --- | --- | --- |
| FIFO registers and depth/behavior understood | PARTIAL | API/model exists; one doc summary contradicts FIFO order. |
| FIFO empty/full/overflow behavior reported precisely | NOT IMPLEMENTED | No FIFO empty/full status; full only via INT config/flags. |
| Burst reads implemented or explicitly not implemented | PASS | Implemented. |
| FIFO drain cannot monopolize bus unboundedly | PASS | Fixed 16-byte burst or fixed four-slot fallback. |
| FIFO tests exist or are planned | PARTIAL | Happy-path fake tests exist; CRC/stale/hardware tests missing. |

### Interrupts/Thresholds

| Item | Result | Notes |
| --- | --- | --- |
| Threshold registers encoded correctly | PARTIAL | Valid path ok; high exponent conversion can overflow. |
| Interrupt pin open-drain/pull-up behavior documented | PARTIAL | Datasheet docs yes; README/API should be clearer. |
| Latching/persistence/fault-count behavior correct | PARTIAL | Register fields supported; hardware validation missing. |
| Threshold comparisons tested or have hardware validation plan | PARTIAL | Fake tests and this report matrix exist; no hardware evidence. |
| GPIO interrupt capture part of validation matrix | PASS | Included as pending matrix item. |

### Production Integration

| Item | Result | Notes |
| --- | --- | --- |
| Core is framework-neutral | PASS | Guard passed. |
| Transport is injected/non-owning | PASS | Required callbacks. |
| ESP-IDF component pure IDF if present | PARTIAL | Example pure; build reproducibility unproven/blocked. |
| Arduino example labeled diagnostic unless production-grade | PARTIAL | Path says bring-up; README should avoid implying production application. |
| Shared-bus locking external and documented | PARTIAL | General guidance exists; example does not demonstrate locking. |
| Thread/ISR contracts documented | PARTIAL | README yes; public headers no. |
| Recovery after bus/device fault specified | PARTIAL | Health/recover exist; tests/hardware matrix incomplete. |

## High-Severity Findings

### H1. Fresh checkout/manual/ESP-IDF builds are not reproducible

Severity: High

Evidence:
- `include/OPT4001/OPT4001.h:11` includes `OPT4001/Version.h`.
- `.gitignore:41` ignores `include/OPT4001/Version.h`.
- `git ls-files include/OPT4001` lists only `CommandTable.h`, `Config.h`, `OPT4001.h`, and `Status.h`.
- `platformio.ini:8` runs `scripts/generate_version.py`, but root `CMakeLists.txt:1` has no equivalent generation step.
- README manual and ESP-IDF install claims are at `README.md:59` and `README.md:63`.

Impact:
- Clean manual installs and pure ESP-IDF builds can fail before compiling the driver.
- The repository cannot honestly claim ESP-IDF component readiness until version generation is handled outside PlatformIO.

Recommended remediation:
- Either track a deterministic generated `Version.h`, provide a committed fallback header, or add CMake/generic build generation that works for ESP-IDF and manual consumers.
- Add a fresh-checkout CI job that deletes ignored generated files and builds both manual/native and ESP-IDF paths.

Suggested tests:
- `git clean -xfd` in CI-safe clone, then `idf.py -C examples/esp_idf/basic set-target esp32s3 build`.
- Compile a minimal CMake/native consumer including `OPT4001/OPT4001.h`.

### H2. Public raw register APIs can touch I2C while driver is uninitialized

Severity: High

Evidence:
- `readRegisters()` checks `_initialized` at `src/OPT4001.cpp:1431`.
- `readRegister16()` and `writeRegister16()` do not check `_initialized` at `src/OPT4001.cpp:1444` and `src/OPT4001.cpp:1457`.
- `end()` leaves cached callbacks in `_config` after marking `_initialized=false` (`src/OPT4001.cpp:232`).
- Failed `begin()` can store `_config` before probe/apply returns (`src/OPT4001.cpp:183`, `src/OPT4001.cpp:188`).

Impact:
- Public API can violate lifecycle expectations and access the bus from `UNINIT`.
- Field diagnostics and tests can observe `INVALID_CONFIG` or bus I/O instead of `NOT_INITIALIZED`.

Recommended remediation:
- Add initialized guards to public raw single-register APIs.
- If internal pre-init/probe/apply needs raw register access, split private helpers from public guarded methods.

Suggested tests:
- Before `begin()`, after failed `begin()`, and after `end()`, assert `readRegister16()` and `writeRegister16()` return `NOT_INITIALIZED` and do not touch fake bus counters.

### H3. Continuous/readiness state can over-report stale or premature samples

Severity: High

Evidence:
- `tick()` marks continuous samples ready by elapsed configured time only (`src/OPT4001.cpp:203`).
- `conversionReady()` returns true once `_conversionReady` is true in continuous mode, without hardware polling (`src/OPT4001.cpp:447`).
- `readSample()` caches a continuous sample but does not clear continuous readiness (`src/OPT4001.cpp:526`).
- Datasheet docs say `CONVERSION_READY_FLAG` is set at conversion end and clears on register `0x0C` read/write (`docs/OPT4001_datasheet.md:784`).
- Datasheet docs warn auto-range overflow can extend completion beyond nominal conversion time (`docs/OPT4001_datasheet.md:428`).

Impact:
- Production applications can treat repeated old register contents as fresh samples.
- Fast optical transients/auto-range overflow can be read prematurely.

Recommended remediation:
- Define the API contract: latest cached sample vs fresh conversion.
- For fresh reads, use `CONVERSION_READY_FLAG`, INT, or sample counter changes.
- Clear or advance readiness state after reads in continuous and one-shot paths.

Suggested tests:
- Fake bus with static result/counter in continuous mode; assert `tryReadSample()` does not repeatedly report fresh data.
- Fake bus where auto-range completion is delayed beyond nominal conversion time.
- Hardware test with counter/timestamp capture across continuous conversion times.

### H4. Probe and device-ID diagnostics are too weak for production startup

Severity: High

Evidence:
- `probe()` converts most transport failures to `DEVICE_NOT_FOUND` (`src/OPT4001.cpp:253`).
- `probe()` and `recover()` check only `(deviceId & MASK_DIDH) == DIDH_EXPECTED` (`src/OPT4001.cpp:262`, `src/OPT4001.cpp:284`).
- `decodeDeviceId()` knows DIDL and full match semantics (`src/OPT4001.cpp:1473`).
- Datasheet extracted ID register notes fixed upper bits and DIDL reset/read value (`docs/pdf-extracted-md/OPT4001_datasheet.md:1701`).

Impact:
- A field system cannot distinguish address NACK, bus timeout, arbitration, and generic absence during `begin()`.
- Incorrect devices or corrupted ID values with matching low 12 bits can be accepted.

Recommended remediation:
- Preserve transport status in `probe()` or add a diagnostic probe API that returns the raw transport code plus device-ID detail.
- Validate DIDL/reserved bits in presence detection, not only DIDH.

Suggested tests:
- Fake `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, and `I2C_BUS` during `begin()`/`probe()`.
- Device ID values `0x1121`, `0x2121`, `0x3121` should fail if fixed bits/DIDL are nonzero.

### H5. Production and validation claims exceed available evidence

Severity: High

Evidence:
- "Production-grade" appears in `README.md:3`, `library.json:4`, and `idf_component.yml:2`.
- README references validation/build commands but contains no captured hardware matrix (`README.md:356`).
- `docs/IDF_PORT_IMPLEMENTATION.md:3` lists implementation status, not measured validation status.
- No hardware commands, upload logs, logic analyzer captures, reference lux-meter data, or pure `idf.py` build logs were found.

Impact:
- Users can over-trust behavior that has not been validated under real optical, electrical, interrupt, FIFO, and bus-fault conditions.

Recommended remediation:
- Downgrade claims to "framework-neutral OPT4001 driver under validation" until P0/P1 fixes and hardware matrix are complete.
- Add a committed validation log/template with board, sensor package, firmware hash, commands, captures, and results.

Suggested tests:
- Execute the hardware validation matrix in this report on ESP32-S2 and ESP32-S3, Arduino and native IDF where applicable.

## Medium-Severity Findings

### M1. Threshold and raw lux conversion can overflow or invoke undefined behavior

Severity: Medium

Evidence:
- `rawToLux()` shifts caller-provided `mantissa << exponent` without bounds (`src/OPT4001.cpp:1522`).
- `_thresholdValid()` accepts exponent up to 15 (`src/OPT4001.cpp:1963`).
- `thresholdToAdcCodes()` returns `uint32_t` from `result << (8 + exponent)` (`src/OPT4001.cpp:1656`).

Impact:
- Public utility methods can return truncated lux values or hit undefined behavior for out-of-range caller inputs.
- Threshold comparisons can be wrong for register-valid high-exponent thresholds.

Recommended remediation:
- Validate sample exponent <= 8 and mantissa <= 20 bits for sample-style APIs.
- Use `uint64_t` for threshold ADC-code conversion or reject unsupported high exponents with clear status.

Suggested tests:
- Boundary tests for exponent 8, 9, 15, 31, 32 and max mantissa/result.
- Threshold reset and max-threshold conversion tests.

### M2. CRC correctness lacks independent validation

Severity: Medium

Evidence:
- Tests generate good CRCs by calling the driver's private helper (`test/test_basic.cpp:194`).
- The implementation computes CRC at `src/OPT4001.cpp:1968`.
- Datasheet equations are local but no independent oracle is used.

Impact:
- A copied or interpreted CRC formula bug can pass the suite.

Recommended remediation:
- Add independent CRC vectors from datasheet examples, TI support material, hardware captures, or a separate test-side implementation.

Suggested tests:
- At least five fixed vectors including all-zero, max mantissa/exponent, multiple counters, and corrupted single-bit cases.

### M3. Partial multi-register configuration failures can leave hardware/cache divergent

Severity: Medium

Evidence:
- `setThresholds()` writes low then high threshold and rolls back cache on second failure (`src/OPT4001.cpp:1091`).
- `_applyConfig()` writes four registers sequentially (`src/OPT4001.cpp:1853`).
- Successful later I2C operations reset health to READY (`src/OPT4001.cpp:1792`).

Impact:
- Software cache can say old settings while hardware has partially new settings.
- A field system may run with unexpected thresholds or mode until reset/recover.

Recommended remediation:
- Expose a dirty/config-sync state when partial apply fails.
- Provide `resyncFromDevice()` and/or readback verification after multi-register applies.

Suggested tests:
- Fail each write position in `_applyConfig()` and assert dirty state/readback behavior.

### M4. Burst CRC behavior is not per-sample despite API/docs implications

Severity: Medium

Evidence:
- `readBurst()` decodes later slots only while `status.ok()` (`src/OPT4001.cpp:587`).
- README says sample data may still be populated on `CRC_ERROR` (`README.md:168`) and advertises per-sample CRC.

Impact:
- A single CRC warning can hide later valid FIFO data.
- Caller cannot inspect all four `crcValid` states.

Recommended remediation:
- Decode all four slots regardless of individual CRC result.
- Return aggregate `CRC_ERROR` if any slot fails while preserving all decoded fields.

Suggested tests:
- Burst with CRC error in newest only, middle slot only, and last slot only.

### M5. ESP-IDF build and CI coverage are incomplete

Severity: Medium

Evidence:
- CI has PlatformIO Arduino/native jobs but no `idf.py` build (`.github/workflows/ci.yml:41`).
- `check_idf_example_contract.py` exists but is not in CI (`tools/check_idf_example_contract.py:113`).
- Local `idf.py --version` failed.

Impact:
- IDF support can regress while CI remains green.

Recommended remediation:
- Add IDF setup and build matrix for `esp32s2` and `esp32s3`.
- Add IDF contract checker to CI.

Suggested tests:
- `idf.py -C examples/esp_idf/basic set-target esp32s3 build`.
- `idf.py -C examples/esp_idf/basic set-target esp32s2 build`.

### M6. Public API headers lack key operational contracts

Severity: Medium

Evidence:
- README documents thread/ISR and CRC behavior (`README.md:154`, `README.md:168`).
- Public declarations around lifecycle and reads are sparse (`include/OPT4001/OPT4001.h:127`, `include/OPT4001/OPT4001.h:190`).

Impact:
- Generated API docs omit safety-critical usage constraints.

Recommended remediation:
- Add Doxygen comments for lifecycle, blocking, CRC, freshness, clear-on-read, threading, ISR, callbacks, and units.

Suggested tests:
- Doxygen build or grep-based doc contract check for required phrases on key APIs.

### M7. ESP-IDF CLI/docs overstate behavior and can stall periodic work

Severity: Medium

Evidence:
- README says IDF example runs the same interactive shell (`README.md:67`).
- IDF `diag`, `selftest`, and `watch` are shallower than Arduino equivalents (`examples/esp_idf/basic/main/main.cpp:383`, `:388`, `:396`).
- `cliLoop()` calls `getchar()` in the same loop as `device.tick()` (`examples/esp_idf/basic/main/main.cpp:430`).

Impact:
- IDF diagnostic behavior can be over-trusted and periodic driver work may stall on console input.

Recommended remediation:
- Correct docs to "command-token parity subset" unless behavior is actually equivalent.
- Use nonblocking console input or a separate task/timer for periodic sensor work.

Suggested tests:
- Behavioral CLI contract tests for command semantics, not just quoted tokens.
- Runtime IDF test with watch/tick active and no console input.

### M8. Fault-injection coverage is not broad enough

Severity: Medium

Evidence:
- FakeBus has global read/write statuses and one indexed write failure hook (`test/test_basic.cpp:23`).
- Tests do not inject address NACK, data NACK, bus arbitration, partial reads, or null timebase.

Impact:
- Production diagnostics and recovery behavior can fail for untested bus-fault modes.

Recommended remediation:
- Expand fake transport to model address, transfer length, partial write/read, per-transaction failures, and specific I2C errors.

Suggested tests:
- Matrix over `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS`, partial transfer, and recover paths.

## Low-Severity Findings

### L1. Local documentation has datasheet summary inconsistencies

Severity: Low

Evidence:
- `docs/OPT4001_datasheet.md:282` states FIFO0 is oldest, while later FIFO docs and the PDF extraction indicate FIFO0 is the newest shadow.
- `docs/AN_picostar_package.md:28` presents `-40 to 125 C`, conflicting with OPT4001 datasheet operating range `-40 to 85 C`.
- `docs/OPT4001_datasheet.md:701` lists PicoStar exponent 2 full-scale as `1310`, while implementation/range table use `1311`.

Impact:
- Future implementation work can be guided by inconsistent local summaries.

Recommended remediation:
- Correct summaries or mark app-note facts as not overriding the OPT4001 datasheet.

Suggested tests:
- Add docs consistency check for copied range table values.

### L2. `end()` hides power-down I/O failure

Severity: Low

Evidence:
- `end()` is `void` and discards `_i2cWriteRaw()` status (`src/OPT4001.cpp:232`).

Impact:
- Caller cannot know whether best-effort power-down reached hardware.

Recommended remediation:
- Document best-effort behavior or add a fallible `powerDown()` / `endWithStatus()` helper.

Suggested tests:
- Fake write failure during `end()`; assert documented state behavior.

### L3. Example transports are not production shared-bus examples

Severity: Low

Evidence:
- Arduino example initializes global `Wire`; IDF example owns its bus/device handles.
- No example mutex/shared-bus arbitration layer exists.

Impact:
- Integrators may copy examples into multi-device systems without adding locking.

Recommended remediation:
- Label examples as bring-up adapters and add a short shared-bus integration note.

Suggested tests:
- None required for core; documentation/example clarity is enough.

### L4. Release/security metadata has stale version information

Severity: Low

Evidence:
- `library.json:3` and `idf_component.yml:1` say `1.0.0`.
- `SECURITY.md:7` says supported version `0.3.x`.

Impact:
- Consumers receive inconsistent support/release metadata.

Recommended remediation:
- Update `SECURITY.md` during release metadata cleanup.

Suggested tests:
- Simple script comparing supported major/minor versions against `library.json`.

## Recommended Implementation Plan

### P0 - Must fix before production claim

1. Make `Version.h` available to clean manual and ESP-IDF builds: track a deterministic fallback or add CMake/generic generation.
2. Guard public `readRegister16()` and `writeRegister16()` with `_initialized`; split private internal register helpers as needed.
3. Redesign sample freshness/readiness semantics for continuous and one-shot paths; tie fresh reads to `CONVERSION_READY_FLAG`, INT, counter changes, or explicitly document "latest" vs "fresh".
4. Strengthen `probe()`/`recover()` ID validation and preserve transport diagnostic precision.
5. Downgrade production/validation claims until hardware and pure IDF validation evidence exists.

### P1 - Should fix before release/merge

1. Fix threshold/raw conversion overflow by validating inputs and/or using `uint64_t`.
2. Add independent lux, CRC, threshold, range, and conversion-time test vectors.
3. Add dirty/resync state for partial multi-register configuration failures.
4. Decode all burst FIFO slots even when one slot has CRC error.
5. Add full address matrix and transport-error fault-injection tests.
6. Add CI for `check_idf_example_contract.py` and pure ESP-IDF builds.
7. Move safety-critical API contracts into public Doxygen comments.
8. Fix IDF NACK mapping and IDF console/tick blocking behavior.

### P2 - Nice hardening / later

1. Delete or explicitly define copy/move semantics for `OPT4001`.
2. Add shared-bus integration example notes.
3. Correct docs inconsistencies and stale `SECURITY.md`.
4. Add Doxygen generation validation.
5. Add optional hardware validation log templates and captured example logs.

## Suggested Future Branch

```text
hardening/opt4001-industry-readiness
```

## Final Verdict

The repository is **not ready as-is for an industry-grade / production-ready claim**.

It has a solid framework-neutral architecture and good early test/build coverage, but it needs code hardening, CI expansion, documentation honesty fixes, and hardware/optical validation before production readiness can be claimed. The highest priority is to fix clean build reproducibility, lifecycle guard violations, sample freshness semantics, probe diagnostics, and validation evidence.
