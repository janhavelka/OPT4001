# OPT4001 audit verification and remediation

Reviewed on 2026-09-08. This is the completion report for the audit and its nine
follow-up items A-I, including the fixes already staged when work began.

## Scope and result

`git fetch --all --prune` confirmed that `main` already contained the newest
remote work, `origin/main` at `39a8bd0`. Local `main` was at `d983541`, one commit
ahead, with six staged audit-related files. Those changes were preserved and
reviewed before adding the corrections below.

The original findings remain available with
`git show d6e2eeb:AUDIT-FINDINGS.md`; their initial remediation was `73b66e4`.
Review covered that original list, every disposition in this ledger, the staged
changes, current implementations/callers, API contracts, and regression tests.
Driver, example, and independent audit reviews ran in parallel; changes then
received integration review and the validation listed below.

- **47 original finding IDs reviewed:** 44 addressed, including explicit
  compatibility/observability limits, and 3 proposed changes deliberately
  rejected with reasons (4d, 6, 9g). IDs 9a and 9b were missing from the old ledger.
- **A-I resolved:** six by code changes (A, B, C, E, G, I), three by explicit
  documentation of retained behavior (D, F, H).
- **Two additional readiness defects corrected:** poll counter proof retaining
  one-shot ownership, and cached readiness bypassing the FIFO-full burst gate.
- The old headline counted 49 findings although its table held 45 IDs. Its
  statement that 15 follow-up defects were fixed also overstated completion:
  six were listed as fixed and nine were still open. These counts are corrected
  here; test coverage limitations are recorded instead of inferred away.

## Follow-up items A-I

| ID | Verified behavior and action | Why this solution |
| --- | --- | --- |
| A | ESP-IDF `rebeginWithProfile()` restored the bus handle but left a previously bound-only driver at the rejected address. It now snapshots binding and initialization, restores with `begin(previous)`, `bind(previous)`, or `unbind()`, and preserves bound settings on successful changes. Failed handle rollback explicitly unbinds. | Uses the existing lifecycle operations and by-value configuration; no rollback service or hidden retry. An unavailable transport cannot leave a usable stale binding. |
| B | Arduino `rebeginWithConfig()` had the same initialized-only rollback. It now restores all three prior lifecycle states from the configuration copied before `end()`/`begin()`. | The existing helper owns profile replacement, so the fix stays local. Bound-only restoration performs no I2C. |
| C | One-shot watch polled every loop, while the continuous gate capped the requested sample cadence. Both paths now gate pending readiness retries with `lastPollMs`; `lastStepMs` continues to control cadence. Each new conversion can make its first readiness attempt immediately. | Reuses `waitingConversion` and existing timestamps. Ready continuous samples can still follow a 1 ms requested interval; pending retries remain 5 ms apart. |
| D | Pre-clock `_lastOkMs`/`_lastErrorMs` remain zero after the first caller timestamp. Both health accessors now say so, and the integration clock contract agrees. | Actual event times cannot be reconstructed. Rebasing would invent a recent success/failure and mislead age-based decisions. Supply time before tracked I/O when ages matter. |
| E | Invalid hardware exponents previously escaped as `INVALID_PARAM` with `verifyCrc=false`. They now return data-bearing `CRC_ERROR` in either setting, including matching-checksum malformed frames. Raw fields and actual checksum validity remain populated; ADC codes are zero and lux is NaN. Invalid exponents cannot veto readiness or advance its trusted counter. | Classifies a bad sensor frame as a data error while preserving caller-argument `INVALID_PARAM` in numeric helpers. Existing fresh-read error handling then consumes evidence consistently. |
| F | Synchronous readiness has no conversion/retry time gate before any clock is known. `conversionReady()` overloads and `Config::nowMs` now document repeated clear-on-read FLAGS access in that window. | A driver with no advancing clock cannot measure a retry interval. Keeping hardware-only inspection avoids making previously valid callers permanently not-ready; timed callers must provide/advance a clock. |
| G | Five duplicated CRC-aware counter vetoes now call one private duplicate predicate. Counter trust is expressed once and shared with baseline/consistency checks. | Ordinary valid and checksum-invalid frame behavior is preserved. E additionally excludes impossible exponents, which can now reach the data-bearing path. No evidence-source framework is needed. |
| H | All three full-scale accessors now explicitly promise rounded nominal datasheet values, with `rawToLux(exponent, 0xFFFFF)` identified for the decoded maximum. | Preserves existing numeric output and uses the existing conversion API for exact bounds instead of silently changing the meaning of full scale. |
| I | Continuous watch now prints `Counter gaps: n/a (continuous mode)`; one-shot watch retains the numeric gap result. | Distinguishes an unchecked diagnostic from an observed zero without changing the gap policy. |

