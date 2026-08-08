# OPT4001 Naming And Repository-Hygiene Audit

Audit date: 2026-08-08
Target version: 1.2.2

This audit compares OPT4001 with six clean, mature I2C libraries in the local
`Projects/` workspace. The peers were inspected read-only: PCA9555 3.0.2,
INA228 3.0.3, INA3221 3.1.0, MB85RC 4.1.0, RV3032-C7 3.0.1, and LDC1614
3.1.0. BME280 was not used as naming evidence because its worktree was not
clean at inspection time.

The comparison is a consistency rubric, not a reason to copy architecture that
does not fit this driver. Public source compatibility takes precedence over
cosmetic uniformity.

## Evidence-Based Rubric

| Concern | Mature-peer convention | OPT4001 decision |
| --- | --- | --- |
| Fallible result | `Err` plus `Status { code, detail, msg }`; allocation-free static messages | Retain the existing public `Err` and `Status` contract. |
| Enum names | A stable error-name helper is common but spelling varies (`errorName`, `errName`, or no helper); current mature helpers use `"UNKNOWN"` for invalid casts | Retain `errorName(Err)` and `toString(Err)`, and use the shared `"UNKNOWN"` fallback without changing valid enum names. |
| Driver state | Usually `DriverState::{UNINIT, READY, DEGRADED, OFFLINE}` with `"UNKNOWN"` for an invalid cast | Retain the existing append-only four-state enum and `driverStateName()` / `toString()` helpers; align only the invalid fallback. |
| Health access | `state()`, often `driverState()`, `isOnline()`, timestamps, last error, consecutive and lifetime counters | OPT4001 already exposes `state()`, `driverState()`, `isOnline()`, `lastOkMs()`, `lastErrorMs()`, `lastError()`, `consecutiveFailures()`, `totalFailures()`, and `totalSuccess()`. No alias or rename is needed. |
| Lifecycle | `begin()`, `end()`, `probe()`, `recover()`; non-owning designs may add `bind()` / `unbind()` | Retain all existing methods. `end()` keeps its compatibility power-down attempt; `unbind()` is the explicit bus-silent release path. |
| Transport internals | `_i2cWriteReadRaw`, `_i2cWriteRaw`, `_i2cWriteReadTracked`, `_i2cWriteTracked`, `_updateHealth` | Retain these private layers. Address-override helpers use the unambiguous private suffix `Addr`; there is no public API change. |
| Dirty provenance | Explicit hardware/cache dirty flag plus error provenance where multi-write operations can partially apply | Retain `hardwareConfigDirty()`, `hardwareConfigDirtyError()`, `_markHardwareConfigDirty()`, and `_clearHardwareConfigDirty()`. |
| CLI vocabulary | `cfg`/`settings`, `drv`/`health`, `state`, `probe`, `recover`, `scan`, `init`/`begin`, `end` | Both example CLIs already expose these compatible labels and aliases; no command rename is justified. |

## Compatibility Judgment

No public type, enum, field, method, or CLI command was renamed or reordered.
The public naming already matches the useful shared conventions, and the
library-owned enum-name helpers are stronger than several peers' example-local
mappings. A breaking rename would add migration cost without improving the
contract.

The only helper renames are private `_i2cWriteRawTo()` and
`_i2cWriteTrackedTo()` to `_i2cWriteRawAddr()` and
`_i2cWriteTrackedAddr()`. They identify the address override directly and do
not affect source or binary consumers of the public headers.

The follow-up audit aligned invalid `Err` and `DriverState` name fallbacks from
the OPT-specific `"UNKNOWN_ERROR"` / `"UNKNOWN_STATE"` spellings to the shared
`"UNKNOWN"` convention used by SCD41, TCA9548A, PCA9555, and newer mature peer
helpers. Every valid enum name and numeric value is unchanged. Native coverage
now asserts every valid error/state name plus invalid casts. Private
`_recordFailure()` remains intentionally named and structured like INA3221's
semantic-failure health path. The general health documentation now explicitly
includes the `recover()` identity-mismatch case instead of incorrectly saying
that counters contain only transport outcomes. The raw/tracked/address/config/
poll helper vocabulary already matches the peer rubric, so no cosmetic
internal rename is justified.

## Proven Cleanup

- Removed `_markConversionReadyByRegisterPoll()`. A repository-wide symbol
  scan found only its declaration and definition, with no caller. The active
  freshness path requires FLAGS, INT, or sample-counter evidence and does not
  infer readiness merely from a power-down mode readback.
- Removed duplicate Arduino and native-IDF `errToStr` / `stateToStr` wrappers;
  both CLIs now call `errorName()` and `driverStateName()` directly.
- Typed the shared Arduino health snapshot with `DriverState` and included
  hardware/cache dirty state in its output instead of converting the enum to a
  locally maintained integer table.
- Removed unused verbose/snapshot/diff code from `HealthDiag.h`; the used
  periodic monitor delegates to the single shared health view.
- Removed the unused legacy `CommandHandler.h`. It had no include or caller and
  implemented a second, silently truncating line parser beside the exercised
  fixed-capacity discard-and-recovery parser.
- Removed unused style functions with no caller while retaining the help,
  prompt, yes/no, and success-rate formatting used by the Arduino CLI.
- Calculated example health success rates with a 64-bit total so saturated
  32-bit lifetime counters cannot wrap the formatting denominator.
- Removed completed implementation prompt files and an empty COM8 NOT-RUN
  report. Git history retains those records. Durable HIL procedure and current
  validation-status documents remain, and no physical test evidence is
  invented.

Static contracts reject the removed core helper names, duplicate CLI enum-name
wrappers, obsolete parser, completed prompt directory, and empty HIL report so
these parallel paths are not accidentally restored.

## Evidence Boundary

The code and documentation changes are source-level cleanup. They do not alter
the TI SBOS993A register map or the feature coverage recorded in
`feature-matrix-20260808.md`. Native fake-transport, build, package, and CI
results are recorded in `docs/validation/validation-status.md`; real-device,
optical, INT, FIFO timing/order, address-strap, and injected electrical-fault
validation remain pending.
