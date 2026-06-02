# Prompt 7 — Fix FIFO/Burst CRC Semantics, INT/Flags Clarity, and Partial Hardware-State Dirty Tracking

Continue on `hardening/opt4001-industry-readiness`. Complete only this prompt, commit, sync, and stop.

## Findings from exploration report

FIFO/INT/partial-state gaps:

- FIFO API exists, but CRC/freshness/empty semantics are incomplete.
- Documentation may be inconsistent about FIFO slot order.
- Hardware INT, FIFO-full pulse, threshold interrupt behavior, and address-pin combinations are not validated.
- Threshold/INT register helpers exist but hardware validation is missing.
- Partial multi-register operations can leave hardware/cache divergence; rollback exists but no exposed dirty/resync-required state.
- `readFlagsRaw()` or flags reads may clear hardware flags and must be documented precisely.

## Goal

Make FIFO burst behavior deterministic, preserve per-sample CRC state, document INT/flag semantics, and expose partial hardware state after failed multi-register configuration sequences.

## Required first commands

```bash
git status --short
git branch --show-current
```

Stop if dirty.

## Subagents

Spawn:

1. **fifo subagent**
   - Inspect burst read implementation and datasheet FIFO movement diagram.
   - Confirm slot order: current output register and FIFO0/FIFO1/FIFO2 relationship.

2. **flags-int subagent**
   - Inspect flags, INT configuration, thresholds, fault-count/latch/polarity APIs.
   - Identify all read/clear side effects.

3. **partial-state subagent**
   - Inspect multi-register config writes: thresholds, INT config, reset/reapply, `_applyConfig()`, recover.
   - Propose dirty-state design.

4. **tests subagent**
   - Add fake-bus tests for FIFO CRC and partial write failures.

5. **docs subagent**
   - Update hardware validation matrix and Doxygen.

## FIFO implementation requirements

- Decode all four slots in burst mode even if one slot has CRC error.
- Preserve `crcValid` per decoded sample.
- Return aggregate `CRC_ERROR` if any slot fails CRC, but still populate all slots.
- Document which slot is newest/current and which are previous FIFO slots.
- Do not discard good slots because one slot failed CRC.
- Do not claim FIFO-empty hardware behavior validated unless tested.
- If the API has an output count, ensure it is correct even when CRC fails.

## FIFO tests

Add fake-bus tests:

1. all four slots valid,
2. newest/current slot CRC invalid,
3. middle FIFO slot CRC invalid,
4. last FIFO slot CRC invalid,
5. multiple CRC errors,
6. decoded fields remain populated for all slots,
7. aggregate status is `CRC_ERROR` when any slot invalid,
8. counter sequence/order matches documented behavior.

## INT and flags requirements

- Document `INT` is only on SOT-5X3/DTS.
- PicoStar/YMN must not expose INT GPIO behavior as available.
- INT pin is open-drain/pull-up as per datasheet; app owns GPIO and ISR setup.
- Document INT modes:
  - threshold interrupt,
  - every-conversion interrupt,
  - FIFO-full pulse/interrupt if configured,
  - hardware-trigger one-shot if supported.
- Document read/clear side effects of flag register `0x0C`.
- If API reads flags destructively, name/document it clearly.

If low-risk, improve API naming or add non-ambiguous comments; avoid broad API breakage.

## Partial hardware-state dirty tracking

Implement dirty/resync state for multi-register configuration failures.

Required behavior:

- If a multi-register operation writes one register successfully and a later register write fails, mark `hardwareConfigDirty` or equivalent.
- Preserve original failing `Status`.
- Expose dirty state via:
  - settings snapshot,
  - `hardwareConfigDirty()` accessor,
  - `hardwareConfigDirtyError()` accessor,
  - or equivalent.
- Dirty state persists through unrelated successful reads.
- Dirty state clears only after successful full config resync/recover/reapply.
- If first write fails before any hardware mutation, dirty can remain false.
- Tests must verify all failure positions.

Likely affected operations:

- threshold low/high pair,
- config/control register sequence,
- INT config plus threshold sequence,
- reset + reapply,
- recover reapply.

## Partial-state tests

Add fake-bus tests:

1. `_applyConfig()` fail first write: dirty false if no hardware mutation occurred.
2. fail second write: dirty true.
3. fail third/fourth write if applicable: dirty true.
4. threshold low succeeds, high fails: dirty true and error preserved.
5. dirty state visible in snapshot/accessor.
6. unrelated read does not clear dirty.
7. successful recover/resync clears dirty.
8. failed recover leaves dirty true.

## Documentation updates

Update README/Doxygen:

- FIFO slot ordering,
- per-slot CRC and aggregate status,
- flags clear-on-read/write warning,
- INT package restriction,
- threshold/interrupt hardware validation pending,
- dirty-state meaning and recovery recipe,
- not all GPIO/INT functions are core-owned.

Update finding-to-prompt plan.

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
docs/OPT4001_FIFO_INT_PARTIAL_STATE_REPORT.md
```

Include:

- FIFO behavior before/after,
- CRC policy,
- INT/flags doc changes,
- dirty-state design,
- tests,
- exact command results,
- remaining hardware validation.

## Commit and sync

```bash
git diff --check
git status --short
git add .
git commit -m "fix: harden OPT4001 FIFO and partial config state"
git push
```

## Final response

Report exact changes, tests, commit hash, push result, and that Prompt 8 will fix docs/examples/CI honesty.
