# Driver Contracts

This document keeps the durable driver and application-integration contracts in
one place. Per-method contracts live as Doxygen comments in
`include/OPT4001/OPT4001.h`.

## Lifecycle And Health

- `bind(const Config&)` validates and caches configuration without touching I2C.
  `unbind()` releases the transport and runtime state without touching I2C.
- `startAttach()` plus `poll(nowMs, 1)` performs the identity read and four
  configuration writes with at most one transport callback per owner poll.
- `begin(const Config&)` validates configuration, probes `DEVICE_ID`, then
  applies cached configuration synchronously for compatibility.
- Validation failures reset cached config/runtime to defaults.
- Probe or apply failures after a valid config leave the normalized config
  cached for diagnostics and later `probe()`, while the lifecycle remains
  `UNINIT`.
- All public tracked I2C goes through the tracked wrappers. Validation and
  precondition failures do not update health.
- Latched offline health uses `Err::OFFLINE`; active conversion or poll-job
  contention uses `Err::BUSY`.
- `recover()` is the manual path back from `OFFLINE` or dirty configuration.
- `powerDown()` is the explicit error-reporting one-write shutdown. `end()`
  intentionally retains its older best-effort raw power-down attempt and then
  moves to `UNINIT`; use `unbind()` when the owner requires a bus-silent path.

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

`INVALID_CONFIG` and `INVALID_PARAM` are reserved for integration/validation
failures, including a missing adapter handle. They do not change device health.
An adapter must report bus faults with an `I2C_*` status rather than hiding them
behind a validation code. A READY health snapshot does not override an explicit
configuration error returned by an API call.

## Clock And Waits

When `Config::nowMs` is set, it is the authoritative clock for synchronous
operations, `tick()` and `poll()`; the latter two ignore their timestamp argument
for timing. Without the hook, the driver uses the last timestamp supplied to
`tick()`/`poll()`. Their first observation establishes the epoch and rebases
conversion/cache stamps made before that observation. Later synchronous calls
use the last observed caller time. Feed that clock regularly when relying on
timing gates, and pass the same clock domain to `sampleAgeMs()`; the age query
does not update the driver's clock.

Before any time source is available, synchronous nonblocking readiness helpers
can still inspect hardware evidence, but cannot enforce a time gate. Blocking
helpers require `Config::nowMs`. Their caller timeout is measured with unsigned
elapsed milliseconds. A separate guard reports `INVALID_CONFIG` if the clock
does not advance within 1,000,000 consecutive loop observations. That guard is
a broken-clock fail-safe, not a portable measure of elapsed time. The clock
must meet this progress contract; a cooperative hook that blocks for a scheduler
tick is recommended on the supported examples.

Readiness retries are separated by `max(1, conversionTimeMs / 16)` and do not
restart a conversion's lifetime. Poll read jobs have a limit of
`4 * getOneShotBudgetMs(ONE_SHOT_FORCED_AUTO) + 1000` milliseconds from their
first poll, including time spent awaiting owner scheduling. A lost one-shot
completion is also abandoned after that bound when the driver services it or
tries to start another conversion. Abandonment releases software state; it does
not cancel a physical conversion. Every individual I2C callback remains bounded
by the transport timeout, so a wait deadline can be exceeded by an in-flight
transaction and scheduling delay.

## Freshness And Samples

- Fresh samples require hardware evidence: `CONVERSION_READY_FLAG` or output
  counter advance. A polled SOT-5X3 INT level is only a hint to check the counter,
  never proof that a conversion completed.
- `readBurst()` is the preferred low-level sample primitive for shared-I2C
  integrations. It exposes newest plus FIFO history and raw exponent, mantissa,
  ADC code, counter, CRC, and lux fields.
- `readSampleSlot(0)` keeps fresh-current-sample semantics.
- `readSampleSlot(1..3)` reads FIFO shadow slots directly and does not require
  or consume a fresh-current-sample token.
- After a valid freshness baseline exists, readiness probes use the counter and
  leave FLAGS untouched. Before that baseline exists, FLAGS supplies the initial
  completion evidence after the conversion gate.
- `FLAGS` reads are clear-on-read. Any API path that reads `FLAGS` can consume
  that latched hardware view.

Typed and raw FLAGS reads preserve an observed conversion-ready bit in software;
a subsequent read of cleared FLAGS does not erase that evidence. Valid samples
with the same counter as the last accepted fresh sample are still rejected.
The counter wraps every 16 conversions, so a complete missed wrap cannot be
distinguished from no new conversion while preserving latched FLAGS. An old
ready flag can also remain set after a counter-based read. Use
`readLatestSample()` when freshness cannot be established, and sample faster
than a full counter wrap when every conversion matters.

