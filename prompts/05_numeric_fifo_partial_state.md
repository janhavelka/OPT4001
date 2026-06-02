# Prompt 5 — Numeric Bounds, Independent Vectors, FIFO CRC Semantics, and Partial Configuration Dirty State

Continue on `hardening/opt4001-industry-readiness`. Complete only this prompt, commit, sync, and stop.

## Goal of this prompt

Fix P1 correctness and robustness issues:

1. Prevent overflow/undefined behavior in raw lux and threshold conversions.
2. Add independent lux/CRC/threshold/range/conversion-time vectors.
3. Add dirty/resync state for partial multi-register configuration failures.
4. Decode all FIFO/burst slots and preserve per-sample CRC state even if one slot fails.

## Required first steps

```bash
git status --short
git branch --show-current
```

Stop if dirty.

## Subagents

Spawn:

1. **numeric-correctness subagent**
   - Inspect lux, ADC-code, threshold, milli/micro-lux conversion.
   - Identify all shifts and casts that can overflow or be undefined.
2. **datasheet-vector subagent**
   - Build independent test vectors from datasheet formulas, not driver helpers.
   - Include package-specific LSBs and boundary values.
3. **fifo subagent**
   - Inspect burst/slot order, CRC behavior, stale/empty semantics, and docs inconsistencies.
4. **partial-state subagent**
   - Inspect multi-register apply paths and cache rollback.
   - Design hardware-config-dirty state.
5. **test subagent**
   - Implement fake-bus fault injection and vector coverage.

## Numeric requirements

Fix or guard:

- `rawToLux()` with caller-provided exponent and mantissa.
- Sample decode with exponent values 9-15.
- `thresholdToAdcCodes()` for high threshold exponents.
- Any `mantissa << exponent` or `result << (8 + exponent)` that can overflow.
- Rounding and truncation behavior for threshold packing.

Acceptable approaches:

- Validate sample-style exponent <= 8 and mantissa <= 20 bits.
- Use `uint64_t` for intermediate ADC codes.
- Return `INVALID_PARAM` or documented saturation for unsupported caller inputs.
- Keep public utility behavior deterministic; no undefined shifts.
- Document units and bounds.

## Independent vector requirements

Do not generate expected values with the same private helper under test.

Add fixed vectors for:

- PicoStar LSB and SOT-5X3 LSB.
- exponent 0, 1, 8.
- max 20-bit mantissa.
- invalid sample exponents 9, 15, 31, 32.
- threshold low/default, high/default, max valid threshold register, exponent 15.
- CRC all-zero-ish vector, max fields vector, counter variations, single-bit corruption.
- all 12 conversion-time enum values with expected nominal microseconds/milliseconds.
- address/package validity matrix: PicoStar only `0x45`, SOT-5X3 valid `0x44`, `0x45`, `0x46`, and invalid addresses rejected.

## FIFO / burst requirements

Review `readBurst()` and slot decoding.

Required behavior:

- Decode all four slots even if one slot has CRC error.
- Preserve `crcValid` per sample.
- Return aggregate `CRC_ERROR` if any slot fails CRC, but still populate all decoded slots.
- Document FIFO ordering clearly: newest/current output register plus previous shadow slots according to datasheet. Correct any conflicting local summary.
- Do not drain unbounded FIFO; fixed-size burst is fine.

Add tests:

- CRC error in newest slot only.
- CRC error in middle slot only.
- CRC error in last slot only.
- Multiple CRC errors.
- All slots decoded regardless of aggregate status.
- Counter sequence sanity across the four slots.

## Partial configuration dirty state

Add a dirty state for multi-register apply paths where hardware may have changed before a later I2C failure.

Required behavior:

- If any multi-register operation partially succeeds then later fails, mark configuration dirty/sync-needed.
- Expose dirty state in a snapshot or public diagnostic method.
- Preserve the original failing `Status`.
- Do not clear dirty state just because a later unrelated I2C operation succeeds.
- Clear dirty state only after successful full reapply/readback/resync/recover.
- If strict readback exists or is added, use it carefully and document limitations.

Likely affected paths:

- threshold low/high writes,
- `_applyConfig()` multiple register writes,
- reset + reapply,
- INT config + threshold config combinations.

Add tests:

- fail first write, second write, third write, fourth write in `_applyConfig()`;
- fail high threshold after low threshold succeeds;
- dirty state visible after partial failure;
- dirty state persists through unrelated read;
- dirty state clears after successful recover/resync.

## Copy/move semantics

If not already done, delete copy/move operations for `OPT4001` or explicitly justify safe copy/move semantics.

Add compile-time tests if practical.

## Documentation updates

Update README/public Doxygen:

- numeric bounds,
- lux units,
- rounding policy,
- threshold conversion limits,
- FIFO slot ordering and CRC aggregate behavior,
- dirty partial-configuration state and recovery recipe,
- copy/move/thread/ISR contracts if not already done.

Correct stale docs inconsistencies noted in the audit:

- FIFO order wording,
- PicoStar/app-note temperature statement if present,
- full-scale table typo if present.

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

Run IDF builds if available. Record exact results.

## Report update

Create:

```text
docs/OPT4001_NUMERIC_FIFO_PARTIAL_STATE_REPORT.md
```

Include:

- numeric fixes,
- vector sources,
- FIFO behavior before/after,
- dirty state design,
- tests added,
- remaining limitations.

## Commit and sync

```bash
git diff --check
git status --short
git add .
git commit -m "fix: harden OPT4001 numeric FIFO and config state"
git push
```

## Final response for this prompt

Report files changed, tests, commit hash, push result, and what remains for Prompt 6.
