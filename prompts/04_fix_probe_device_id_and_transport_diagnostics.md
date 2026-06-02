# Prompt 4 — Fix H4: Probe, Device-ID Validation, and Transport Diagnostics

Continue on `hardening/opt4001-industry-readiness`. Complete only this prompt, commit, sync, and stop.

## Finding from exploration report

H4: Probe and device-ID diagnostics are too weak for production startup.

Evidence:

- `probe()` converts most transport failures to `DEVICE_NOT_FOUND`.
- `probe()` and `recover()` check only `(deviceId & MASK_DIDH) == DIDH_EXPECTED`.
- `decodeDeviceId()` already knows DIDL/full match semantics.
- Local datasheet register map shows ID register at `0x11` with DIDL and DIDH fields.
- Incorrect/corrupted ID values with matching low DIDH can be accepted.
- IDF transport/status mapping is weak or insufficiently documented.

## Goal

Make `probe()`, `begin()`, and `recover()` preserve useful diagnostic statuses and validate the full documented ID register pattern.

## Required first commands

```bash
git status --short
git branch --show-current
```

Stop if dirty.

## Subagents

Spawn:

1. **datasheet-id subagent**
   - Confirm exact `DEVICE_ID` register bits from local datasheet/extracted markdown.
   - List valid/invalid example values.

2. **probe subagent**
   - Inspect `probe()`, `begin()`, `recover()`, `decodeDeviceId()`.
   - Propose a unified ID validation helper.

3. **transport subagent**
   - Inspect core status mapping and Arduino/ESP-IDF adapters.
   - Decide what status precision is possible.

4. **tests subagent**
   - Add fake-bus status and invalid-ID tests.

## Implementation requirements

### Full ID validation

Implement one shared helper for ID validation, for example:

```cpp
bool _isExpectedDeviceId(uint16_t raw);
DeviceIdInfo decodeDeviceId(uint16_t raw);
```

Presence detection must validate:

- DIDH expected value.
- DIDL expected value.
- reserved/fixed bits according to datasheet.
- no acceptance of invalid high/fixed bits just because low DIDH matches.

Use the same validation path in:

- `probe()`,
- `begin()`,
- `recover()`,
- any strict/init verification path if present.

### Diagnostic status preservation

Status mapping rules:

- Definite address NACK: may map to `DEVICE_NOT_FOUND` or `I2C_NACK_ADDR`; choose one and document it.
- Data NACK: preserve `I2C_NACK_DATA` if transport can distinguish it.
- Timeout: preserve `I2C_TIMEOUT`.
- Bus/arbitration/driver error: preserve `I2C_BUS` or precise status.
- Generic I2C error: preserve generic I2C error; do not call it “not found” unless absence is known.
- Invalid ID after successful I2C read: return `DEVICE_NOT_FOUND` or `DEVICE_ID_MISMATCH` if such status exists; document exact behavior.

If a new status code is justified, add it cleanly and update docs/tests.

### Optional detailed probe

If `probe()` must remain simple, add:

```cpp
Status probeDetailed(DeviceIdInfo* info);
```

or similar. Only add this if it improves field diagnostics without bloating the API.

### IDF adapter mapping

Improve ESP-IDF adapter mapping where possible:

- `ESP_ERR_TIMEOUT` -> `I2C_TIMEOUT`.
- NACK-related return -> NACK-specific status if API provides it.
- invalid state/bus problems -> `I2C_BUS` where defensible.
- unknown `esp_err_t` preserved in detail field if Status supports detail.

Do not invent precision that ESP-IDF does not expose. Add comments documenting limitations.

## Required tests

Add native tests for:

1. `probe()` with valid full ID succeeds.
2. IDs with correct low DIDH but wrong DIDL/fixed bits fail.
3. `begin()` rejects invalid full ID.
4. `recover()` rejects invalid full ID.
5. `probe()` status mapping for:
   - address NACK,
   - data NACK,
   - timeout,
   - bus error,
   - generic I2C error.
6. Optional `probeDetailed()` fills raw ID/decode info on success and invalid ID on mismatch.
7. Health side effects remain as documented: raw diagnostic probe should not incorrectly update tracked health if that was the existing contract.

## Documentation updates

Update README and Doxygen:

- what `probe()` verifies,
- that ID validation is register-pattern validation, not optical validation,
- status mapping table,
- how to diagnose address NACK vs timeout vs invalid ID,
- ESP-IDF mapping limitations.

Update `docs/OPT4001_HARDENING_FINDING_TO_PROMPT_PLAN.md`.

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
docs/OPT4001_H4_PROBE_DEVICE_ID_DIAGNOSTICS_REPORT.md
```

Include:

- old behavior,
- new ID validation,
- status mapping table,
- public API changes,
- tests,
- exact command results.

## Commit and sync

```bash
git diff --check
git status --short
git add .
git commit -m "fix: strengthen OPT4001 probe and device ID diagnostics"
git push
```

## Final response

Report exact changes, tests, commit hash, push result, and that Prompt 5 will fix freshness/readiness.