## Poll-Chunked Jobs

Shared-bus owners can use:

- `bind()` / `startAttach()`
- `startReadSample()`
- `startReadBurst()`
- `startConfigureMeasurement(...)`
- `startResetAndReapply()`
- `poll(uint32_t nowMs, uint8_t maxInstructions)`
- `pollBusy()`
- `lastPollStatus()`
- `getLastBurst(BurstFrame&)`
- `cancelPollJob()`

One 16-bit register read, one 16-bit register write, or one RESULT/FIFO block
read counts as one instruction. CRC checks, lux conversion, cache updates, state
transitions, and delay gates are CPU-only and do not count. `FLAGS` reads count
because they are side-effecting register reads.

Sole-owner integrations should keep the default budget of one callback per
owner poll. Larger budgets are bounded diagnostic batching and intentionally
allow multiple callbacks in one `poll()` call.

While a poll job is active, `tick()` does not perform readiness I2C and normal
tracked I2C helpers return `BUSY` for non-poll callers.

Cancellation is bus-silent. If confirmed configuration or reset writes may
have reached hardware, cancellation marks `hardwareConfigDirty`; a cancelled
reset that reached the general-call write also leaves lifecycle `UNINIT`.

Starting a read job requires continuous mode, a pending one-shot, or already
captured readiness, and an observed enabled hardware burst bit. Inactive
power-down returns `MEASUREMENT_NOT_READY` without starting a job. An elapsed
read-job deadline returns `TIMEOUT` and releases `pollBusy()`.

## Burst Framing And Consistency

The framing decision follows the observed `I2C_BURST` bit from successful
INT-configuration writes/readbacks, including raw access, rather than merely
the requested `Config::burstMode`. An uncertain write invalidates the burst
assumption; synchronous reads then use individual register transactions and
poll block jobs require readback or reapplication before they can run.

An unobserved external writer can change any device configuration. The
application must serialize register ownership and call `readIntConfiguration()`
or `recover()` after such changes. A reset enables burst by default but also
resets measurement settings; use recovery to restore the intended configuration.
The driver does not add a redundant configuration read to every data transfer
or claim to exclude an asynchronous power fault during a transfer.

With burst disabled, `readBurst()` assembles eight register reads and checks the
newest counter once more. A detected shift returns `MEASUREMENT_NOT_READY` and
does not publish that frame as the cached burst. This bounded check cannot prove
an atomic snapshot: a full modulo-16 wrap or a torn register pair can escape it,
and a CRC check is not collision-free. Keep CRC verification enabled and choose
bus speed/conversion cadence appropriate to the transfer duration. Inspect each
slot's `crcValid` when a CRC warning is returned.

## Dirty Configuration

Multi-register configuration writes can partially reach hardware. The driver
tracks dirty hardware/cache state when a failure occurs after a partial write.
Dirty state survives unrelated reads and bus-silent rebinding. Successful full
configuration application through begin/attach/recovery clears it; `unbind()`
explicitly releases the diagnostic state along with the binding.

`getSettings(SettingsSnapshot&)` is cache-only and must not perform I2C. The
snapshot exposes `hardwareConfigDirty` and `hardwareConfigDirtyError`, matching
the direct diagnostics `hardwareConfigDirty()` and
`hardwareConfigDirtyError()`, so callers can see the root status that first made
configuration uncertain. `hardwareConfigDirtyError == OK` means an intentional
successful raw register write made the hardware/cache relationship uncertain.

Raw register writes can also make cached settings dirty. Use typed setters for
normal operation and read back configuration, INT configuration, and thresholds
after diagnostics that intentionally change registers.

## Numeric And CRC Policy

- Numeric helpers validate exponent, mantissa, range, and threshold fields
  before shifting.
- Lux and threshold helpers use wider intermediates where required.
- Full-scale helpers return rounded nominal datasheet values, not exact numeric
  saturation bounds. Convert the maximum mantissa/exponent when an exact bound
  is needed. Invalid conversion-time helpers return zero as a sentinel; public
  configuration APIs reject invalid enums before applying them.
- CRC verification can return `CRC_ERROR` while still preserving decoded sample
  fields.
- Burst reads decode all four slots after transfer success and preserve per-slot
  CRC fields while returning aggregate CRC status.
- CRC-invalid samples remain data-bearing but do not change the last trusted
  freshness counter, including when CRC error reporting is disabled. A later
  valid read may deliver the same conversion again.