The example changes are in `examples/01_basic_bringup_cli/main.cpp` and
`examples/esp_idf/basic/main/main.cpp`; core changes are in `src/OPT4001.cpp` and
its public headers. README and `docs/integration/driver-contracts.md` now match
these contracts. `CHANGELOG.md` records the work under Unreleased. No version
bump or release tag was requested; generated `Version.h` remains unchanged.

## Readiness fixes and inherited changes

The staged synchronous counter fix correctly called `_latchReadinessEvidence()`
instead of setting ready flags inline. The poll `READ_COUNTER` branch still used
the inline form: after a one-instruction counter poll, cancellation left a
completed one-shot owned and the next start returned `BUSY`. It now uses the
same latch. A regression cancels between counter proof and frame transfer and
checks that another one-shot can start immediately.

The staged removal of the poll INT shortcut correctly protected the four-period
`FIFO_FULL` burst gate. However, `tick()` or explicit `readFlags()` could already
have cached readiness after one period. `WAIT_READY` accepted that cached state
before checking the burst gate. The FIFO gate now also applies to cached
evidence, retaining the evidence until the full window elapses. Dedicated tests
cover both cached evidence and an asserted INT with an established baseline.
Elapsed time remains a gate, never proof that hardware completed a conversion.

The other staged changes were retained: pre-clock sample timestamps no longer
move forward on the first caller timestamp; the INT-hint and CRC-counter tests
exercise formerly unreachable branches; transaction-address assertions are
bounded by the fake recorder capacity. Sample and health ages recorded before
a clock exists are unknown and subject to unsigned wrap, so this report does
not repeat the old blanket claim that those ages always over-report safely.

## Full original finding review

The table identifies the relevant source owners and functions. Test results and
mutation evidence are reported separately below; source review alone is not
hardware validation. Source labels refer to [Core](../src/OPT4001.cpp),
[API](../include/OPT4001/OPT4001.h), [Arduino](../examples/01_basic_bringup_cli/main.cpp),
[IDF](../examples/esp_idf/basic/main/main.cpp), [IDF transport](../examples/esp_idf/basic/main/Opt4001IdfI2cTransport.cpp),
and [Native tests](../test/test_basic.cpp). A finding can be addressed by a documented boundary where
its proposed behavioral change would introduce a worse contract.

