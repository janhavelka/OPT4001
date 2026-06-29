# OPT4001 HIL Validation Preparation Report

Date/time: 2026-06-29 16:33 Europe/Prague

Status: pre-HIL preparation only. No hardware-in-loop test was run because no
board with an OPT4001 is currently attached.

## Session Setup

| Field | Value |
| --- | --- |
| Repository path | `C:\Users\Honza\Documents\Projects\OPT4001` |
| Branch | `hardening/opt4001-industry-readiness` |
| Commit | `e8a634f` |
| Dirty status at start | Clean |
| Dirty status after edits | Dirty, limited to HIL tooling/docs/report edits |
| Operating system | Microsoft Windows NT 10.0.26200.0 |
| Python | Python 3.12.10 |
| PlatformIO | PlatformIO Core 6.1.18 |
| Target environments | `esp32s3dev`, `esp32s2dev`, `native` |
| Requested serial port | `COM8` |
| Baud rate | 115200 |
| Detected device identity/address | NOT RUN: no OPT4001 fixture attached |
| Fixture details | NOT RUN: no board with this chip attached |
| Electrical safety assumptions | No live electrical tests performed |

## Tooling Inspection

Existing repository tooling already provided:

- PlatformIO Arduino builds for ESP32-S3 and ESP32-S2.
- Native unit tests.
- Arduino and ESP-IDF diagnostic CLIs.
- `tools/hil_opt4001_runner.py` with serial command groups, dry-run,
  per-command timeout, idle timeout, boot wait, Markdown/JSON logs, and
  guarded fault/reset groups.
- `tools/test_hil_opt4001_runner_parser.py` host parser regression tests.

Pre-HIL gaps found by inspection:

- Non-empty but unrelated serial output could be classified as `PASS`.
- The runner had parser tests but no built-in `--parser-self-test`.
- Boot/banner serial text was discarded instead of being included in logs.
- Numeric timeout/count arguments were not range-validated.
- No simple repeated-command benchmark group existed for quick sample-rate
  timing checks.
- Reconnect support was absent.

## Implemented Pre-HIL Fixes

| Area | Change | Verification |
| --- | --- | --- |
| Parser self-test | Added `--parser-self-test` to `tools/hil_opt4001_runner.py`. | Host parser test and direct command. |
| Expected tokens | Added optional `--strict-expected`; recognized commands with missing evidence tokens classify as `UNKNOWN`. | Host parser test. |
| Classification counts | Log summaries now include `PASS`, `WARN`, `FAIL`, `UNKNOWN`, and `NOT_RUN` columns. | Host parser test. |
| Boot transcript | Live logs preserve boot text in Markdown and JSON. | Host parser test. |
| Benchmark group | Added `--group benchmark`, `--benchmark-command`, and `--benchmark-count`. | Dry-run command. |
| Bounded args | Added positive/non-negative validation for baud, counts, and timeouts. | Host parser test. |
| Reconnect | Added bounded `--reconnect-attempts` / `--reconnect-wait` handling after no-response commands. | Static/parser coverage; live behavior NOT RUN. |
| OFFLINE visibility | Added `OFFLINE` stringification in examples/common helpers and HIL failure-token classification. | Parser tests and example builds. |
| Raw write dirty state | Successful raw writes to config/INT/threshold/FLAGS registers now mark hardware/cache state dirty. | Native test. |
| Freshness evidence | Counter freshness evidence is checked before destructive `FLAGS` reads when that non-destructive path is available. | Native test. |
| Watch timeout | Arduino one-shot `watch` now has a per-conversion deadline. | ESP32-S2/S3 builds. |
| CRC-warning output | ESP-IDF CLI prints data for `CRC_ERROR` sample/lux/burst paths. | IDF contract check. |
| Destructive status reads | Default HIL smoke/all-safe plans no longer include clear-on-read `FLAGS`; explicit `--group status` remains available. | Dry-run and parser tests. |
| Docs | README, hardware procedure, and release checklist now include parser self-test and strict mode guidance. | Docs checks. |

## Exact Commands

Build/static commands run during this pre-HIL pass:

