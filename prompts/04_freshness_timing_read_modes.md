# Prompt 4 — Sample Freshness, Readiness, Continuous Mode, One-Shot Timing, and INT/Counter Semantics

Continue on `hardening/opt4001-industry-readiness`. Complete only this prompt, commit, sync, and stop.

## Goal of this prompt

Fix the P0 measurement/readiness issue: continuous and one-shot reads must not over-report stale or premature samples. Freshness must be tied to OPT4001 hardware evidence: `CONVERSION_READY_FLAG`, INT pin state where available, or output counter changes.

The exploration report found that software time alone can mark continuous samples ready, `conversionReady()` can remain sticky, and `readSample()` can leave readiness true after a read.

## Required first steps

```bash
git status --short
git branch --show-current
```

Stop if dirty.

## Subagents

Spawn:

1. **freshness-contract subagent**
   - Propose explicit API semantics: latest sample vs fresh sample.
2. **datasheet-timing subagent**
   - Re-check local datasheet sections for conversion times, auto-range overflow, flags clear behavior, counter, FIFO, INT modes.
3. **implementation subagent**
   - Inspect `tick()`, `conversionReady()`, `readSample()`, `tryReadSample()`, `readBlocking*()`, `startConversion()`.
4. **test subagent**
   - Design fake-bus tests for stale counter, delayed auto-range, flags, and one-shot/continuous transitions.
5. **API-doc subagent**
   - Plan docs and Doxygen updates so users understand latest/fresh semantics.

## Required semantic decision

Define and document these concepts:

- **latest sample**: the current output register contents, even if the same sample was already read.
- **fresh sample**: a sample that completed after the previous fresh-read observation or has a changed hardware counter/ready flag.
- **not ready**: no completed sample is available under the selected freshness contract.
- **cached sample**: last sample stored by the driver for diagnostic/convenience use, not proof of freshness.

If existing API names are ambiguous, either:

1. preserve them but clearly document behavior, and add explicit fresh/latest methods, or
2. adjust behavior if API compatibility allows.

Do not make a breaking change silently.

Possible clean API additions:

```cpp
Status readLatestSample(Sample* out);
Status tryReadFreshSample(Sample* out, bool* didRead);
Status readFreshBlocking(Sample* out, uint32_t timeoutMs);
```

Only add new APIs if they materially improve clarity.

## Freshness implementation requirements

Use hardware evidence:

- `CONVERSION_READY_FLAG` where appropriate.
- Output counter change for repeated continuous readings.
- INT pin if configured and available via `gpioRead`, but do not require INT for PicoStar/YMN.
- Account for auto-range overflow extending beyond nominal conversion time.

Rules:

- A continuous-mode sample read must not keep reporting the same sample as fresh if the counter did not change.
- `tryReadSample()` or fresh equivalent must return no fresh data when the counter is unchanged.
- One-shot completion must not rely solely on nominal time when hardware ready flag says not done.
- After reading a fresh sample, advance/clear the internal freshness state.
- Expose or document counter wraparound behavior; 4-bit counter wraps 15 -> 0.
- CRC warnings must remain visible; CRC error should not be confused with not-ready.

## Blocking behavior

Review `readBlocking()` / `readBlockingLux()`:

- They must have a real timeout guarantee.
- If `nowMs` is missing, either return `INVALID_CONFIG` before starting conversion or rely on a clearly documented finite poll cap. Prefer an explicit `nowMs` requirement for time-correct blocking APIs if this matches project style.
- If `cooperativeYield` is absent, avoid uncontrolled CPU spin or document bounded polling.

Add tests for null `nowMs` behavior.

## Required tests

Add native fake-bus tests for:

1. Continuous mode static counter: first fresh read succeeds, repeated fresh read with same counter returns not ready / didRead false.
2. Counter wraparound 15 -> 0 is handled as fresh.
3. Same lux value but changed counter is treated as fresh.
4. Different result but unchanged counter is treated according to documented contract; be explicit.
5. One-shot nominal time elapsed but `CONVERSION_READY_FLAG` not set: not ready.
6. Auto-range delayed completion beyond nominal time: blocking/try-read behavior waits or reports not ready.
7. `CONVERSION_READY_FLAG` clears on the documented read/write path, or the driver accounts for clear-on-read/write behavior.
8. Missing `nowMs` behavior for blocking APIs.
9. Transition from one-shot to continuous clears stale readiness.
10. CRC error on a fresh sample returns the documented status while still updating or not updating freshness consistently.

## Hardware validation hook

Do not run hardware unless available/requested, but add a CLI or diagnostic plan if helpful:

- print sample counter,
- print CRC validity,
- print flags,
- print conversion time setting,
- log timestamps for continuous samples.

If adding CLI commands, keep them diagnostic-only and update `tools/check_cli_contract.py`.

## Documentation updates

Update README and public Doxygen:

- latest vs fresh semantics,
- counter wrap,
- flag clear behavior,
- auto-range extended completion,
- blocking `nowMs` contract,
- INT availability per package,
- PicoStar vs SOT-5X3 behavior.

## Required local validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
```

Run IDF builds if available and record exact result.

## Report update

Create:

```text
docs/OPT4001_FRESHNESS_TIMING_REPORT.md
```

Include:

- old behavior,
- new latest/fresh contract,
- public API changes,
- tests added,
- remaining hardware validation.

## Commit and sync

```bash
git diff --check
git status --short
git add .
git commit -m "fix: clarify and enforce OPT4001 sample freshness"
git push
```

## Final response for this prompt

Report exact changes, tests, commit hash, sync result, and what remains for Prompt 5.
