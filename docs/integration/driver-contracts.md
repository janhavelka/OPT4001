# Driver Contracts

This document keeps durable integration contracts in one place. Prompt-by-prompt
audit reports were removed from `docs/`; git history is the record for those
implementation passes.

## Lifecycle And Health

- `begin(const Config&)` validates configuration, probes `DEVICE_ID`, then
  applies cached configuration.
- Validation failures reset cached config/runtime to defaults.
- Probe or apply failures after a valid config leave the normalized config
  cached for diagnostics and later `probe()`, while the lifecycle remains
  `UNINIT`.
- All public tracked I2C goes through the tracked wrappers. Validation and
  precondition failures do not update health.
- Latched offline health uses `Err::OFFLINE`; active conversion or poll-job
  contention uses `Err::BUSY`.
- `recover()` is the manual path back from `OFFLINE` or dirty configuration.

## Transport Status

`probe()` uses raw transport and does not update health. Transport errors remain
specific when the adapter can distinguish them:

- `I2C_NACK_ADDR`
- `I2C_NACK_DATA`
- `I2C_TIMEOUT`
- `I2C_BUS`
- `I2C_ERROR`

A successful transport read with an unexpected fixed-pattern `DEVICE_ID` returns
`DEVICE_ID_MISMATCH`.

## Freshness And Samples

- Fresh samples require hardware evidence: `CONVERSION_READY_FLAG`, configured
  SOT-5X3 INT assertion, or output counter advance.
- `readBurst()` is the preferred low-level sample primitive for shared-I2C
  integrations. It exposes newest plus FIFO history and raw exponent, mantissa,
  ADC code, counter, CRC, and lux fields.
- `readSampleSlot(0)` keeps fresh-current-sample semantics.
- `readSampleSlot(1..3)` reads FIFO shadow slots directly and does not require
  or consume a fresh-current-sample token.
- Freshness checks skip the FLAGS clear-on-read path when configured INT
  evidence is already sufficient.
- `FLAGS` reads are clear-on-read. Any API path that reads `FLAGS` can consume
  that latched hardware view.

## Poll-Chunked Jobs

Shared-bus owners can use:

- `startReadSample()`
- `startReadBurst()`
- `startConfigureMeasurement(...)`
- `startResetAndReapply()`
- `poll(uint32_t nowMs, uint8_t maxInstructions)`
- `pollBusy()`
- `lastPollStatus()`
- `getLastBurst(BurstFrame&)`

One 16-bit register read, one 16-bit register write, or one RESULT/FIFO block
read counts as one instruction. CRC checks, lux conversion, cache updates, state
transitions, and delay gates are CPU-only and do not count. `FLAGS` reads count
because they are side-effecting register reads.

While a poll job is active, `tick()` does not perform readiness I2C and normal
tracked I2C helpers return `BUSY` for non-poll callers.

## Dirty Configuration

Multi-register configuration writes can partially reach hardware. The driver
tracks dirty hardware/cache state when a failure occurs after a partial write.
Dirty state survives unrelated reads and is cleared only after successful
`recover()` or `resetAndReapply()`.

Raw register writes can also make cached settings dirty. Use typed setters for
normal operation and read back configuration, INT configuration, and thresholds
after diagnostics that intentionally change registers.

## Numeric And CRC Policy

- Numeric helpers validate exponent, mantissa, range, and threshold fields
  before shifting.
- Lux and threshold helpers use wider intermediates where required.
- CRC verification can return `CRC_ERROR` while still preserving decoded sample
  fields.
- Burst reads decode all four slots after transfer success and preserve per-slot
  CRC fields while returning aggregate CRC status.