| ID | Verification and simplest proper disposition | Source and regression evidence |
| --- | --- | --- |
| 1 | Valid loop-count/deadline conflation; elapsed timeout plus a separate unchanged-clock guard is appropriate. Keeping 1,000,000 rather than proposed 4096 avoids another aggressively low CPU-count assumption; the documented progress precondition remains essential. Precise ESP32 failure latency was an estimate, not measurement. | Core `readBlocking()`, `readFreshBlocking()`, stalled-clock guard. |
| 2 | Valid conflicting clocks; one selected source is implemented. Hook time overrides tick/poll arguments; otherwise caller observations feed `_hostMs`. Keeping explicit timestamp parameters for poll steps is harmless, so deleting every overload is not necessary for correctness. Conversion gate rebases; old sample/health timestamps must not be made artificially recent. | Core `tick()`, `poll()`, `_observeHostMs()`; API clock contracts. |
| 3a | Valid destructive early FLAGS polling; timed mode now gates both first probe and retry. The separately documented no-clock hardware-only mode is a compatibility boundary, not equivalent to timed throttling (see F disposition). | Core `_refreshReadinessEvidence()`, conversion/retry gates. |
| 3b | Valid poll ordering problem; once baseline exists, poll uses counter and never stale-counter fallback to FLAGS. This preserves threshold latches with no new evidence-source framework. | Core `_pollReadJob()` counter/FLAGS selection. |
| 3c | Valid ping-pong/unbounded readiness I/O per poll; separate unsigned-elapsed retry stamp returns to WAIT_READY without moving conversion start. Storing elapsed origin is simpler and avoids signed-deadline half-wrap behavior. | Core `_readinessRetryElapsed()`, `_armReadinessRetry()`. |
| 3d | Valid indefinitely owned idle/read job; inactive power-down start is rejected, first poll captures deadline, timeout releases job and one-shot ownership. This remains bounded in caller time and per-poll instruction budget. | Core `startReadSample()`, `startReadBurst()`, `_pollReadJob()`, `startConversion()`. |
| 4a | Valid INT pulse-as-proof error; demoting level to optional counter hint is the smaller supported solution, avoiding a speculative ISR API. Poll has no INT timing shortcut. This pass also fixes cached readiness bypassing the FIFO burst gate. | Core `_intFreshEvidenceAsserted()`, `_pollReadJob()`; vendor Table 8-2. |
| 4b | Valid discarded clear-on-read evidence; all register-read paths converge on set-only capture; complete/partial FLAGS word handling is explicit. Non-burst 1-byte public read performs a full 2-byte hardware read and captures that full evidence correctly. | Core register reads, `_captureReadinessFromFlags()`. |
| 4c | Valid corrupted counter poisoning; checksum-invalid newest frames neither veto readiness nor advance baseline, including reporting-disabled mode. New G shared trust/duplicate predicates remove five drifting copies. Trusted counters now require both a valid checksum and valid exponent. Valid newest counter stays usable when only historical FIFO CRC fails. | Core `_sampleCounterIsTrusted()`, `_sampleCounterIsDuplicate()`, `_markFreshSampleConsumedAt()`. |
| 4d | Counter aliasing is real, but proposed flag-authority bypass is unsafe: stale FLAGS can describe a prior sample accepted via counter. Retain conservative equal-counter rejection and document modulo-16 limitation; `readLatestSample` explicitly opts out of proof. | Core `_sampleCounterIsFresh()`; API `readSample()` contract. |
| 5a | Valid raw-write framing divergence; observed burst bit plus conservative invalidation is proper within single-owner/readback boundary. Reject claim that reset disables burst: vendor reset is 0x8011, bit 0=1. A cached bit cannot discover an unobserved external writer; automatic universal detection was never implemented or promised. | Core register writes/readbacks and `_readBurstBlockTracked()`; vendor PDF page 31. |
| 5b | Valid non-atomic multi-transaction history; one final counter comparison catches detected FIFO movement without a retry framework. It does not prove atomicity across counter wrap, torn newest pairs, or CRC collisions. Existing header correctly discloses these limits. | Core `readBurst()`; API burst consistency contract. |
| 6 | Rejection upheld. Counting all callback errors would count adapter precondition/host configuration failures, contrary to repository rules and 9h. Keep reserved INVALID_PARAM/INVALID_CONFIG health-neutral, require I2C_* for bus faults, return explicit error to caller. | Core tracked transport wrappers; `Config.h` callback contract; IDF transport status mapping. |
| 7a | Shared latch extraction is proper, but inherited fix missed poll READ_COUNTER: cancellation/failure after counter-only polling retained one-shot ownership. Both counter-proof paths now invoke `_latchReadinessEvidence`, with a poll/cancel regression. | Core `_latchReadinessEvidence()`, both counter proof paths. |
| 7b | Valid decode status/field population problem; deterministic outputs before data error are correct. E extends handling to reporting-disabled and checksum-valid impossible exponents. Keep checksum validity factual while classifying invalid sensor fields as a frame error; valid checksums on impossible exponents do not make their counters trusted. | Core `_decodeSampleRegisters()` and burst aggregation. |
| 7c | Valid dirty-state loss; bind is bus-silent and cannot repair prior divergence, so removing clear is correct. Unbind remains explicit diagnostic reset; successful reapply clears dirty. | Core `bind()`, dirty-state diagnostics. |
| 7d | Valid documentation gap; current API intentionally tears down old binding even on validation failure. Clarifying this and copying aliased getConfig input is safer than a silent lifecycle compatibility change. | API `bind()`/`begin()` contracts and implementation. |
| 7e | Valid missing precondition documentation; keep initialized requirement because cached polarity before hardware apply is not authoritative. No reason to relax to bound-only. | API/core `readIntPinAsserted()`. |
| 7f | Valid burst-cache documentation mismatch; synchronous burst fills cache and starting sample-only poll invalidates it. Document ownership instead of changing cache semantics. | API `getLastBurst()`; core `readBurst()` and `startReadSample()`. |
| 7g | Valid resident scratch waste; local BurstFrame in single READ_BURST_BLOCK step is correct, no retained pointer/lifetime need. | Core `_pollReadJob()` local `BurstFrame`; API private members. |
| 7h | Zero invalid-value sentinel is a legitimate documented contract; configuration validation prevents it reaching internal timing gates. Clamping to 800ms would silently make invalid input appear valid. | Core `getConversionTimeUs()`, `getConversionTimeMs()`; API invalid-enum contracts. |
| 7i | Nominal vs exact maximum distinction is real; retaining established nominal values with explicit contracts on all three helpers is the smaller compatible solution. Range-8 example in original finding had wrong direction: 117441 exceeds exact SOT maximum. Use existing rawToLux(exponent,0xFFFFF) for exact maximum. | Core full-scale helpers; API full-scale contracts. |
| 7j | Valid redundant short-length exception; removed. Existing bounded span validation remains clear; introducing another validating accessor is optional and unnecessary. | Core `readRegisters()` span/framing validation. |
| 7k | Concrete original threshold divergence scenario was invalid, but lifecycle asymmetry is real for partially applied attach/reset. Reusing cancelPollJob in end is simplest. | Core `end()` and `cancelPollJob()`. |
| 8 | Valid coverage deficiency; source now covers changed clocks, statuses, flags, framing and address paths. Test fake should record requested transfer framing instead of inventing undefined hardware byte patterns. Tests still prove software behavior only. | Native `FakeBus` and regression suite; CLI/parser/checker behavior tests. |
| 9a | Valid destructive failed capture; current selfcheck aborts before any mutating diagnostic when raw or lux threshold capture fails. This is safer and simpler than guarding only restore writes. | Arduino `runSelfTest()` threshold capture and early return. |
| 9b | Valid misleading CLI failure; existing raw-config handler accepts OK or IN_PROGRESS and reads back. | Arduino `config write` dispatch. |
| 9c | Valid zero-tick spin risk; direct vTaskDelay(1) is simpler than new rounding helper for two callers. sdkconfig default 1000Hz restores millisecond cadence; delay still blocks at other tick rates. | IDF wait helpers; `examples/esp_idf/basic/sdkconfig.defaults`. |
| 9d | Valid scheduler-yield limitation; adapter/example callback vTaskDelay(1) gives lower-priority tasks a turn, README matches. Core stays framework-neutral. | Arduino yield hook, IDF transport yield hook; README example. |
| 9e | Valid missing GPIO initialization; configure input/pull-up once for enabled pin, check gpio_config result. External pull-up remains application hardware obligation. | IDF `app_main()` GPIO initialization. |
| 9f | Valid directory-name restriction; documented in README and IDF guide. Retaining existing component registration avoids duplicate source-owner CMake plumbing for an example. | IDF CMake files; README and ESP-IDF integration guide. |
| 9g | Deliberate target support limit upheld. Manifest S2/S3 matches declared targets and CI. Documentation branch is one of original finding's permitted solutions; The README now also states the restriction. | `idf_component.yml`; README and ESP-IDF integration guide. |
| 9h | Valid host fault misclassification; INVALID_CONFIG for unconfigured transport context follows health-neutral integration contract. Returned failure remains explicit even while health is READY. | IDF transport callback preconditions and error mapping. |
| 9i | Valid loss of prior profile; initialized-only prior fix incomplete. Current Arduino and IDF changes capture both initialized/bound state and restore begin/bind/unbind accordingly, preserving value-copied configuration and stopping sessions. IDF also reports bus rollback failure explicitly. | Arduino `rebeginWithConfig()`, IDF `rebeginWithProfile()`; CLI workflow regressions. |
| 9j | Valid throttling issue; previous continuous-only gate was partial and clamped ready sample cadence. Current watch uses lastPollMs for pending retries in both paths; first ready sample attempts retain requested cadence. | Arduino `handleWatch()`; CLI workflow cadence/retry regressions. |
| 9k | Valid false counter-gap diagnostics in continuous mode; gate gap detection to host-paced one-shots. Current I also prints n/a for unchecked continuous gaps, avoiding false zero assurance. | Arduino `recordWatchSample()`, `finishWatch()`. |
| 9l | Valid status styling inconsistency; one statusColor helper classifies OK, progress/readiness/CRC warning and real errors. | Arduino `statusColor()`, `printStatus()`. |
| 9m | Valid grammar duplication; both measure handlers use one bool tokenizing parser. No driver Status or shared framework parser required for CLI usage errors. | Both CLIs share local measurement argument parsers. |
| 9n | Valid excessive blocking sequence; restrict 1..20 and document cancellable watch N 0. Bounded diagnostic operation may still block for up to timeout per attempt; no claim of immediate cancellation. | Arduino help/read dispatch; IDF read dispatch. |
| 9o | Valid misleading help; bare watch is inspector and counted watch is session start, now separate help entries. | Arduino `printHelp()`. |
| 9p | Valid broken serial indirection; deleting unused indirection keeps all example I/O on Serial. | `examples/common/Log.h`, `CliShell.h`; obsolete names absent. |
| 9q | Valid unused/repeated helpers; dead recovery/log init removed, CLI callers use shared cli color helpers. | `examples/common/CliStyle.h`, `I2cScanner.h`; Arduino callers. |
| 10a | Valid prose false positives; uppercase status tokens preserve health fields lacking Status prefix and avoid lower-case prose. Anchoring every match to Status would miss state=OFFLINE, so adopted narrow change is better. | `tools/hil_opt4001_runner.py`; parser regressions. |
| 10b | Valid help-only parity loophole; dispatcher strcmp requirement plus mutation test closes reported source-contract gap. This remains a source checker, not a C++ semantic verifier. | `tools/check_idf_example_contract.py`, `tools/test_idf_example_contract.py`. |
| 10c | Valid test gap, no routing bug; recorded device addresses and all three general-call reset paths assert address 0x00/payload 0x06. Recorder bound checked before assertion loops. | Native `FakeBus` address recorder and general-call reset regressions. |
| 10d | Valid fake bug; zero FLAGS write preserves register, nonzero write clears conversion-ready per hardware specification. No invented mask behavior required. | Native `fakeWrite()` FLAGS behavior and regression. |
| 10e | Valid missing API coverage; one-shot raw configuration, invalid fields, raw FLAGS side effects, blocking lux CRC, online/latch and overload now explicitly exercised. | Native raw configuration/FLAGS, lux, online/latch and overload regressions. |