```powershell
python -B tools\test_hil_opt4001_runner_parser.py
python tools\hil_opt4001_runner.py --parser-self-test
python tools\hil_opt4001_runner.py --dry-run --group benchmark --benchmark-command lux --benchmark-count 3
python tools\hil_opt4001_runner.py --dry-run --group all-safe --strict-expected
python tools\check_core_timing_guard.py
python tools\check_cli_contract.py
python tools\check_idf_example_contract.py
python tools\check_version_header_contract.py
python tools\check_clean_consumer_package.py
python tools\check_readiness_claims.py
python tools\check_public_api_docs.py
python scripts\generate_version.py check
pio test -e native
pio run -e esp32s3dev
pio run -e esp32s2dev
pio pkg pack
git diff --check
```

Commands intentionally not run:

```powershell
pio run -e esp32s3dev -t upload --upload-port COM8
python tools\hil_opt4001_runner.py --port COM8 --baud 115200 --cli arduino --group all-safe --strict-expected
python tools\hil_opt4001_runner.py --port COM8 --baud 115200 --cli arduino --group benchmark --benchmark-command read --benchmark-count 50
```

Reason: no board with an OPT4001 is attached.

## HIL Summary

| PASS | WARN | FAIL | UNKNOWN | NOT_RUN |
| ---: | ---: | ---: | ---: | ---: |
| 0 | 0 | 0 | 0 | 20 |

No serial transcript was captured. No `hil_logs/` evidence was produced.

## Detailed HIL Matrix

| Test id | Feature area | Command or step | Expected result | Observed result | Elapsed | Result | Notes |
| --- | --- | --- | --- | --- | ---: | --- | --- |
| HIL-001 | Build/flash | `pio run -e esp32s3dev -t upload --upload-port COM8` | Firmware uploaded | No board attached | 0 s | NOT RUN | User explicitly excluded HIL. |
| HIL-002 | Boot | Open serial `COM8` at 115200 | Boot banner and CLI prompt | No board attached | 0 s | NOT RUN | No transcript. |
| HIL-003 | Version | `version` | Version/build info | No board attached | 0 s | NOT RUN | Covered only by dry-run plan. |
| HIL-004 | Scan | `scan` | ACK/bus-presence evidence only | No board attached | 0 s | NOT RUN | Scan is not identity proof. |
| HIL-005 | Identity | `probe`, `id` | DEVICE_ID register pattern `0x0121` | No board attached | 0 s | NOT RUN | Identity requires DEVICE_ID path. |
| HIL-006 | Settings | `cfg` | Cached/config readback | No board attached | 0 s | NOT RUN | No live settings captured. |
| HIL-007 | Health | `state` / `drv` | READY/DEGRADED/OFFLINE state and counters | No board attached | 0 s | NOT RUN | No health evidence captured. |
| HIL-008 | One-shot read | `read`, `lux`, `mlux`, `ulux` | Fresh sample or documented warning | No board attached | 0 s | NOT RUN | No optical evidence. |
| HIL-009 | Raw/latest data | `sample`, `sampleage` | Cached sample fields and age | No board attached | 0 s | NOT RUN | No sample evidence. |
| HIL-010 | Timing sweep | `ctime ...`, `read` | Bounded latency by conversion setting | No board attached | 0 s | NOT RUN | No timing data. |
| HIL-011 | Freshness | `start`, `poll` / `drdy`, repeated reads | No stale duplicate freshness | No board attached | 0 s | NOT RUN | Native tests cover API logic only. |
| HIL-012 | FIFO/burst | `burst`, `readburst`, `slot 0..3` | Four-slot decode and counters | No board attached | 0 s | NOT RUN | No FIFO evidence. |
| HIL-013 | Threshold helpers | `threshold`, `thcalc`, `thdecode` | Valid readback and safe limits | No board attached | 0 s | NOT RUN | No hardware threshold evidence. |
| HIL-014 | INT behavior | `int ...`, `intpin`, logic capture | INT polarity/latch/pulse evidence | No board attached | 0 s | NOT RUN | Requires SOT-5X3 INT fixture. |
| HIL-015 | Reset/reapply | `resetreapply` | Bus-wide reset and config replay | No board attached | 0 s | NOT RUN | Requires safe bus topology. |
| HIL-016 | Recovery | `recover` | Manual recovery from fault/offline | No board attached | 0 s | NOT RUN | No safe fault fixture. |
| HIL-017 | Fault classification | wrong address / disconnect / stuck bus | Precise visible status | No board attached | 0 s | NOT RUN | Requires controlled fixture. |
| HIL-018 | Benchmark | `--group benchmark --benchmark-command read` | min/mean/max command timing | No board attached | 0 s | NOT RUN | Runner support added. |
| HIL-019 | Stress | `stress`, `stress_mix` | bounded success/failure counts | No board attached | 0 s | NOT RUN | Runner count/time bounds available. |
| HIL-020 | 8-hour soak | bounded all-safe mix | 8-hour summary and failures | No board attached | 0 s | NOT RUN | Duration 0. |

