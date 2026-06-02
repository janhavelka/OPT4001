# Prompt 1 — Branch, Baseline, AGENTS.md, and Finding-to-Prompt Plan

You are working on the OPT4001 repository. This is the first prompt in a sequence. You will receive later prompts one by one. Do not jump ahead.

The attached/audited source report is:

```text
docs/OPT4001_INDUSTRY_READINESS_EXPLORATION_REPORT.md
```

If the file name differs, locate the existing OPT4001 industry-readiness exploration/audit report and use it as the source of truth.

## Goal

Create the production-readiness branch, capture baseline evidence, update `AGENTS.md`, and create a report-grounded work plan that maps every finding from the exploration report to a later implementation prompt.

Do not implement the functional fixes yet in this prompt.

## Required first commands

```bash
git status --short
git branch --show-current
git log --oneline -5
```

If the working tree has uncommitted user changes, stop and report them. Do not overwrite or stash user work unless explicitly told.

Start the hardening branch:

```bash
git checkout -b hardening/opt4001-industry-readiness
```

If it already exists:

```bash
git checkout hardening/opt4001-industry-readiness
```

## Subagents

Spawn these subagents and request short factual findings, with file paths and line references where possible:

1. **finding-index subagent**
   - Read the exploration report.
   - Extract all high-, medium-, and low-severity findings.
   - Produce a table mapping each finding to one of the later prompts.

2. **core-contracts subagent**
   - Inspect `include/OPT4001/*.h` and `src/OPT4001.cpp`.
   - Confirm the lifecycle, copy/move, raw register, blocking, and sample-readiness findings.

3. **device-correctness subagent**
   - Confirm report findings against local OPT4001 datasheet/extracted docs.
   - Focus on package variants, device ID register, result/CRC/counter registers, flags, conversion timing, FIFO, thresholds, INT, address rules.

4. **build-ci subagent**
   - Inspect `.gitignore`, `scripts/generate_version.py`, `platformio.ini`, `CMakeLists.txt`, `idf_component.yml`, `.github/workflows/ci.yml`.
   - Confirm the `Version.h` and ESP-IDF reproducibility gaps.

5. **docs-examples subagent**
   - Inspect README, CHANGELOG, SECURITY, ASSUMPTIONS, examples.
   - Confirm where claims exceed evidence and where examples should be labeled diagnostic.

## AGENTS.md update

Create or update `AGENTS.md` with OPT4001-specific rules:

```markdown
# OPT4001 repository rules for coding agents

- Core code under `include/` and `src/` must remain framework-neutral: no Arduino, Wire, ESP-IDF, FreeRTOS, Serial, logging frameworks, global bus objects, hidden framework delays, pin ownership, task ownership, or heap-heavy framework types.
- I2C is injected and non-owning. Bus ownership, locking, timeout policy, reset-line ownership, INT GPIO ownership, and application scheduling belong to examples/adapters or the application.
- Public fallible APIs must return structured `Status`; do not silently ignore failed I2C writes or collapse diagnostics without documentation.
- Public APIs are not ISR-safe unless explicitly proven. Driver instances are not internally thread-safe; external serialization is required.
- Transport callbacks must not re-enter the same driver instance.
- PicoStar/YMN and SOT-5X3/DTS package differences must remain explicit. PicoStar lacks ADDR and INT. SOT-5X3 has ADDR and INT.
- Fresh sample semantics must be tied to hardware evidence: `CONVERSION_READY_FLAG`, INT when configured and available, or output counter changes.
- Do not treat repeated current output-register reads as new fresh samples unless the sample counter/ready evidence says so.
- Numeric helpers must validate exponent/mantissa/threshold ranges and must not perform undefined shifts.
- Multi-register configuration paths must either avoid partial hardware/cache divergence or expose a dirty/resync-required state.
- The current Arduino and ESP-IDF CLIs are diagnostic/bring-up examples unless they clearly demonstrate production bus management.
- Do not claim hardware validation, optical validation, interrupt validation, address-pin validation, FIFO validation, or pure ESP-IDF validation without captured evidence.
- After each hardening prompt: run checks, update a report, commit, push/sync if possible, and stop.
```

## Work plan document

Create:

```text
docs/OPT4001_HARDENING_FINDING_TO_PROMPT_PLAN.md
```

It must contain:

- branch name,
- baseline commit,
- source audit report filename,
- finding-to-prompt mapping,
- prompt sequence,
- “done criteria” per finding,
- exact checks attempted in this prompt,
- statement that no functional fixes were implemented in Prompt 1.

Map at least these findings:

| Exploration finding | Later prompt |
| --- | --- |
| H1 `Version.h` / clean checkout / manual / ESP-IDF reproducibility | Prompt 2 |
| H2 public raw register APIs while uninitialized | Prompt 3 |
| copy/move not deleted | Prompt 3 |
| missing thread/ISR/reentrancy public contracts | Prompts 3 and 8 |
| H4 weak probe/device-ID/transport diagnostics | Prompt 4 |
| H3 stale/premature continuous readiness | Prompt 5 |
| blocking/readiness `nowMs`/yield behavior | Prompt 5 |
| threshold/raw lux overflow / undefined shifts | Prompt 6 |
| missing independent lux/CRC/threshold/range/conversion-time vectors | Prompt 6 |
| FIFO CRC/freshness/ordering gaps | Prompt 7 |
| multi-register partial-state / cache dirty tracking | Prompt 7 |
| INT/threshold/flags semantics and hardware validation gap | Prompts 7 and 9 |
| production/validation claims exceed evidence | Prompt 8 |
| ESP-IDF CI/build readiness and loose include boundary | Prompt 8 |
| ESP-IDF CLI blocking `tick()` | Prompt 8 |
| SECURITY.md and metadata stale | Prompt 8 |
| hardware/optical/address/INT/FIFO/fault validation pending | Prompt 9 |

## Baseline checks

Run all available and record exact results:

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

If a command is missing or fails, record exact output. Do not invent pass results.

Remove package artifacts after `pkg pack` if not meant to be committed.

## Commit and sync

```bash
git diff --check
git status --short
git add AGENTS.md docs/OPT4001_HARDENING_FINDING_TO_PROMPT_PLAN.md
git commit -m "docs: map OPT4001 hardening findings to prompt sequence"
git push -u origin hardening/opt4001-industry-readiness
```

If there is no remote or push fails, report exactly why.

## Final response

Report:

- branch,
- subagents used,
- files changed,
- baseline checks and exact results,
- commit hash,
- sync/push result,
- which finding Prompt 2 will fix.