## Deliberately retained behavior

- **4d:** Equal trusted counters remain duplicates even after explicit FLAGS
  readiness. A stale flag can refer to a sample already accepted through counter
  evidence. Full modulo-16 aliasing remains unavoidable; `readLatestSample()` is
  the explicit path when freshness proof is not required.
- **6:** Adapter `INVALID_PARAM`/`INVALID_CONFIG` remain health-neutral integration
  errors. Counting every callback failure would conflict with the repository
  rule excluding validation/precondition failures. Adapters must report actual
  bus faults as `I2C_*`; every API still returns its explicit failure.
- **9g:** The ESP-IDF component remains restricted to the supported S2/S3 targets.
  Both the integration guide and README now expose that restriction. Removing
  the pin would imply unsupported target coverage.
- **5a limitation:** Observed configuration supports correct framing within the
  documented single-owner/readback contract; no cache can detect an unobserved
  external register writer. The original example claiming reset disables burst
  was incorrect: the authoritative vendor PDF, printed page 31, section
  8.6.1.12/Figure 8-34, specifies INT_CONFIGURATION reset `0x8011`, burst bit 0
  equal to 1. Reset enables burst.
- **5b limitation:** A final counter reread detects some non-burst FIFO movement;
  it does not establish atomicity across a full counter wrap, torn register pair,
  or CRC collision. Those limits remain explicit in the API contract.

