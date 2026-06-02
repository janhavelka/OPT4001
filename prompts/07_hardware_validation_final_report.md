# Prompt 7 — Hardware Validation Preparation, Optional HIL Runner, Final Integration Review, and Final Report

Continue on `hardening/opt4001-industry-readiness`. Complete this final prompt, commit, sync, and stop.

## Goal of this prompt

Finalize the hardening sequence with:

1. A comprehensive final report.
2. Hardware/optical validation matrix and operator procedures.
3. Optional Python HIL/device-test runner if it is clearly useful and bounded.
4. Final integration review.
5. Clear merge/release readiness verdict.

Do not claim hardware validation unless actual hardware commands were run and captured during this prompt.

## Required first steps

```bash
git status --short
git branch --show-current
git log --oneline -10
```

Stop if dirty.

## Subagents

Spawn:

1. **integration-review subagent**
   - Review all diffs since branch start.
   - Check for broad accidental refactors, generated artifacts, and framework leakage.
2. **validation-matrix subagent**
   - Turn the audit hardware matrix into concrete operator steps.
3. **hil-runner subagent**
   - Determine whether an automatic Python serial HIL runner can be added safely.
4. **release-readiness subagent**
   - Review README, changelog, package metadata, CI, reports, and remaining gaps.
5. **datasheet-final-check subagent**
   - Re-check the final docs against the local OPT4001 datasheet for package, conversion, FIFO, counter, CRC, INT, address, VDD/IO, and optical-window statements.

## Hardware validation procedure

Create or update:

```text
docs/OPT4001_HARDWARE_VALIDATION_PROCEDURE.md
```

Include:

- board info fields,
- MCU target fields,
- sensor package variant field,
- address pin wiring field,
- firmware hash field,
- command log field,
- reference lux meter field,
- optical window/cover field,
- pass/fail matrix.

Minimum procedures:

### Basic bring-up

```text
version
scan
probe
id
cfg
state
selftest
read
readv / lux
flags
```

Adjust command names to actual CLI.

### Address/package matrix

- PicoStar/YMN: expected fixed address `0x45`; no INT pin.
- SOT-5X3/DTS: validate ADDR=GND/VDD/SDA addresses; document ADDR=SCL caveat.
- Do not claim untested address combinations.

### Conversion-time matrix

Test all 12 conversion times from 600 us to 800 ms.

Expected:

- counters advance plausibly,
- no repeated stale fresh reads,
- timing within host scheduling limits.

### Optical matrix

- dark/covered sensor,
- stable indoor light,
- reference lux meter comparison,
- bright/high-light saturation,
- under glass/window if relevant,
- infrared-rich light source if available.

### FIFO/counter/CRC

- continuous mode with fast conversion,
- burst read,
- slot counters,
- CRC validity,
- stale/empty behavior if implemented.

### Interrupt/INT pin for SOT-5X3 only

- threshold interrupt,
- every-conversion pulse,
- FIFO-full pulse,
- hardware-trigger one-shot if supported,
- logic analyzer or GPIO timestamp capture.

### Fault/recovery

- unplug/replug,
- address NACK,
- timeout if inducible,
- stuck bus if safe,
- brownout/power-cycle,
- offline latch,
- manual recover,
- reset/reapply.

### Framework matrix

- Arduino ESP32-S2,
- Arduino ESP32-S3,
- pure ESP-IDF ESP32-S2 if available,
- pure ESP-IDF ESP32-S3 if available.

## Optional HIL runner

If the repository already has a Python serial HIL pattern or CLI structure, add a bounded automatic tester:

```text
tools/hil_opt4001_runner.py
```

Useful features:

- serial port selection,
- baud selection,
- safe default smoke sequence,
- optional stress count,
- optional conversion-time sweep,
- optional FIFO test,
- optional INT test placeholder requiring operator confirmation,
- log capture to `hil_logs/`,
- summary pass/fail,
- never perform destructive or electrical fault tests without explicit flags.

Do not overbuild if the CLI is not ready. If not implemented, document why.

## Final checks

Run all available:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Run any new scripts added in earlier prompts.

Remove generated artifacts not meant for commit.

## Final report

Create:

```text
docs/OPT4001_HARDENING_FINAL_REPORT.md
```

It must include:

1. Date, branch, commit range.
2. Summary.
3. Original audit blockers and how each was addressed.
4. Public API changes.
5. Core changes.
6. Build/CI changes.
7. ESP-IDF changes.
8. Documentation changes.
9. Tests added.
10. Exact commands run locally with exact results.
11. Commands not run and exact reasons.
12. Hardware validation performed, if any, with raw command/capture references.
13. Hardware validation still pending.
14. Known limitations.
15. Readiness verdict:
    - ready to merge,
    - ready to release,
    - or still blocked.
16. Explicit statement that full industry-grade/field-grade claim still requires hardware/optical/fault validation unless completed.

## Final integration review

Run:

```bash
git diff --stat
git diff --check
git status --short
```

Review:

- no build artifacts,
- no generated tarballs,
- no accidental unrelated files,
- no Arduino/ESP-IDF leakage into core,
- CI claims match actual workflow,
- README claims match evidence,
- public API docs match implementation,
- reports do not invent hardware validation.

## Commit and sync

```bash
git add .
git commit -m "docs: finalize OPT4001 industry hardening report"
git push
```

If no file changes remain, do not create an empty commit unless there is a project convention for it.

## Final response for this prompt

Report:

- final commit hash,
- push/sync result,
- tests run with exact pass/fail,
- whether hardware was actually run,
- final readiness verdict,
- remaining future work.
