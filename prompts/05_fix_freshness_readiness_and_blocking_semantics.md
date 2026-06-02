# Prompt 5 — Fix H3: Freshness, Readiness, Continuous/One-Shot Semantics, Blocking Contracts

Continue on `hardening/opt4001-industry-readiness`. Complete only this prompt, commit, sync, and stop.

## Finding from exploration report

H3: Continuous/readiness state can over-report stale or premature samples.

Evidence:

- `tick()` marks continuous samples ready by elapsed configured time only.
- `conversionReady()` returns true once `_conversionReady` is true in continuous mode without hardware polling.
- `readSample()` caches a continuous sample but does not clear continuous readiness.
- Datasheet says `CONVERSION_READY_FLAG` is set at conversion end and clears on `0x0C` read/write.
- Datasheet warns auto-range overflow can extend completion beyond nominal conversion time.
- Output register includes counter and CRC fields.
- SOT-5X3 has INT; PicoStar does not.

## Goal

Define and enforce explicit “latest sample” vs “fresh sample” behavior. Do not report repeated old register contents as fresh. Do not read one-shot samples as complete when hardware says not ready. Ensure blocking APIs have bounded timebase behavior.

## Required first commands

```bash
git status --short
git branch --show-current
```

Stop if dirty.

## Subagents

Spawn:

1. **freshness-api subagent**
   - Propose exact API semantics for existing and any new methods.
   - Avoid silent breaking changes.

2. **datasheet-timing subagent**
   - Confirm conversion times, auto-range overflow delay, `CONVERSION_READY_FLAG` clearing, counter behavior, INT availability.

3. **implementation subagent**
   - Inspect `tick()`, `conversionReady()`, `readSample()`, `tryReadSample()`, `readLux()`, blocking reads, `startConversion()`.

4. **tests subagent**
   - Add fake-bus tests for static counter, wraparound, delayed flag, missing `nowMs`.

5. **docs subagent**
   - Update README and Doxygen contracts.

## Required semantic model

Document and implement these definitions:

- **Latest sample**: current output-register contents, may be the same sample as previously read.
- **Fresh sample**: a newly completed conversion observed after the previous fresh sample. Evidence must be hardware-based:
  - `CONVERSION_READY_FLAG`,
  - changed sample counter,
  - INT state if configured and GPIO callback is available.
- **Cached sample**: driver copy for convenience, not proof of freshness.
- **Not ready**: no fresh sample evidence available.

If existing API names cannot be changed without breaking users, preserve them but clarify behavior. Add explicit methods if needed, for example:

```cpp
Status readLatestSample(Sample* out);
Status tryReadFreshSample(Sample* out, bool* didRead);
Status readFreshBlocking(Sample* out, uint32_t timeoutMs);
```

Only add APIs that are clearly useful.

## Implementation requirements

### Continuous mode

- Do not keep `_conversionReady` sticky forever in continuous mode.
- After a fresh sample is read, advance internal “last seen counter” or clear freshness state.
- Repeated reads of the same counter must not count as fresh.
- Counter wraparound must be handled (e.g. 15 -> 0 is fresh).
- Same lux value with changed counter is fresh.
- Changed register contents with unchanged counter must follow a documented policy; prefer counter as freshness authority if datasheet supports it.

### One-shot mode

- If nominal conversion time elapsed but `CONVERSION_READY_FLAG` is not set, report not ready unless there is another valid hardware completion signal.
- Account for auto-range overflow extending completion beyond nominal time.
- Trigger/read state transitions must clear stale readiness.

### INT / GPIO

- INT support is optional and package-specific.
- PicoStar/YMN must not require INT.
- SOT-5X3 can use INT via `gpioRead` if configured, but core should not own GPIO or ISR setup.

### Blocking APIs

- If blocking APIs require `Config::nowMs`, return `INVALID_CONFIG` before starting conversion when missing.
- If blocking APIs do not require `nowMs`, prove they have a bounded finite poll count and document it. Prefer explicit `nowMs` requirement for time-correct behavior.
- `cooperativeYield` may be optional, but CPU spin behavior must be bounded or documented.

## Required tests

Add native fake-bus tests for:

1. Continuous: first fresh sample succeeds.
2. Continuous: repeated same counter is not fresh.
3. Continuous: counter 15 -> 0 is fresh.
4. Continuous: same lux but changed counter is fresh.
5. One-shot: nominal time elapsed but flag not set returns not ready.
6. One-shot: flag set returns sample.
7. Auto-range delayed beyond nominal time is not reported early.
8. `readSample()` / fresh read clears or advances readiness as documented.
9. Transition one-shot -> continuous clears stale readiness.
10. Missing `nowMs` returns `INVALID_CONFIG` before starting conversion if this is the chosen policy.
11. CRC error on a fresh sample returns CRC status and updates freshness according to documented policy.

## Optional CLI diagnostic improvement

If useful and low-risk, update CLI to print:

- sample counter,
- CRC valid/invalid,
- flags,
- fresh/latest state,
- active conversion time,
- package variant.

Update `tools/check_cli_contract.py` if command/help changes.

## Documentation updates

Update README and public Doxygen:

- latest vs fresh API table,
- counter wrap behavior,
- flag clear behavior,
- auto-range timing warning,
- blocking timebase requirements,
- INT availability and GPIO ownership,
- PicoStar/SOT-5X3 differences.

Update the finding-to-prompt plan marking H3 complete/pending.

## Required validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_version_header_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
```

Attempt IDF builds if available.

## Report

Create:

```text
docs/OPT4001_H3_FRESHNESS_READINESS_REPORT.md
```

Include:

- old behavior,
- new latest/fresh definitions,
- API changes,
- tests,
- exact command results,
- hardware validation still pending.

## Commit and sync

```bash
git diff --check
git status --short
git add .
git commit -m "fix: enforce OPT4001 sample freshness semantics"
git push
```

## Final response

Report exact changes, tests, commit hash, push result, and that Prompt 6 will fix numeric/vector gaps.