The original refuted-claim appendix was also checked. Rejections still hold for
the alleged PicoStar CLI setter path, missing overall HIL timeout failure,
invented burst-disabled fake bytes, alleged checker regex syntax defect,
unchanged-threshold divergence scenario, speculative counter-overload mismatch,
removal of poll switch/return guards, and hidden dirty-state reason. The two
previously overturned rejections remain correctly fixed: unsigned parsers
reject leading negatives/overflow, and fake one-shot timing sums microseconds
before rounding once. The original range-8 full-scale example also misstated the
rounding direction; nominal SOT-5X3 117441 lux is above the decoded maximum.

## Still open

No unaddressed code finding remains from A-I. These rows retain the deliberately
unchanged behavioral alternatives and their reasons instead of silently deleting
them:

| ID | Retained behavior / reason |
| --- | --- |
| D | Do not rebase health event timestamps: actual pre-clock event times are unknown. The public accessor contracts now document this. |
| F | Do not invent a clockless polling interval: hardware inspection remains available, and timing gates require a caller-provided advancing clock. |
| H | Do not change nominal full-scale output: all delegating accessors now warn that it is not an exact saturation bound; existing raw conversion supplies that bound. |

Hardware validation and complete embedded compilation remain separate validation
obligations, as described below.

## Validation

