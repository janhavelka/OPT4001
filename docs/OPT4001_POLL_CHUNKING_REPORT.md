# OPT4001 Poll-Chunked I2C Execution Report

Date: 2026-06-16

## Scope

This pass added an explicit poll-chunked execution path for shared I2C owners
that need to limit how much bus work one driver call can perform. It does not
replace the existing synchronous APIs.

The companion audit prompt owns status taxonomy, packaging, and general
freshness semantics. This report covers execution sequencing, instruction
accounting, delay gates, and budget tests only.

## Public API Added

- `poll(uint32_t nowMs, uint8_t maxInstructions)`
- `pollBusy()`
- `lastPollStatus()`
- `startReadSample()`
- `startReadBurst()`
- `getLastBurst(BurstFrame&)`
- `startConfigureMeasurement(...)`
- `startResetAndReapply()`

## Instruction Accounting

One 16-bit register read, one 16-bit register write, or one RESULT/FIFO burst
block read counts as one instruction. CRC checks, lux conversion, cache updates,
state transitions, and delay gates are CPU-only and do not count. `FLAGS` reads
are explicit instructions because the register is clear-on-read.

## Sequencing

Sample jobs wait for the configured conversion gate, then collect hardware
freshness evidence through `FLAGS`, INT, or sample-counter advance. `FLAGS`
reads are explicit side-effecting instructions. After
freshness is available, burst-mode jobs read the RESULT/FIFO block in one I2C
`writeRead` and decode all four slots. `startReadBurst()` caches the full burst
frame and `BurstFrame::newest` as the last sample. `startReadSample()` uses the
same one-instruction burst primitive but publishes only the newest sample cache.
`readBurst()` remains the preferred low-level shared-task primitive.
`tryReadSample()` remains a synchronous compatibility/diagnostic helper rather
than an instruction-budgeted poll job.

While a poll job is active, `tick()` does not perform readiness I2C and normal
tracked I2C helpers return `BUSY` for non-poll callers. This keeps the active
poll job's instruction budget authoritative.

Configuration jobs stage the requested measurement settings and apply the same
four-register sequence as `_applyConfig()`: threshold low, threshold high, INT
configuration, and measurement configuration. The job stops on the first failed
write and preserves existing dirty-state behavior.

Reset/reapply jobs perform one general-call reset instruction followed by the
same four staged configuration writes. A config failure after reset marks dirty
state and leaves the driver uninitialized, matching the synchronous
`resetAndReapply()` policy.

## Tests Added

- Status-plus-burst completes in one `poll(..., 2)` call.
- `startReadSample()` uses the burst primitive without publishing a cached
  `BurstFrame`.
- A one-instruction budget splits FLAGS and burst reads across calls.
- Zero-instruction polls perform no I2C and keep the job active.
- Delay gates consume no I2C instructions before the conversion interval.
- Active poll jobs block `tick()` and synchronous tracked I2C from consuming
  budget outside `poll()`.
- FIFO-full burst reads wait for the four-sample cadence before touching I2C.
- Forced-auto one-shot burst reads wait for the full one-shot budget, including
  standby wake and forced auto-range margin, before touching I2C.
- Chunked config apply updates cache only after all four writes complete.
- Chunked config apply stops on failure, reports the original status, rolls back
  cached settings, and marks dirty after partial hardware writes.
- Chunked reset/reapply stops after reset-following config failure, reports the
  original status, clears runtime sample/burst state, marks dirty, and returns
  the driver to `UNINIT`.

## Validation

- `python -m platformio test -e native`: passed, 109 tests.

## Subagent Notes

Explorer subagents were spawned before implementation as requested. The
conversion/burst/config explorer flagged the need to block `tick()` and clear
runtime state after a chunked reset. The transaction-budget explorer confirmed
the one-transfer instruction model and recommended explicit coverage for
`startReadSample()`, zero budgets, and interleaving guards. Those findings were
folded into the implementation and tests.
