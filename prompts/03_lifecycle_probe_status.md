# Prompt 3 — Lifecycle Guards, Probe Diagnostics, Device-ID Validation, and Status Precision

Continue on `hardening/opt4001-industry-readiness`. Complete only this prompt, commit, sync, and stop.

## Goal of this prompt

Fix the P0 lifecycle and probe/ID diagnostics issues:

1. Public `readRegister16()` and `writeRegister16()` must not touch I2C while the driver is `UNINIT`, after failed `begin()`, or after `end()`.
2. `probe()` / `begin()` / `recover()` must preserve useful transport diagnostics and validate the full documented device-ID pattern, not only the low 12-bit DIDH field.
3. IDF transport should map errors more precisely where possible.

## Required first steps

```bash
git status --short
git branch --show-current
```

Stop if the tree is dirty.

## Subagents

Spawn:

1. **lifecycle subagent**
   - Inspect `begin()`, `end()`, `readRegisters()`, `readRegister16()`, `writeRegister16()`, internal raw helpers.
   - Identify public methods that can touch I2C while uninitialized.
2. **probe-id subagent**
   - Inspect `probe()`, `recover()`, `decodeDeviceId()`, `Status`, `CommandTable`.
   - Confirm exact ID register semantics from local datasheet extraction.
3. **transport-status subagent**
   - Inspect Arduino and ESP-IDF transports.
   - Identify where NACK/timeout/bus errors are lost.
4. **tests subagent**
   - Design fake-bus tests for lifecycle and probe status matrix.

## Lifecycle implementation

Add initialized guards to public raw single-register APIs.

Required behavior:

- Before `begin()`, `readRegister16()` returns `NOT_INITIALIZED` and does not touch fake bus.
- Before `begin()`, `writeRegister16()` returns `NOT_INITIALIZED` and does not touch fake bus.
- After failed `begin()`, both return `NOT_INITIALIZED` and do not touch fake bus.
- After `end()`, both return `NOT_INITIALIZED` and do not touch fake bus.
- Internal code paths needed during `begin()`, `probe()`, or `recover()` must use private raw helpers that are clearly not public lifecycle APIs.

Do not create recursive public calls in initialization paths.

## Probe and device-ID implementation

Strengthen presence detection:

- Use the existing full decode helper if appropriate.
- Validate DIDH and DIDL / fixed/reserved bits according to the local datasheet extraction.
- Device ID examples with matching low DIDH but wrong fixed bits/DIDL must fail.

Preserve transport errors:

- Definite address NACK may map to `DEVICE_NOT_FOUND`.
- Timeout should remain `I2C_TIMEOUT`.
- Bus error should remain `I2C_BUS`.
- Data NACK should remain `I2C_NACK_DATA` or a documented NACK-specific status.
- Generic transport error should not be collapsed into "device not found" unless absence is actually known.

If public API compatibility matters, add a diagnostic method such as:

```cpp
Status probeDetailed(DeviceIdInfo* outInfo);
```

only if it is clearly useful. Otherwise improve `probe()` behavior directly and document the change.

## ESP-IDF transport status

In `examples/esp_idf/basic`, improve IDF error mapping where possible.

Do not overclaim if ESP-IDF does not distinguish address/data NACK precisely in the API used. At minimum:

- timeout -> `I2C_TIMEOUT`,
- invalid response/NACK -> NACK-specific status if defensible, otherwise documented `I2C_ERROR`,
- bus/arbitration -> `I2C_BUS` if distinguishable.

Add comments where precision is limited by IDF API behavior.

## Required tests

Add native fake-bus tests for:

1. `readRegister16()` before `begin()` returns `NOT_INITIALIZED`, no bus touch.
2. `writeRegister16()` before `begin()` returns `NOT_INITIALIZED`, no bus touch.
3. Failed `begin()` path leaves public raw APIs guarded.
4. `end()` leaves public raw APIs guarded.
5. Probe preserves `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS`, and generic errors according to the documented mapping.
6. Device ID values with wrong DIDL/fixed bits fail even if low DIDH bits match.
7. `recover()` uses the strengthened ID validation.

Update tests to use independent constants, not implementation shortcuts, wherever possible.

## Documentation updates

Update README and public Doxygen comments:

- Lifecycle: which APIs require initialized state.
- Probe semantics: which status codes can be returned.
- ID validation limitation: probe verifies register pattern, not optical performance.
- ESP-IDF transport mapping limitations.

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

Run IDF builds if available:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Record exact results.

## Report update

Create or append:

```text
docs/OPT4001_LIFECYCLE_PROBE_STATUS_REPORT.md
```

Include:

- root cause,
- API behavior before/after,
- public API changes,
- status mapping table,
- test matrix,
- commands run,
- remaining limitations.

## Commit and sync

```bash
git diff --check
git status --short
git add .
git commit -m "fix: guard lifecycle and improve OPT4001 probe diagnostics"
git push
```

## Final response for this prompt

Report files changed, exact tests, commit hash, push result, and what remains for Prompt 4.