All following results were obtained from the final fixed working tree. The
native baseline was 165 tests with the pre-existing staged changes; this pass
adds seven and expands existing CRC-path coverage.

| Check / command | Result |
| --- | --- |
| `.\scripts\pio.cmd test -e native` | **172/172 passed**, including final rerun after mutation restoration. |
| `.\scripts\pio.cmd run -e native_core_no_arduino` | **Passed**, `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`. |
| `python tools/check_core_timing_guard.py` | Passed. |
| `python tools/check_cli_contract.py` | Passed. |
| `python tools/check_idf_example_contract.py` | Passed. |
| `python tools/check_ci_action_pins.py` | Passed. |
| `python tools/check_version_header_contract.py` | Passed. |
| `python tools/check_clean_consumer_package.py` | Passed: packaged library compiles in a clean native consumer. |
| `python scripts/generate_version.py check` | Passed; generated files unchanged. |
| `python tools/test_cli_integer_parsers.py` | 1/1 passed. |
| `python tools/test_hil_opt4001_runner_parser.py` | 17/17 passed. |
| `python tools/test_idf_example_contract.py` | 1/1 passed. |
| `python tools/test_cli_workflows.py` | 8/8 passed, covering 24 rollback failure cases, six successful profile changes, watch cadence/retries/wrap/timeout and summary output. |
| `python -m unittest discover -s tools -p 'test_*.py'` | **27/27 passed**, including the new CLI workflow tests. Existing CI discovers them automatically. |
| `python tools/hil_opt4001_runner.py --parser-self-test` | Passed. |
| `git diff --check` and staged whitespace check | Passed. |
| `.\scripts\pio.cmd run -e esp32s3dev` | Blocked locally: `xtensa-esp32s3-elf-g++` is not recognized. |
| `.\scripts\pio.cmd run -e esp32s2dev` | Blocked locally: `xtensa-esp32s2-elf-g++` is not recognized. |
| Native ESP-IDF S2/S3 builds | Unavailable locally: `idf.py` and `IDF_PATH` are absent. |

### Mutation evidence

**22 independent production mutations were killed by runtime assertions: 14
core and eight example mutations.** Each test ran first against the fixed code,
then with only its relevant production change reverted in the actual source,
then with the fixed bytes restored. No compile failure was counted as a killed
mutation; all temporary changes were restored. The final suites pass.