## Timing And Sampling Results

No timing, sampling-rate, optical, FIFO, interrupt, reset, recovery, or soak
measurements were captured because no OPT4001 hardware was attached.

## Soak Summary

| Field | Value |
| --- | --- |
| Result | NOT RUN |
| Duration | 0 seconds |
| Start/end | Not started |
| Command mix | Not run |
| Sample count | 0 |
| Error count | 0 |
| Reset/recovery count | 0 |
| Worst latency | Not measured |
| Health-state changes | Not observed |
| Script adjustments during soak | None |

## Audit Findings

### Medium: Ambiguous Output Could Become A False HIL PASS

- Reference: `tools/hil_opt4001_runner.py`, `classify_output()`.
- Evidence: code inspection; no hardware evidence.
- Current behavior before fix: any non-empty output without a failure or warning
  token classified as `PASS`.
- Risk: a prompt echo, truncated response, or unrelated boot text could be
  recorded as successful device evidence.
- Fix implemented: `--strict-expected` with command-specific expected-token
  checks classifies missing evidence as `UNKNOWN`.
- Tests: host parser regression.
- HIL regression: run live smoke with `--strict-expected`.

### Low: Runner Self-Test Was External Only

- Reference: `tools/test_hil_opt4001_runner_parser.py`.
- Evidence: code inspection; no hardware evidence.
- Current behavior before fix: parser tests existed but the runner had no
  built-in self-test flag.
- Risk: operators could skip parser validation before live HIL.
- Fix implemented: `python tools\hil_opt4001_runner.py --parser-self-test`.
- Tests: host parser regression and direct self-test command.
- HIL regression: not applicable; self-test is host-only.

### Low: Boot Transcript Was Discarded

- Reference: `tools/hil_opt4001_runner.py`, serial boot drain path.
- Evidence: code inspection; no hardware evidence.
- Current behavior before fix: boot text was read and ignored.
- Risk: reset/reconnect and early firmware diagnostics could be lost.
- Fix implemented: Markdown/JSON logs include a boot transcript section.
- Tests: host parser regression.
- HIL regression: confirm live log contains boot banner.

### Low: Stress/Timing Arguments Accepted Unsafe Values

- Reference: `tools/hil_opt4001_runner.py`, `parse_args()`.
- Evidence: code inspection; no hardware evidence.
- Current behavior before fix: zero or negative counts/timeouts could be parsed.
- Risk: confusing no-op runs or invalid timing behavior.
- Fix implemented: positive/non-negative argument validators.
- Tests: host parser regression.
- HIL regression: not applicable.

### Medium: OFFLINE Status Was Not Visible In Examples Or HIL Tokens

- Reference: example `errToStr()` helpers and HIL failure-token list.
- Evidence: code inspection; no hardware evidence.
- Current behavior before fix: `Err::OFFLINE` could print as `UNKNOWN`, and HIL
  classification did not explicitly fail on `OFFLINE`.
- Risk: offline-latch evidence could be missed or misclassified.
- Fix implemented: added `OFFLINE` strings and failure-token classification.
- Tests: host parser regression and example builds.
- HIL regression: run a controlled offline/fault fixture and confirm `OFFLINE`
  is recorded as failure evidence.

