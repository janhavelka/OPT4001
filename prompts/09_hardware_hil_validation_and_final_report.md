# Prompt 9 — Hardware/HIL Validation Procedure, Optional Device Tester, Final Report, and Merge Verdict

Continue on `hardening/opt4001-industry-readiness`. This is the final prompt in the sequence. Complete it, commit, sync, and stop.

## Findings from exploration report

Remaining industry-grade blockers are validation evidence:

- hardware validation missing,
- optical/reference-lux validation missing,
- interrupt/INT validation missing,
- FIFO validation missing,
- address-pin combinations missing,
- pure ESP-IDF builds may still be pending if `idf.py` unavailable,
- fault paths missing: NACK, timeout, unplug/replug, brownout, stuck bus,
- under-glass/optical-window behavior missing.

## Goal

Create the final validation procedures, optional safe HIL runner, and comprehensive final report. Run any available hardware validation only if hardware and serial access are available. Do not invent validation.

## Required first commands

```bash
git status --short
git branch --show-current
git log --oneline -12
```

Stop if dirty.

## Subagents

Spawn:

1. **final-integration subagent**
   - Review all commits on branch.
   - Check no build artifacts, no framework leakage, no broad accidental rewrites.

2. **hardware-validation subagent**
   - Write concrete hardware/optical/INT/FIFO/fault validation matrix.

3. **hil-runner subagent**
   - Decide whether a safe Python serial runner is feasible using the existing CLI.
   - Add it only if bounded and useful.

4. **release-verdict subagent**
   - Evaluate readiness to merge/release based only on evidence.

5. **docs-consistency subagent**
   - Check final README/CHANGELOG/SECURITY/metadata/reports match evidence.

## Hardware validation procedure

Create:

```text
docs/OPT4001_HARDWARE_VALIDATION_PROCEDURE.md
```

It must include fillable fields:

- date,
- operator,
- board,
- MCU target,
- firmware commit,
- library commit,
- sensor package variant,
- sensor address wiring,
- optical setup,
- reference lux meter model/calibration,
- cover glass/window details,
- serial port/baud,
- pass/fail notes.

Include these command groups, adjusted to the actual CLI names:

### Safe smoke

```text
version
scan
probe
id
cfg
state
read
lux
flags
selftest
```

### Conversion-time sweep

Run all 12 conversion times and verify counter/timing/freshness:

```text
ct 0
read
ct 1
read
...
ct 11
read
```

Use actual CLI syntax.

### Freshness/counter

- continuous mode,
- repeated fresh reads,
- check counter increments,
- verify same counter is not reported as fresh.

### Address/package matrix

- PicoStar/YMN: fixed expected address `0x45`, no INT.
- SOT-5X3/DTS: validate supported addresses `0x44`, `0x45`, `0x46` according to ADDR wiring.
- ADDR=SCL caveat must be documented; test only if board wiring supports it.

### FIFO/burst

- enable burst/FIFO mode,
- read burst,
- verify four slot decode,
- verify counter order,
- verify CRC status.

### INT/threshold, SOT-5X3 only

- threshold low/high,
- fault count,
- latch/persistence,
- every-conversion pulse,
- FIFO-full pulse,
- hardware-trigger one-shot if supported.
- Require logic analyzer or timestamped GPIO capture for real validation.

### Optical validation

- dark/covered sensor,
- stable indoor light,
- bright light,
- reference lux meter comparison,
- under glass/window if applicable,
- IR-rich source if available,
- record expected tolerance and environment notes.

### Fault/recovery

- address NACK,
- unplug/replug,
- power cycle/brownout,
- reset/reapply,
- stuck bus if safe and controlled,
- offline latch,
- manual recover.

### Framework matrix

- Arduino ESP32-S2,
- Arduino ESP32-S3,
- pure ESP-IDF ESP32-S2,
- pure ESP-IDF ESP32-S3.

## Optional HIL runner

If the CLI exists and can be driven safely, add:

```text
tools/hil_opt4001_runner.py
```

Safe features:

- serial port and baud args,
- smoke group,
- conversion-time sweep,
- read/stress group,
- FIFO group if CLI supports it,
- optional INT group disabled by default,
- optional destructive/fault group disabled by default,
- log file output under `hil_logs/`,
- clear pass/fail summary.

Do not implement unsafe hardware manipulation without explicit operator flags.

Add README docs for the runner if created.

## Run final local checks

Run all available:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_version_header_contract.py
python tools/check_readiness_claims.py
python tools/check_public_api_docs.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Only run scripts that exist. Record exact results.

If hardware is connected, run a safe smoke validation and capture logs. If not connected, do not claim it.

## Final report

Create:

```text
docs/OPT4001_HARDENING_FINAL_REPORT.md
```

Must include:

1. Date.
2. Branch.
3. Commit range.
4. Source exploration report.
5. Executive summary.
6. Finding-by-finding closure table:
   - H1 Version/build reproducibility.
   - H2 lifecycle raw register APIs.
   - H3 freshness/readiness.
   - H4 probe/device-ID/status.
   - H5 docs/claims.
   - numeric/vector gaps.
   - FIFO CRC gaps.
   - partial-state dirty gaps.
   - ESP-IDF CI/example gaps.
   - public API contracts.
   - metadata.
   - hardware validation.
7. Public API changes.
8. Core behavior changes.
9. Test coverage added.
10. CI/build coverage added.
11. Docs/examples changed.
12. Exact commands run and exact results.
13. Commands not run and why.
14. Hardware/HIL validation performed with log names if any.
15. Remaining validation work.
16. Known risks.
17. Merge verdict:
   - `READY TO MERGE` only if all code/docs/tests pass and remaining gaps are validation-only.
   - `READY TO RELEASE` only if release claims match evidence.
   - `NOT READY` if P0 code/build correctness remains.
18. Explicit statement:
   - “Do not claim full field-proven industry-grade readiness until hardware/optical/INT/fault validation is completed.”

## Final integration review

Run:

```bash
git diff --stat
git diff --check
git status --short
```

Ensure:

- no tarballs,
- no `.pio/`,
- no build artifacts,
- no unintended generated logs unless explicitly documented,
- no unrelated files,
- no Arduino/ESP-IDF leakage into core,
- CI/docs claims match reality.

## Commit and sync

```bash
git add .
git commit -m "docs: finalize OPT4001 industry hardening"
git push
```

If nothing changed, do not force an empty commit unless project convention requires it.

## Final response

Report:

- final commit hash,
- push/sync result,
- all checks and pass/fail results,
- hardware validation status,
- merge/release verdict,
- remaining future work.
