# OPT4001 audit verification and remediation

Review date: 2026-09-05. Baseline: `main` at
`d6e2eeb9e7df361bd4b4880d912a653a897d831a`, fetched and fast-forward checked
against `origin/main`; the initial working tree was clean.

The requested `docs/CODE_AUDIT.md` was absent from the checkout and fetched
history. The input reviewed was the repository's
[AUDIT-FINDINGS.md at the baseline commit](https://github.com/janhavelka/OPT4001/blob/d6e2eeb9e7df361bd4b4880d912a653a897d831a/AUDIT-FINDINGS.md),
dated 2026-08-29. The IDs below retain its numbering, including the items it
called already fixed and the claims it rejected.

Findings were checked against implementation and callers, public contracts,
existing tests, and the checked-in vendor PDFs. The baseline passed all 133
native tests and the strict framework-neutral build. Passing those checks did
not disprove the reported behavioral defects.

## Core findings

Implementation lives in [OPT4001.cpp](../src/OPT4001.cpp), with contracts in
[OPT4001.h](../include/OPT4001/OPT4001.h), [Config.h](../include/OPT4001/Config.h)
and [driver-contracts.md](integration/driver-contracts.md). Regression coverage
is in [test_basic.cpp](../test/test_basic.cpp).

| ID | Verification and disposition |
| --- | --- |
| 1 | **Valid defect; proposal strengthened.** The old cap counted CPU spins as though they represented elapsed milliseconds. Blocking reads now use their elapsed-time timeout, with a separate consecutive unchanged-clock guard. Clock progress resets that guard. A non-progressing hook reports `INVALID_CONFIG`. The proposed 4096-iteration guard is itself too aggressive for fast CPUs; the implementation uses 1,000,000 and documents its precondition rather than claiming that an iteration count is a portable wall clock. The audit's precise ESP32 failure latency was an estimate, not captured hardware evidence. |
| 2 | **Valid; fixed.** `Config::nowMs` is authoritative when supplied. Otherwise `tick()`/`poll()` feed the stored caller clock, and the first observation rebases startup timestamps. Queries do not mutate time. Clock state is reset at binding boundaries rather than inside configuration completion. This removes comparisons between unrelated hook/caller epochs and the permanent zero timestamp fallback. |
| 3a | **Valid; fixed.** Synchronous readiness uses the conversion gate and a separate retry interval before clear-on-read FLAGS polling. Repeated calls in one conversion window no longer consume threshold flags on every loop iteration. |
| 3b | **Valid; fixed.** Poll reads use counter evidence first once a trusted baseline exists. A stale counter does not route back to destructive FLAGS reads. INT alone never supplies readiness. |
| 3c | **Valid; fixed.** Failed readiness probes return to a retry gate. Its cadence is `max(1, conversionTimeMs / 16)`; it does not modify the conversion's original start time or postpone its timeout. |
| 3d | **Valid; fixed.** A read job in inactive power-down is rejected. Active read jobs have a finite conversion-derived deadline, initialized on their first poll, and a lost one-shot completion no longer holds the conversion BUSY forever. Abandonment releases software ownership; it does not assert that hardware was cancelled. |
| 4a | **Valid; fixed without adding an ISR API.** A GPIO level is a hint to inspect hardware evidence, never sufficient evidence itself. This removes reset-result false positives on an asserted/shared INT line. The approximately 1 us conversion/FIFO pulse cannot be captured reliably by task-level level polling. |
| 4b | **Valid; fixed.** Typed FLAGS, raw FLAGS, raw register, and register-block reads share set-only readiness capture. A later cleared FLAGS read cannot erase captured evidence. Block reads inspect the FLAGS word only when both bytes were returned. |
| 4c | **Valid; fixed.** CRC-invalid output is data-bearing but does not move the trusted counter baseline or undergo a trusted-counter veto. This also applies when `verifyCrc` suppresses the warning status. A subsequent valid read may deliver the conversion again. A bad historical FIFO slot does not invalidate a valid newest sample's counter. |
| 4d | **Real information limit; proposed bypass rejected.** A modulo-16 counter aliases after a full wrap. However, FLAGS can still contain the ready bit from a sample already accepted through the counter path. Treating that bit as unconditional authority would return duplicates, including after an explicit `readFlags()`. The implementation retains conservative equal-counter rejection, preserves threshold flags during counter polling, and documents the full-wrap limit. `readLatestSample()` remains the explicit way to read data without freshness proof. |
| 5a | **Partly valid; fixed at the actual ownership boundary.** Raw `0x0B` writes could desynchronise framing from the cached configuration. Framing now follows the observed hardware burst setting from successful writes/readbacks and becomes conservative after uncertain writes. The audit's reset example is wrong: the documented reset value is `0x8011`, with burst enabled. A second cached bit cannot detect arbitrary out-of-band changes; applications must serialize writers and read back or recover after external changes. No speculative readback transaction was added to every sample. |
| 5b | **Valid; bounded detection added, atomicity claim rejected.** Non-burst FIFO assembly spans separate transactions. A final newest-counter read rejects detected FIFO movement with `MEASUREMENT_NOT_READY`. This is a consistency check, not proof of an atomic snapshot: a complete counter wrap, a torn pair, or CRC collision remains possible. CRC verification and appropriately fast reads are still necessary. |
| 6 | **Contract/mapping issue; proposed rewrite rejected.** Repository rules explicitly exclude validation and precondition errors from health tracking, including adapter configuration faults. Counting every callback error would violate those rules and contradict item 9h. Reserved validation statuses remain health-neutral; genuine bus faults must use `I2C_*`. The callback contract and tests make this distinction explicit. No parallel preflight layer was added. |
| 7a | **Valid; fixed.** Readiness capture is shared and clears one-shot lifecycle state only outside continuous mode. |
| 7b | **Valid; fixed.** Sample fields are populated deterministically before returning a decode error. CRC failure takes precedence over a corrupted exponent's numeric error when verification is enabled. The unreachable ADC overflow branch is removed. |
| 7c | **Valid; fixed.** A bus-silent `bind()` cannot establish that hardware matches the cache, so it preserves existing dirty evidence. `unbind()` remains an explicit release boundary. |
| 7d | **Valid documentation gap; clarified.** Invalid `bind()`/`begin()` configuration tears down runtime state and clears the cached configuration. Retrying from a cached configuration is possible only after a valid configuration was retained following a probe/apply failure. No incompatible lifecycle change was introduced. |
| 7e | **Valid documentation gap; clarified.** `readIntPinAsserted()` requires initialization because its cached polarity must first have been applied to hardware. Relaxing the precondition would permit inverted interpretation. |
| 7f | **Valid documentation gap; clarified.** Both synchronous and poll burst reads populate the burst cache; starting a sample-only job invalidates it. Existing cache ownership is retained. |
| 7g | **Valid simplification; fixed.** The poll-step burst scratch object is local rather than permanently resident in every driver instance. |
| 7h | **Existing zero-return guard retained.** Invalid conversion enums are rejected by configuration APIs. The defensive timing helpers keep their documented zero result instead of silently presenting an invalid value as a valid 800 ms setting. Callers must not use the invalid sentinel as a duration. |
| 7i | **Partly valid; nominal-value contract clarified.** Full-scale helpers expose rounded datasheet values, not exact saturation bounds. For example, PicoStar range 1 can encode approximately 655.359 lux while its nominal value is 655. The audit's chosen range-8 example is wrong: 117441 exceeds the approximately 117440.4 lux exact maximum. Existing nominal values remain compatible. |
| 7j | **Valid simplification; fixed.** The unnecessary short-length framing exception is removed; the existing bounded register-span validator is retained. |
| 7k | **Valid lifecycle asymmetry; fixed.** `end()` goes through cancellation when a poll job is active, preserving the same partial-configuration dirty rules as explicit cancellation. The audit's original configure-measurement example was not evidence of differing threshold writes. |
| 8 | **Valid coverage gaps; expanded.** Native regressions cover slow/progressing and stalled clocks, caller-time operation, raw-write framing, address/payload routing, clear-on-write FLAGS behavior, readiness throttling, CRC neutrality and relevant public APIs. The fake does not invent undocumented burst-disabled byte patterns. The requested silent external-change test is scoped to observing that change first; no cache can detect an unobserved writer. |

The local [SBOS993A PDF](reference/OPT4001_datasheet.pdf), Table 8-2 and sections
8.5.2.2 / 8.6.1.12, supports the INT and burst distinctions above. TI's
[published datasheet](https://www.ti.com/lit/ds/symlink/opt4001.pdf) was also checked.

## Examples and integration

Both [Arduino](../examples/01_basic_bringup_cli/main.cpp) and
[native ESP-IDF](../examples/esp_idf/basic/main/main.cpp) remain diagnostic
examples. No framework dependency was added to the driver core.

| ID | Verification and disposition |
| --- | --- |
| 9a | **Prior fix incomplete; completed.** Skipping restoration after failed threshold capture was already present, but self-check still changed thresholds later. It now stops the destructive portion if either raw or lux threshold capture fails. Failed capture cannot lead to default/zero threshold writes. |
| 9b | **Already fixed; verified.** Arduino raw configuration writes accept the documented one-shot `IN_PROGRESS` result and perform readback. Status rendering is also corrected by 9l. |
| 9c | **Valid; fixed more simply.** The two IDF short waits use `vTaskDelay(1)` directly, avoiding a generic rounding helper with no other caller. `sdkconfig.defaults` requests 1000 Hz to match the example's millisecond polling cadence; a one-tick delay still blocks at another tick rate. |
| 9d | **Valid; fixed.** Arduino and IDF blocking-helper hooks use `vTaskDelay(1)` so lower-priority tasks can run. The README example follows the same pattern. A plain scheduler yield is not an idle-task scheduling guarantee. |
| 9e | **Valid; fixed.** The native IDF example configures an enabled INT pin as an input with pull-up and checks configuration failure. The application's external pull-up requirement remains documented. |
| 9f | **Valid integration restriction; documented.** The checkout must be named `OPT4001` for `REQUIRES OPT4001`. Existing component ownership is retained rather than introducing a duplicate component registration just for the example. |
| 9g | **Intentional support boundary; documented.** The ESP-IDF manifest remains restricted to ESP32-S2/S3, the repository's declared and CI-built targets. Removing the restriction without validating more targets would overstate support. |
| 9h | **Valid; fixed consistently with 6.** An unconfigured IDF transport context returns `INVALID_CONFIG`. It is an explicit integration failure, not device/bus failure evidence. |
| 9i | **Valid; fixed.** Profile changes save the old configuration by value, stop watch/stress sessions, and attempt rollback if reinitialization fails. The original failure and any rollback failure are reported. The corresponding IDF profile path receives the same protection. |
| 9j | **Valid; fixed.** Arduino watch has a separate 5 ms readiness-poll gate. Sample cadence still uses its original interval timestamp; a not-ready poll does not extend the requested sample interval. |
| 9k | **Valid; fixed.** Counter gaps are diagnosed only for host-paced one-shots. Continuous sampling may legitimately skip conversions, and modulo-16 arithmetic cannot reconstruct the missing count. |
| 9l | **Valid; fixed.** Arduino status styling treats `IN_PROGRESS`, `CRC_ERROR`, and `MEASUREMENT_NOT_READY` as informational/warning states rather than rendering every non-OK result as an error. |
| 9m | **Valid; fixed.** `measure` and `job measure` share a tokenizer-based parser within each framework. Extra/malformed tokens are rejected and optional quick-wake semantics agree. No driver `Status` is manufactured for CLI usage errors. |
| 9n | **Valid; fixed.** Blocking `read N` is restricted to 1–20 in both CLIs with matching usage text. Arduino's cancellable back-to-back equivalent is `watch N 0`. This remains a bounded diagnostic command, not a promise of immediate cancellation. |
| 9o | **Valid; fixed.** Arduino help separates bare `watch` state inspection from `watch N [interval]` session startup. |
| 9p | **Valid; removed.** Broken `LOG_SERIAL` indirection and unused `log_begin()` are deleted. Example reads, writes and serial initialization consistently use `Serial`. |
| 9q | **Valid; simplified.** Unused scanner recovery is removed; duplicated color helpers in the CLI and health view use the existing shared `cli` definitions. No new helper framework was introduced. |

Scheduler behavior was checked against the pinned
[ESP-IDF FreeRTOS documentation](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/api-reference/system/freertos_idf.html).

## Tooling and missing API coverage

| ID | Verification and disposition |
| --- | --- |
| 10a | **Valid; fixed and reproduced.** The actual `cfg` output containing `Timeout / offline threshold` was classified as failure. Five bare status-token patterns now match uppercase emitted statuses without matching ordinary prose. Existing structured diagnostic patterns and the `cfg` expected-output pattern are retained. Uppercase `state=OFFLINE` must still fail, so restricting everything to `Status:` would be too narrow. |
| 10b | **Valid; fixed and mutation-tested.** The IDF mandatory-command guard checks `strcmp(cmd, "...")` inside dispatch rather than a quoted word anywhere in the file. A regression removes the `diag` handler while leaving its real help entry and verifies rejection. This remains a source contract checker, not a complete C++ parser or execution proof. |
| 10c | **Valid test gap; addressed.** The fake records transaction addresses. Device addresses and general-call `0x00`/`0x06` are checked for synchronous reset, reset/reapply and poll reset/reapply. No production address-routing defect was found. |
| 10d | **Valid fake defect; fixed.** A zero FLAGS write no longer clears the fake register. Non-zero writes retain the documented conversion-ready clear behavior. |
| 10e | **Valid coverage gaps; addressed.** Tests exercise raw configuration one-shot/validation behavior, raw FLAGS readiness capture, CRC warning propagation through blocking lux, online state, interrupt latch and overload decoding. Existing poll-budget tests remain in place. |

CI now discovers all `tools/test_*.py` tests, including real CLI integer-parser
boundary cases compiled from the source functions, rather than running only
the HIL transcript parser tests.

## Rechecking the report's other claims

The report's prior-fix list was checked individually:

| Previous claim | Result |
| --- | --- |
| Conversion-time guard; unreachable `luxToThreshold()` branch | Present and retained; see 7h. |
| `crcValid` and saturating consecutive-failure documentation | Correct in the baseline; updated CRC freshness contract does not change the fact that `crcValid` is always computed. |
| OFFLINE exception list | Present and consistent with the recovery/reset/probe paths. |
| SOT resolution `47.344` to `57.344` | Correctly annotated correction to vendor arithmetic, not an unmarked quotation. |
| CRC nibble explanation | Correct: the calculated nibble is compared with the received nibble. |
| PicoStar pin-table view | Correct: Figure 6-1 specifies Top View. |
| Vendor PDFs excluded from packages | Exclusion present; package validation verifies the current artifact rather than reusing the report's approximate size. |
| IDF generated-file ignores | Present for `sdkconfig`, `sdkconfig.old` and `dependencies.lock`. |
| Cross-project version-generator code removed | Confirmed absent; version generation/check remains functional. |
| Dead `TransportAdapter.h` alias removed | Confirmed absent along with its checker dependency. |
| CI SHA-pinning guard restored | Present, invoked in CI, and passing. |
| Self-check threshold restore and one-shot config status | See 9a: incomplete fix corrected; 9b: existing fix verified. |
| Historical report/readiness-checker cleanup | Removed paths remain absent; durable contracts and tests replace dated prose checks. App-note summaries needed two small additional corrections: the high-speed brief discusses display response, and the light-detection article omits PicoStar dimensions specifically, not all package dimensions. |

The claims under “Raised but refuted” were not accepted on the strength of the
earlier reviewer vote:

| Claim | Independent result |
| --- | --- |
| `pkg pico` can never succeed | Rejection upheld. The CLI rebuilds configuration; it does not call `setPackageVariant()`. Profile rollback is separately fixed in 9i. |
| `parseU32` accepts negative input | **Original rejection overturned.** `strtoul()` accepts a leading minus sign; checking trailing text does not reject it. Unsigned parsers now reject negative tokens and overflow, and signed parsing validates its bounds. Tests include whitespace-prefixed negatives, decimal/hex limits, trailing text and values beyond the host integer range. |
| HIL overall timeout can silently drop work | Rejection upheld. The runner appends an explicit FAIL result before stopping. |
| FakeBus must model burst-disabled overlong reads | Rejection upheld. No vendor-defined byte pattern should be invented. Tests inspect requested transaction framing instead. |
| IDF checker regex can never match | Rejection upheld. The challenged expression is reachable for the source shape it checks; 10b addresses the separate real handler-coverage defect. |
| `end()` necessarily diverges during measurement configuration | Original concrete threshold scenario rejected; the cancellation asymmetry is handled in 7k. |
| Counter-gate overloads already behave differently | No demonstrated baseline caller-visible difference in the cited contexts; clock unification removes the duplicate logic anyway. |
| `poll()` switch/post-loop code should be deleted | No useful simplification. Retained exhaustive switch handling and return coverage under strict compilation; runtime unreachability alone is not a reason to remove them. |
| Fake one-shot timing never differs by a millisecond | **Original rejection overturned.** At 100 ms with wake plus forced auto-range, the fake waited 102 ms while the driver budget is 101 ms. The fake now adds microseconds and rounds once. Other fractional settings are checked too. |
| Dirty-cache reason is never surfaced | Rejection upheld. Direct dirty-error diagnostics and `SettingsSnapshot` already expose the reason; no additional diagnostic abstraction is needed. |

## Validation and limits

| Check | Result |
| --- | --- |
| Native unit/contract suite | **162/162 passed**, including 29 added tests. Original tests were adjusted where they encoded the audited defect or supplied a caller epoch different from their configured clock hook. |
| Strict framework-neutral consumer | **Passed** with `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`. |
| Arduino ESP32-S3 | **Passed**, `esp32s3dev`. |
| Arduino ESP32-S2 | **Passed**, `esp32s2dev`. |
| Python tooling regressions | **19/19 passed**, including 52 compiled C++ integer-parser cases. |
| HIL parser self-test | **Passed** without a serial device. |
| Core/CLI/IDF/action-pin/version contracts | **All passed**. |
| Generated files | **All up to date**, including generated `Version.h`. |
| Clean package consumer | **Passed** against the packed library. |
| Package inspection | **Passed**: no vendor PDF members; generated `Version.h` is included. |
| Whitespace validation | `git diff --check` **passed**. |
| Native ESP-IDF ESP32-S2/S3 | Local toolchain unavailable; final verification uses repository CI. |

The initial Arduino command failed before compilation because inherited
`PLATFORMIO_CORE_DIR=C:\pio` selected an incomplete auxiliary Python environment.
Using the intact existing Core then exposed Windows path-length failure while
extracting SDK headers. Both targets built successfully with invocation-local
environment overrides using the existing installation and short cache/package
paths:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $env:USERPROFILE '.platformio'
$env:PLATFORMIO_CACHE_DIR = 'C:\pio\.cache'
$env:PLATFORMIO_PACKAGES_DIR = 'C:\pio\packages'
$env:PLATFORMIO_PLATFORMS_DIR = 'C:\pio\platforms'
.\scripts\pio.cmd run -e esp32s3dev -e esp32s2dev
```

No additional PlatformIO Core was installed and no machine-wide environment
setting was changed. These are local validation-environment details, not new
repository or consumer requirements.

Native transports prove source behavior, not physical sensor, optical, INT,
address-pin, FIFO or fault-recovery behavior. No board was flashed or hardware
validation claimed during this review.

This is an unreleased maintenance change. `library.json` remains the version
source of truth; no version header was hand-edited and no release tag was made.
