# Prompt 1 — Baseline, Branch, AGENTS.md, and Work Plan

You are working on the OPT4001 repository. You will receive a series of prompts one by one. Complete only this prompt now. Later prompts will logically and chronologically follow.

This is a production/industry-readiness hardening sequence based on the existing exploration report. Do not jump ahead into later work unless it is required to complete this prompt safely.

## Goal of this prompt

Create the hardening branch, capture the baseline, update `AGENTS.md`, and create a concise implementation plan based on the exploration report. Do not yet perform broad code hardening.

## Required first steps

Run:

```bash
git status --short
git branch --show-current
git log --oneline -5
```

If the working tree is dirty, stop and report the uncommitted files. Do not overwrite user work.

Create or switch to the intended branch:

```bash
git checkout -b hardening/opt4001-industry-readiness
```

If it already exists:

```bash
git checkout hardening/opt4001-industry-readiness
```

Then run:

```bash
git status --short
```

## Subagents

Spawn subagents and ask for short, factual reports:

1. **core-contracts subagent**
   - Review lifecycle, `begin()`, `end()`, raw register APIs, copy/move semantics, thread/ISR contracts.
2. **device-correctness subagent**
   - Review OPT4001 datasheet-specific assumptions: package variants, address rules, ID register, conversion-ready flag, counter, FIFO, CRC, threshold registers, INT behavior.
3. **build-ci subagent**
   - Review Version.h generation, clean checkout, PlatformIO, root CMake, ESP-IDF metadata, CI jobs.
4. **tests-fault subagent**
   - Review fake-bus coverage gaps and propose precise test additions.
5. **docs-examples subagent**
   - Review README, Doxygen comments, example labeling, SECURITY.md, production/validation claims.

Each subagent must reference concrete files and line numbers where possible.

## AGENTS.md update

Update or create `AGENTS.md` with OPT4001-specific rules:

```markdown
# OPT4001 hardening rules

- Core code in `include/` and `src/` must remain framework-neutral: no Arduino, Wire, ESP-IDF, FreeRTOS, global bus ownership, logging frameworks, heap-heavy framework types, or hidden platform delays.
- The driver core must use injected, non-owning transport callbacks. I2C bus ownership, locking, timeout policy, GPIO interrupt ownership, and reset-line ownership belong to the application or example adapter.
- Public fallible APIs must return `Status` or equivalent structured results. Do not silently ignore transport failures except for explicitly documented best-effort methods.
- Public APIs are not ISR-safe unless explicitly documented and proven. Driver instances are not internally thread-safe; external serialization is required on shared buses.
- Transport callbacks must not re-enter the same driver instance.
- Fresh sample APIs must be tied to OPT4001 hardware state: `CONVERSION_READY_FLAG`, INT, or output counter changes. Do not report stale samples as fresh.
- OPT4001 package differences must be explicit: PicoStar/YMN lacks ADDR and INT; SOT-5X3/DTS has ADDR and INT. Do not claim features for a package that lacks the pin.
- Do not claim hardware, optical, interrupt, address-pin, FIFO, or pure ESP-IDF validation unless there is captured evidence.
- Each prompt in this sequence must end with tests/checks, a short report, `git diff --check`, `git status --short`, commit, and sync/push if remote is available.
```

## Create a staged work-plan note

Create or update:

```text
docs/OPT4001_HARDENING_WORK_PLAN.md
```

It must contain:

- branch name,
- source exploration report name,
- P0/P1/P2 issues,
- planned prompt sequence,
- explicit statement that implementation will happen in chunks,
- explicit statement that hardware validation remains pending unless actually run.

Use this priority structure:

### P0 — Must fix before production claim

1. Clean checkout/manual/ESP-IDF build reproducibility and `Version.h`.
2. Lifecycle guards for public raw single-register APIs.
3. Continuous/one-shot freshness semantics.
4. Probe/device-ID diagnostics.
5. Documentation honesty around production and validation claims.

### P1 — Should fix before release/merge

1. Threshold/raw lux overflow and undefined shifts.
2. Independent lux, CRC, threshold, range, conversion-time vectors.
3. Partial multi-register dirty/resync state.
4. Per-sample FIFO CRC behavior.
5. Address matrix and transport fault-injection tests.
6. ESP-IDF CI/contract coverage.
7. Public Doxygen operational contracts.
8. IDF NACK mapping and IDF console/tick behavior.

### P2 — Later hardening

1. Copy/move deletion or explicit semantics.
2. Shared-bus integration notes.
3. Docs consistency and SECURITY.md version metadata.
4. Doxygen generation validation.
5. Hardware validation log templates and captured runs.

## Baseline checks

Run whatever exists and is safe:

```bash
python --version
python -m platformio --version
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
idf.py --version
```

If any command is unavailable, record exactly why. Do not invent pass results.

Remove generated package artifacts after pack validation if they are not meant to be committed.

## Commit and sync

Before committing:

```bash
git diff --check
git status --short
```

Commit only the intended files:

```bash
git add AGENTS.md docs/OPT4001_HARDENING_WORK_PLAN.md
git commit -m "docs: plan OPT4001 industry hardening"
```

Sync/push if a remote is configured:

```bash
git push -u origin hardening/opt4001-industry-readiness
```

If push is not possible, report why.

## Final response for this prompt

Report:

- branch name,
- subagents used and summary,
- files changed,
- baseline commands run with exact results,
- commit hash,
- push/sync result,
- what remains for Prompt 2.
