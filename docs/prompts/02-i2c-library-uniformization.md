# OPT4001 I2C Uniformization Prompt

Repository: `OPT4001`

Absolute path: `C:\Users\Honza\Documents\Projects\OPT4001`

## Execution Rules

You are working inside this single repository. Implement this prompt directly;
do not repeat the cross-repository audit.

You may spawn subagents for read-only inspection of APIs, tests, I2C
transactions, docs, and diagnostics. Keep final judgment, edits, and
verification in the main agent.

Prefer simple, robust, readable code. Before adding code, inspect whether
existing code can be simplified, reused, tightened, or deleted.

Preserve dirty user changes. Do not commit unless explicitly asked.

## Common Uniformization Target

Apply this shared I2C library contract: injected non-owning transport, `Status` returns, cache-only `getSettings(SettingsSnapshot&) const`, active `probe()`/diagnostics named explicitly, `DriverState` with `state()` and `driverState()`, `isOnline()`, `lastOkMs()`, `lastErrorMs()`, `lastError()`, `consecutiveFailures()`, `totalFailures()`, and `totalSuccess()`.

Keep the common `Err` vocabulary append-only where missing: `OK`, `NOT_INITIALIZED`, `INVALID_CONFIG`, `INVALID_PARAM`, `I2C_ERROR`, `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS`, `DEVICE_NOT_FOUND`, `TIMEOUT`, `BUSY`, and `IN_PROGRESS`. Preserve OPT4001-specific device-ID, CRC, measurement, conversion, and offline codes.

Uniformization is not a new base class or framework. Make only local, source-compatible additions and tests.

## Current State

- Public lifecycle and diagnostics are in `include\OPT4001\OPT4001.h`: `SettingsSnapshot` starts at line 112, diagnostic section starts around line 200, `getSettings(SettingsSnapshot&)` at line 227, and `driverState()` at line 237.
- Status model includes `DEVICE_ID_MISMATCH`, `CRC_ERROR`, `MEASUREMENT_NOT_READY`, `CONVERSION_NOT_READY`, and `OFFLINE`.
- Implementation updates health in `src\OPT4001.cpp:2187`.
- HIL runner exists as `tools\hil_opt4001_runner.py`, with dry-run/list-command support and `--port` required for live serial at `tools\hil_opt4001_runner.py:228-299`.
- Native tests passed 109 tests.

## Best Sources To Adapt

- For HIL parser tests, adapt SSD1315 `tools\test_hil_runner_parser.py` because OPT4001's runner also classifies serial evidence and supports dry-run/operator-style modes.
- For dirty/readback docs, compare INA228 `include\INA228\INA228.h:515-653` and BME280 dirty-state docs where OPT4001 has config-affecting writes.
- Keep OPT4001's existing health API; it already has `driverState()`.

## Implementation Tasks

1. Preserve existing public health/status names. Do not rename `driverState()`, `getSettings()`, or probe/recover APIs.
2. Add host-side parser tests for `tools\hil_opt4001_runner.py`. Cover `--dry-run`, missing `--port`, failure token classification, expected address/device-id parsing if present, and no false hardware PASS.
3. Review `probe()` and device ID behavior. Ensure ACK scan is never documented as identity proof; OPT4001 identity requires the existing device ID path.
4. Confirm all active diagnostic reads are named as active reads and no live I2C is hidden in `getSettings(SettingsSnapshot&)`.
5. If raw writes can leave cached configuration uncertain, make sure the current dirty/readback evidence is exposed in `SettingsSnapshot` and documented with the root error status.
6. Audit every wait/poll path for finite timeout bounds and visible status returns. Normal measurement/register APIs must not hide retries; recovery remains explicit and application-scheduled.
7. If the existing HIL runner is kept under the nonstandard name `tools\hil_opt4001_runner.py`, document that exception or add a thin `tools\run_i2c_hil.py` wrapper. Either path must cover the common minimum `version`, `scan`, `probe`, `settings`, `health`, failure-token classification, and dry-run/parser test contract.

## API Changes Required

- None expected unless task 5 finds an unexposed dirty-state gap.

## Simplifications Before Adding Code

- Prefer parser tests for the existing HIL runner over adding a second HIL script.

## Tests To Add Or Update

- Host HIL parser tests.
- Native bus-silence test for `getSettings()` if not already present.
- Native fault-injection test for any dirty/readback behavior changed.

## Commands To Run

- `pio test -e native`
- `pio run -e esp32s3dev`
- `python tools\hil_opt4001_runner.py --dry-run`
- Live HIL only with `--port <PORT>` and a connected OPT4001 fixture.

## Constraints And Non-Goals

- Do not add bus ownership or hidden retries.
- Do not collapse CRC/device-ID errors into generic I2C errors.
- Preserve distinct timeout, address NACK/device-not-found, data NACK, bus, CRC, device-ID, measurement, and conversion statuses. Do not use `DEVICE_NOT_FOUND` for timeout/data/bus failures.

## Risks And Open Questions

- Open: whether `hil_opt4001_runner.py` should gain a companion `check_hil_contract.py` like BME280/PCA9555/SHT3x.