### Medium: Raw Register Writes Could Desynchronize Cache Without Dirty State

- Reference: `writeRegister16()` and dirty-state diagnostics.
- Evidence: code inspection; no hardware evidence.
- Current behavior before fix: successful raw writes to configuration-affecting
  registers left `hardwareConfigDirty()` false.
- Risk: HIL `wreg` diagnostics could alter hardware while settings snapshots
  still looked authoritative.
- Fix implemented: successful raw writes to configuration, INT configuration,
  thresholds, or FLAGS now mark dirty state. A dirty root status of `OK` means
  the uncertainty came from an intentional successful raw write.
- Tests: native raw-write dirty-state regression.
- HIL regression: run `wreg` on a safe fixture, confirm `cfg` reports dirty,
  then `recover` clears dirty state.

### Medium: Freshness Polling Could Consume FLAGS Before Counter Evidence

- Reference: `_refreshReadinessEvidence()`.
- Evidence: code inspection; no hardware evidence.
- Current behavior before fix: FLAGS was polled before the non-destructive sample
  counter probe.
- Risk: threshold/INT validation evidence could be cleared during ordinary
  sample polling even when counter evidence was sufficient.
- Fix implemented: when counter probing is allowed, counter evidence is checked
  before FLAGS.
- Tests: native counter-freshness test preserves latched high/low flags.
- HIL regression: run threshold/flags validation with a logic/analyzer fixture.

### Medium: Arduino One-Shot Watch Could Wait Forever

- Reference: Arduino example `watch` one-shot state.
- Evidence: code inspection; no hardware evidence.
- Current behavior before fix: after `startConversion()` succeeded, repeated
  `tryReadSample()` calls had no per-conversion deadline.
- Risk: a missing ready signal could leave a watch session active forever.
- Fix implemented: added a deadline based on `getOneShotBudgetMs()` plus the
  existing blocking-read margin.
- Tests: ESP32-S2/S3 builds.
- HIL regression: force a no-ready condition on a safe fixture and confirm watch
  records `TIMEOUT` and advances.

### Low: ESP-IDF CLI Dropped Data On CRC Warnings

- Reference: ESP-IDF read/lux/burst command branches.
- Evidence: code inspection; no hardware evidence.
- Current behavior before fix: data printed only on `OK`.
- Risk: decoded fields preserved by the driver on `CRC_ERROR` would be absent
  from HIL logs.
- Fix implemented: ESP-IDF CLI now prints data for `CRC_ERROR` like the Arduino
  CLI.
- Tests: IDF contract check and Arduino example builds.
- HIL regression: inject or observe CRC warning and confirm decoded data is
  present.

## Final Verification

Updated after local validation:

| Command | Result |
| --- | --- |
| `python -B tools\test_hil_opt4001_runner_parser.py` | PASS |
| `python tools\hil_opt4001_runner.py --parser-self-test` | PASS |
| `python tools\hil_opt4001_runner.py --dry-run --group benchmark --benchmark-command lux --benchmark-count 3` | PASS |
| `python tools\hil_opt4001_runner.py --dry-run --group all-safe --strict-expected` | PASS |
| `python tools\check_core_timing_guard.py` | PASS |
| `python tools\check_cli_contract.py` | PASS |
| `python tools\check_idf_example_contract.py` | PASS |
| `python tools\check_version_header_contract.py` | PASS |
| `python tools\check_clean_consumer_package.py` | PASS |
| `python tools\check_readiness_claims.py` | PASS |
| `python tools\check_public_api_docs.py` | PASS |
| `python scripts\generate_version.py check` | PASS |
| `pio test -e native` | PASS, 112 tests |
| `pio run -e esp32s3dev` | PASS |
| `pio run -e esp32s2dev` | PASS |
| `pio pkg pack` | PASS; generated archive removed |
| `git diff --check` | PASS |

## Limitations

- No OPT4001 board is attached.
- No firmware was uploaded.
- No serial boot transcript was captured.
- No optical, FIFO, INT, address-pin, reset/recovery, fault-injection, timing,
  sample-rate, or 8-hour soak evidence was captured.
- This report does not claim hardware validation or production readiness.