| Reintroduced defect | Regression that failed |
| --- | --- |
| Mandatory malformed-exponent error mapping removed | `test_corrupt_exponents_always_report_frame_error_with_raw_fields`, `test_corrupt_exponent_fresh_reads_consume_readiness`, `test_corrupt_fifo_exponent_keeps_valid_newest_sample`, plus both updated inherited tests. |
| Matching checksum allowed an invalid exponent into counter trust | `test_corrupt_exponent_fresh_reads_consume_readiness`, `test_decode_invalid_result_exponent_caches_frame_error`. |
| Non-burst movement guard trusted an invalid exponent | `test_corrupt_exponent_fresh_reads_consume_readiness`. |
| Trust requirement removed separately at each of five duplicate-veto sites: sample, slot 0, burst, non-burst, poll | `test_crc_invalid_duplicate_counter_is_not_vetoed` fails for each independent mutation; it now exercises all six public paths and both CRC policies. |
| CRC requirement removed from baseline trust | `test_crc_invalid_counter_never_advances_freshness_baseline`. |
| Poll counter readiness restored to inline flags | `test_poll_counter_readiness_releases_one_shot_before_frame_read`. |
| Synchronous counter readiness restored to inline flags | `test_counter_proven_readiness_releases_one_shot_ownership`. |
| Cached-readiness FIFO gate removed | `test_poll_fifo_full_cached_readiness_waits_for_history_gate`. |
| Poll INT shortcut restored | `test_poll_fifo_full_int_hint_waits_for_history_gate`. |
| Cached sample timestamp rebase restored | `test_first_caller_clock_preserves_unknown_sample_and_health_stamps`. |
| Old IDF rollback restored | `test_idf_rollback_preserves_lifecycle_config_and_transport`: eight failed subcases. |
| Old Arduino rollback restored | `test_arduino_rollback_preserves_lifecycle_and_config`: five failed subcases. |
| One-shot readiness retry gate removed | `test_oneshot_readiness_retries`. |
| Unconditional continuous cadence clamp restored | `test_continuous_cadence`. |
| New-conversion first-poll timestamp reset removed | `test_oneshot_readiness_retries`. |
| Old numeric continuous gap summary restored | `test_watch_summary`. |
| IDF bound-only configuration discarded on successful change | `test_profile_success_preserves_bound_settings`. |
| IDF binding retained after handle-rollback failure | `test_idf_rollback_preserves_lifecycle_config_and_transport`: three failed subcases. |

The non-burst guard mutation initially survived because the fake changed its
registers after the guard read. The test was corrected to change the window
between the eighth slot read and the ninth guard read; the mutation then failed
at a runtime assertion. This report claims the corrected coverage only.

Two inherited expectations intentionally changed; neither was removed merely
to obtain a passing suite:

- `test_crc_error_precedes_corrupt_exponent_with_populated_output` expected
  `INVALID_PARAM` when CRC reporting was disabled. That assertion encoded E's
  defect and now expects `CRC_ERROR`; populated-field checks remain.
- `test_decode_rejects_invalid_result_exponent_without_cache_update` was renamed
  `test_decode_invalid_result_exponent_caches_frame_error`. Its old no-cache-update
  expectation conflicted with the required data-bearing frame-error contract.
  It now checks cached raw fields, zero ADC codes, NaN lux, consumed readiness,
  and an unchanged trusted baseline.

The new malformed-frame cases cover exponents 9..15, both CRC policies, matching
and mismatching checksums, duplicate/changed counters, synchronous sample/slot/
burst/non-burst and poll sample/burst paths, FIFO-slot population, freshness
recovery, health neutrality, and non-burst guard movement.

### Validation limits

The host workflow harness compiles verbatim production functions under host
`g++ -Wall -Wextra -Werror`. Rollback functions link the actual driver with a fake
transport; watch functions use controlled driver responses and captured serial
output. It does **not** compile either complete embedded application or adapter.
The local build failures above are current observations; the attachment's older
`Unknown repository type` error was not the error produced in this run.

The existing VS Code-managed PlatformIO Core 6.1.19 was used through the wrapper.
No other Core was installed and no toolchain repair was attempted. The inherited
`PLATFORMIO_CORE_DIR=C:\pio` still lacks usable selected S2/S3 compiler commands.
Complete Arduino and native ESP-IDF compilation requires the repository's CI
matrix for the committed source revision.

Native tests prove source behavior using fake transport. No board was flashed,
and no optical, INT pulse, address-pin, real FIFO, or hardware recovery validation
is claimed. Timing gates and counter checks retain the documented hardware and
modulo-counter observability limits.
