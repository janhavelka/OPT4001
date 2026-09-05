/// @file OPT4001.h
/// @brief Main driver class for OPT4001.
#pragma once

#include <cstddef>
#include <cstdint>

#include "OPT4001/CommandTable.h"
#include "OPT4001/Config.h"
#include "OPT4001/Status.h"
#include "OPT4001/Version.h"

namespace OPT4001 {

/// Driver state for health monitoring.
enum class DriverState : uint8_t {
  UNINIT,   ///< begin() has not succeeded or end() was called.
  READY,    ///< Driver is initialized and has no consecutive I2C failures.
  DEGRADED, ///< Driver is initialized with failures below the offline threshold.
  OFFLINE   ///< Consecutive I2C failures reached the configured threshold.
};

/// Return a stable library-owned name for a driver state.
/// @param state Driver state to describe.
/// @return Static storage; invalid enum values return `"UNKNOWN"`.
constexpr const char* driverStateName(DriverState state) {
  switch (state) {
    case DriverState::UNINIT: return "UNINIT";
    case DriverState::READY: return "READY";
    case DriverState::DEGRADED: return "DEGRADED";
    case DriverState::OFFLINE: return "OFFLINE";
  }
  return "UNKNOWN";
}

/// Cross-library alias for `driverStateName()`.
/// @param state Driver state to describe.
/// @return Static storage; invalid enum values return `"UNKNOWN"`.
constexpr const char* toString(DriverState state) { return driverStateName(state); }

/// Decoded measurement sample from RESULT/FIFO registers.
struct Sample {
  uint16_t resultReg = 0;
  uint16_t resultLsbCrcReg = 0;
  /// Result exponent from `RESULT[15:12]`. Valid decoded samples use 0..8.
  uint8_t exponent = 0;
  /// 20-bit result mantissa assembled from `RESULT[11:0]` and `RESULT_LSB[15:8]`.
  uint32_t mantissa = 0;
  /// Linear ADC codes (`mantissa << exponent`) after bounds validation.
  uint32_t adcCodes = 0;
  /// 4-bit hardware sample counter; wraps modulo 16. A changed counter is
  /// freshness evidence, while a duplicate counter means duplicate data or an
  /// unobserved full wrap.
  uint8_t counter = 0;
  /// Received 4-bit CRC nibble from `RESULT_LSB_CRC[3:0]`.
  uint8_t crc = 0;
  /// True when the received CRC matches the datasheet XOR equations.
  bool crcValid = false;
  /// Lux scaled by package variant from validated ADC codes.
  float lux = 0.0f;
};

/// Four-deep burst frame ordered newest to oldest within the history window.
///
/// `newest` maps to RESULT registers 0x00/0x01. `fifo0`, `fifo1`, and `fifo2`
/// map to FIFO shadow pairs 0x02/0x03, 0x04/0x05, and 0x06/0x07. Each `Sample`
/// carries its own received CRC nibble and `crcValid` flag; burst APIs return an
/// aggregate status for the frame.
struct BurstFrame {
  Sample newest;
  Sample fifo0;
  Sample fifo1;
  Sample fifo2;
};

/// Decoded FLAGS register.
struct Flags {
  uint16_t raw = 0;
  bool overload = false;
  bool conversionReady = false;
  bool highThreshold = false;
  bool lowThreshold = false;
};

/// Decoded device identification register.
struct DeviceIdInfo {
  /// Raw register 0x11 value.
  uint16_t raw = 0;
  /// DIDH bits [11:0], expected `cmd::DIDH_EXPECTED`.
  uint16_t didh = 0;
  /// DIDL bits [13:12], expected `cmd::DIDL_EXPECTED`.
  uint8_t didl = 0;
  /// True when fixed/reserved bits [15:14] are clear.
  bool reservedBitsClear = true;
  /// True only when fixed bits, DIDL, and DIDH all match the OPT4001 pattern.
  bool matchesExpected = false;
};

/// Decoded CONFIGURATION register fields.
struct ConfigurationInfo {
  uint16_t raw = 0;
  bool quickWake = false;
  bool reservedBitSet = false;
  Range range = Range::AUTO;
  ConversionTime conversionTime = ConversionTime::MS_100;
  Mode mode = Mode::POWER_DOWN;
  InterruptLatch interruptLatch = InterruptLatch::LATCHED;
  InterruptPolarity interruptPolarity = InterruptPolarity::ACTIVE_LOW;
  FaultCount faultCount = FaultCount::FAULTS_1;
  bool valid = true;
};

/// Decoded INT_CONFIGURATION register fields.
struct IntConfigurationInfo {
  uint16_t raw = 0;
  bool fixedPatternValid = false;
  bool reservedBitSet = false;
  IntDirection intDirection = IntDirection::PIN_OUTPUT;
  IntConfig intConfig = IntConfig::THRESHOLD;
  bool burstMode = true;
  bool valid = true;
};

/// Snapshot of cached configuration and runtime state without I2C access.
///
/// This is the driver's cache, not a live hardware readback. After raw register
/// writes, external resets, brownout, or partial multi-register failures, treat
/// the hardware/cache relation as dirty until `recover()` or `resetAndReapply()`
/// re-applies configuration and any required registers are read back.
struct SettingsSnapshot {
  bool initialized = false;
  DriverState state = DriverState::UNINIT;
  PackageVariant packageVariant = PackageVariant::SOT_5X3;
  uint8_t i2cAddress = cmd::I2C_ADDR_DEFAULT;
  uint32_t i2cTimeoutMs = 0;
  uint8_t offlineThreshold = 0;
  bool verifyCrc = true;
  bool hasNowMsHook = false;
  bool hasGpioReadHook = false;
  bool hasCooperativeYieldHook = false;
  int intPin = -1;
  bool quickWake = false;
  Range range = Range::AUTO;
  ConversionTime conversionTime = ConversionTime::MS_100;
  Mode mode = Mode::POWER_DOWN;
  Mode pendingMode = Mode::POWER_DOWN;
  InterruptLatch interruptLatch = InterruptLatch::LATCHED;
  InterruptPolarity interruptPolarity = InterruptPolarity::ACTIVE_LOW;
  FaultCount faultCount = FaultCount::FAULTS_1;
  IntDirection intDirection = IntDirection::PIN_OUTPUT;
  IntConfig intConfig = IntConfig::THRESHOLD;
  bool burstMode = true;
  Threshold lowThreshold{};
  Threshold highThreshold{0x0B, 0x0FFF};
  bool sampleAvailable = false;
  bool hasSample = false;
  bool lastSampleValid = false;
  bool conversionStarted = false;
  bool conversionReady = false;
  uint32_t conversionStartMs = 0;
  uint32_t sampleTimestampMs = 0;
  uint8_t lastCounter = 0;
  uint32_t lastAdcCodes = 0;
  float lastLux = 0.0f;
  /// True when a multi-register hardware update may have partially applied and
  /// hardware may not match the cached configuration. Clear with successful
  /// full config re-apply through recover(), resetAndReapply(), or a full
  /// configuration write path.
  bool hardwareConfigDirty = false;
  /// Original status that first marked `hardwareConfigDirty`; `OK` means an
  /// intentional successful raw register write made the cache relationship
  /// uncertain.
  Status hardwareConfigDirtyError = Status::Ok();
  /// A validated callback/configuration set is cached. Appended to preserve the
  /// positional meaning of existing aggregate initializers.
  bool bound = false;
};

/// OPT4001 driver class.
///
/// Public methods are not internally synchronized and are not ISR-safe. Use one
/// task/context per instance or protect the instance and the application-owned
/// I2C transport with external serialization. Transport callbacks must not
/// re-enter the same driver instance.
///
/// Public methods that perform normal tracked I2C require a completed `begin()`
/// or `startAttach()` unless explicitly documented otherwise. They return
/// `Err::NOT_INITIALIZED` without
/// touching the bus when the driver is `UNINIT`. When health state is `OFFLINE`,
/// normal public I2C APIs return `Err::OFFLINE` without bus access. The explicit
/// exceptions that still touch the bus while `OFFLINE` are `probe()`,
/// `recover()`, `softReset()`, `resetAndReapply()`, `startResetAndReapply()`,
/// and `poll()` while it drives that reset job.
class OPT4001 {
public:
  OPT4001() = default;
  OPT4001(const OPT4001&) = delete;
  OPT4001& operator=(const OPT4001&) = delete;
  OPT4001(OPT4001&&) = delete;
  OPT4001& operator=(OPT4001&&) = delete;

  // === Lifecycle ===
  /// Validate and cache transport/configuration without touching I2C. This is
  /// the preferred first step for an application-owned single-transfer poller.
  /// A successful bind leaves the driver uninitialized until `startAttach()`
  /// completes (or legacy synchronous `begin()` is used).
  /// Every call resets the previous binding/runtime, including on validation
  /// failure, which leaves default config and no transport callbacks. Existing
  /// dirty-hardware evidence persists until full reapply or unbind(). Passing
  /// getConfig() from this instance is supported; the input is copied first.
  Status bind(const Config& config);
  /// Release the cached transport/configuration and all runtime state without
  /// touching I2C. Unlike `end()`, this never attempts to power down hardware.
  void unbind();
  bool isBound() const { return _bound; }
  /// Initialize the driver, verify Device ID, and apply the supplied config.
  /// Config application writes multiple registers. If a later write fails after
  /// earlier writes reached hardware, `hardwareConfigDirty()` may be set even
  /// though the driver remains `UNINIT`; retry with a caller-owned corrected
  /// config after the bus is healthy. Validation failure resets the previous
  /// binding as documented by bind(). Normal public I2C APIs stay
  /// guarded and return `NOT_INITIALIZED` until `begin()` succeeds.
  Status begin(const Config& config);
  /// Advance conversion timing/poll state. Elapsed time is only a poll gate;
  /// readiness is reported only after hardware evidence is observed. After the
  /// timing gate, `tick()` may perform tracked I2C readiness polling, which can
  /// update health and latch `OFFLINE`; poll errors are retained in health state
  /// rather than returned because `tick()` is void. No I2C is attempted while
  /// `UNINIT` or `OFFLINE`.
  /// Config::nowMs is authoritative when configured. Otherwise tick()/poll()
  /// feed one caller clock: initial conversion/sample timestamps are rebased
  /// on its first observation. Supply current time before synchronous work and
  /// keep this clock progressing; until first observed, synchronous readiness
  /// relies on hardware without a time gate. Rebase is conservative and cannot
  /// reconstruct the actual acquisition time of an earlier cached sample.
  void tick(uint32_t nowMs);
  /// Best-effort shutdown. Attempts a raw power-down write only when initialized
  /// and online, ignores that write status, then clears runtime state and moves
  /// the driver to `UNINIT`. The validated transport/configuration remains bound
  /// for a later `startAttach()`; call `unbind()` for a bus-silent full release.
  void end();

  /// Start a five-instruction attach job: one DEVICE_ID read followed by four
  /// configuration writes. `poll(nowMs)` defaults to at most one transport
  /// callback, allowing a sole bus-owner task to budget attachment precisely.
  /// Call `bind()` first. Completion moves the driver to READY.
  Status startAttach();

  /// Explicit, error-reporting power down. Performs one tracked CONFIGURATION
  /// write and updates the cached mode/runtime state only after success.
  Status powerDown();

  bool isInitialized() const { return _initialized; }
  const Config& getConfig() const { return _config; }

  // === Diagnostics (probe uses raw transport, recover uses tracked transport) ===
  /// Probe the currently cached transport/address using raw I2C without
  /// requiring a successful lifecycle and without updating health, runtime
  /// state, or driver state. This can touch I2C even while `OFFLINE`. Requires
  /// configured I2C callbacks in the cached config. A successful probe means the
  /// DEVICE_ID register pattern is exactly the expected OPT4001 value; it is not
  /// optical or measurement-path validation. Transport statuses are returned
  /// without being collapsed to `DEVICE_NOT_FOUND`; a successful I2C read with
  /// an unexpected ID returns `DEVICE_ID_MISMATCH`.
  Status probe();
  /// Attempt recovery using tracked Device ID readback and config re-apply.
  /// Transport failures and Device ID mismatches update health counters. Use
  /// this after OFFLINE state or suspected dirty hardware/cache state from raw
  /// writes, partial multi-register application, brownout, or external reset.
  /// Success re-applies cached configuration, clears dirty config state, and
  /// returns to `READY`. Failure remains health-tracked; an originally `OFFLINE`
  /// driver remains latched offline unless recovery fully succeeds.
  Status recover();
  /// Perform the documented general-call reset (address 0x00, data 0x06).
  /// This is bus-wide, can touch I2C while `OFFLINE`, and leaves the driver in
  /// `UNINIT` state on success.
  Status softReset();
  /// General-call reset followed by re-applying the cached configuration. This
  /// can touch I2C while `OFFLINE`.
  Status resetAndReapply();
  Status readDeviceId(uint16_t& value);
  Status readDeviceId(DeviceIdInfo& out);
  Status getSettings(SettingsSnapshot& out) const;
  /// True when a multi-register config operation may have partially changed
  /// hardware before failing. The flag persists across ordinary reads and
  /// clears only after a successful full config re-apply.
  bool hardwareConfigDirty() const { return _hardwareConfigDirty; }
  /// First status that marked `hardwareConfigDirty()`; `OK` means an intentional
  /// successful raw register write made the cache relationship uncertain.
  Status hardwareConfigDirtyError() const { return _hardwareConfigDirtyError; }

  // === Driver State ===
  DriverState state() const { return _driverState; }
  /// Alias for state() used by shared diagnostics.
  DriverState driverState() const { return state(); }
  bool isOnline() const {
    return _driverState == DriverState::READY ||
           _driverState == DriverState::DEGRADED;
  }

  // === Health Tracking ===
  /// Health tracking counts tracked transport outcomes after initialization and
  /// an explicit device-identity mismatch observed by `recover()`. Validation
  /// errors, precondition failures, and diagnostic `probe()` outcomes do not
  /// count. A tracked success resets consecutive failures and moves
  /// `DEGRADED`/`OFFLINE` back to `READY`. Lifetime counters saturate at
  /// `UINT32_MAX` and `consecutiveFailures()` saturates at `UINT8_MAX`;
  /// timestamps use `Config::nowMs` when available, otherwise the last
  /// tick()/poll() timestamp (0 before the first observation).
  uint32_t lastOkMs() const { return _lastOkMs; }
  uint32_t lastErrorMs() const { return _lastErrorMs; }
  Status lastError() const { return _lastError; }
  uint8_t consecutiveFailures() const { return _consecutiveFailures; }
  uint32_t totalFailures() const { return _totalFailures; }
  uint32_t totalSuccess() const { return _totalSuccess; }

  // === Measurement API ===
  /// Trigger a one-shot. An unresolved earlier one-shot can be retriggered after
  /// 4 * getOneShotBudgetMs(ONE_SHOT_FORCED_AUTO) + 1000 ms since its start.
  /// This abandons driver wait state, not an in-flight hardware conversion.
  Status startConversion();
  Status startConversion(Mode mode);
  /// Run the active poll-chunked job for at most `maxInstructions` I2C
  /// instructions. One register read/write or one burst RESULT/FIFO block read
  /// counts as one instruction; CPU-only CRC/lux decode and delay gates do not.
  /// Returns `IN_PROGRESS` while the job is waiting, out of budget, or not yet
  /// ready. Use `pollBusy()` to distinguish an active job from idle state.
  /// Clock selection follows tick(). Read jobs return TIMEOUT after
  /// 4 * getOneShotBudgetMs(ONE_SHOT_FORCED_AUTO) + 1000 ms from first poll().
  /// Failed readiness probes retry at max(1, conversionTimeMs / 16) ms.
  Status poll(uint32_t nowMs, uint8_t maxInstructions = 1);
  /// @return true while a poll-chunked sample/config/reset job is active.
  bool pollBusy() const;
  /// Last status produced by `poll()` or a `start*()` poll job method.
  Status lastPollStatus() const { return _lastPollStatus; }
  /// Start a poll-chunked fresh newest-sample read. The job uses the burst
  /// RESULT/FIFO block path and caches `newest`; retrieve it with
  /// `getLastSample()`. Requires observed hardware burst mode. POWER_DOWN with
  /// no pending conversion or captured readiness returns MEASUREMENT_NOT_READY.
  Status startReadSample();
  /// Start a poll-chunked fresh RESULT/FIFO burst read. Requires
  /// observed hardware burst mode and an active conversion/captured readiness;
  /// retrieve the completed frame with `getLastBurst()`.
  Status startReadBurst();
  /// Return the most recent synchronous readBurst() or poll burst frame.
  /// Starting either poll read invalidates it; sample-only jobs do not refill it.
  Status getLastBurst(BurstFrame& out) const;
  /// Start a poll-chunked coherent measurement configuration write. The job
  /// applies the same four-register sequence as the synchronous configuration
  /// path but stops when `maxInstructions` is exhausted or a write fails.
  Status startConfigureMeasurement(Range range, ConversionTime time, Mode mode,
                                   bool quickWake = false);
  /// Start a poll-chunked general-call reset plus cached-configuration reapply.
  /// The reset remains bus-wide; each reset/apply write is one instruction.
  Status startResetAndReapply();
  /// Cancel an active poll-chunked job without touching I2C. Cancellation is
  /// idempotent when idle. If confirmed configuration/reset writes may have
  /// partially applied, the cache/hardware relationship is marked dirty.
  Status cancelPollJob();
  /// Status-returning fresh-readiness check.
  /// Before a valid counter baseline exists, this reads clear-on-read FLAGS
  /// after the conversion gate, consuming latched threshold flags. Thereafter
  /// counter probes preserve FLAGS. Polled INT is a hint for a counter probe,
  /// never proof of freshness; its brief pulses can be missed.
  /// @param[out] ready Set true when a fresh sample can be read
  /// @return Status::Ok() with ready=false when no sample is ready; transport errors are returned
  Status conversionReady(bool& ready);
  /// Convenience readiness check. Transport/poll errors are collapsed to false.
  bool conversionReady();
  /// Read the current RESULT registers without proving freshness. This returns
  /// the latest register contents and may return the same sample repeatedly.
  /// Callers that need newly completed conversions should use readSample(),
  /// tryReadFreshSample(), or readFreshBlocking().
  Status readLatestSample(Sample& out);
  /// Read a fresh sample. Freshness requires hardware evidence: ready flag,
  /// or a changed sample counter after a previous
  /// fresh sample. Returns `MEASUREMENT_NOT_READY` when no fresh evidence exists.
  /// On `OK` or `CRC_ERROR`, readiness is cleared and the cache updates. Only a
  /// CRC-valid counter updates the freshness baseline, even with CRC reporting
  /// disabled; the same conversion may be returned again after a CRC warning.
  /// Equal valid counters are conservatively rejected, even with a ready flag:
  /// a latched flag can describe an already consumed counter-based sample.
  /// Exactly 16 (or a multiple of 16) conversions between reads can alias;
  /// readLatestSample() is available when duplicate acceptance is intentional.
  /// Resolving the initial ready flag consumes clear-on-read threshold flags.
  Status readSample(Sample& out);
  /// Read newest RESULT plus FIFO0/FIFO1/FIFO2. Slot order is newest/current
  /// output register, then prior FIFO shadows in hardware order. Every slot is
  /// decoded; if any slot fails CRC, all decoded fields remain populated and the
  /// aggregate status is `CRC_ERROR`.
  /// Read a fresh four-sample history frame ordered newest to oldest.
  ///
  /// The returned status is aggregate. `OK` means all decoded slots satisfied
  /// the selected CRC policy. `CRC_ERROR` means at least one decoded slot failed
  /// CRC verification; inspect each slot's `crcValid`/`crc` fields. After a
  /// successful register transfer, all four slots are decoded and populated.
  /// `crcValid` is always computed, whatever `Config::verifyCrc` is set to; when
  /// verification is disabled, a CRC mismatch simply does not change the
  /// aggregate status.
  /// Framing follows the last successful INT_CONFIGURATION write/readback,
  /// with uncertain writes/reset attempts disabling burst framing until proven
  /// again. External register changes require readback or recover(); the driver
  /// does not monitor out-of-band writes or brownouts continuously.
  /// Without burst framing, slots span separate transactions. A final counter
  /// reread rejects detected FIFO movement with MEASUREMENT_NOT_READY. This is
  /// not an atomic snapshot: counter wrap, guard corruption, and a torn newest
  /// pair remain possible; pair CRC checking should remain enabled.
  Status readBurst(BurstFrame& out);
  /// Read one slot from the 4-deep history window: 0 = newest RESULT, 1 = FIFO0,
  /// 2 = FIFO1, 3 = FIFO2. Slot 0 consumes freshness; slots 1-3 are direct FIFO
  /// shadow reads and carry independent `crcValid` state.
  Status readSampleSlot(uint8_t slot, Sample& out);
  Status getLastSample(Sample& out) const;
  /// True after at least one sample has been cached. Cached samples are
  /// convenience data and are not proof of freshness.
  bool hasSample() const { return _lastSampleValid; }
  uint32_t sampleTimestampMs() const;
  /// Age of the cached sample in milliseconds.
  /// @param nowMs Current timestamp in the driver's selected clock domain.
  /// This query never updates the driver clock.
  /// @return `nowMs - sampleTimestampMs()` when a sample exists, otherwise 0
  uint32_t sampleAgeMs(uint32_t nowMs) const;
  /// Fresh lux read using `readSample()` semantics. On `CRC_ERROR`, the decoded
  /// output is still written and freshness is consumed.
  Status readLux(float& lux);
  /// Fresh milli-lux read using `readSample()` semantics. On `CRC_ERROR`, the
  /// decoded output is still written and freshness is consumed.
  Status readMilliLux(uint32_t& milliLux);
  /// Fresh micro-lux read using `readSample()` semantics. On `CRC_ERROR`, the
  /// decoded output is still written and freshness is consumed.
  Status readMicroLux(uint64_t& microLux);
  /// Blocking read helpers require `Config::nowMs` and poll until a fresh sample,
  /// timeout, transport error, or stalled-clock diagnostic. Each I2C transaction
  /// is bounded by `Config::i2cTimeoutMs`; `Config::cooperativeYield`, when
  /// configured, is called between polls. On `CRC_ERROR`, sample/lux output is
  /// still populated. A timeout does not cancel an in-flight hardware one-shot;
  /// later readiness polling may still consume that conversion.
  /// @pre `Config::nowMs` must be configured, monotonic, and non-blocking.
  /// It must advance within 1,000,000 consecutive unchanged-clock loop
  /// iterations, otherwise INVALID_CONFIG is returned. This fail-safe cannot
  /// distinguish a stopped clock from a slower-than-contracted clock.
  Status readBlocking(Sample& out, uint32_t timeoutMs = 1000);
  Status readBlocking(Sample& out, Mode mode, uint32_t timeoutMs);
  Status readFreshBlocking(Sample& out, uint32_t timeoutMs = 1000);
  Status readFreshBlocking(Sample& out, Mode mode, uint32_t timeoutMs);
  Status readBlockingLux(float& lux, uint32_t timeoutMs = 1000);
  Status readBlockingLux(float& lux, Mode mode, uint32_t timeoutMs);
  /// Poll-friendly fresh helper: returns OK with didRead=false if no fresh sample
  /// is ready yet. If a sample was read successfully, or read with CRC warning,
  /// didRead is set true. Compatibility helper: this is a synchronous
  /// diagnostic/convenience path, not an instruction-budgeted `poll()` job.
  Status tryReadSample(Sample& out, bool& didRead);
  Status tryReadFreshSample(Sample& out, bool& didRead);
  /// Poll-friendly helper returning lux directly without treating "not ready" as an error.
  /// If a sample was read successfully, or read with CRC warning, didRead is set true.
  Status tryReadLux(float& lux, bool& didRead);

  // === Flags / Status ===
  /// Read FLAGS register. Register 0x0C is clear-on-read; this clears the
  /// device's latched FLAGS view, including conversion-ready and threshold flags.
  /// The driver captures conversion-ready evidence before the hardware clear.
  /// Raw reads of 0x0C or raw blocks spanning 0x0C have the same hardware side
  /// effect.
  Status readFlags(Flags& out);
  /// Raw FLAGS read with the same clear-on-read hardware side effect; ready
  /// evidence is captured set-only, just as with typed readFlags().
  Status readFlagsRaw(uint16_t& value);
  /// Clear CONVERSION_READY_FLAG only by writing a non-zero value to 0x0C; this
  /// clears driver readiness evidence for the pending conversion.
  Status clearConversionReadyFlag();
  /// Clear all sticky status indications using the documented clear-on-read
  /// behavior; this clears driver readiness evidence for the latched view.
  Status clearFlags();
  /// Read the configured INT GPIO hook and report assertion using INT_POL.
  /// @param[out] asserted True when the configured pin is active
  /// @return Status::Ok() on success, INVALID_CONFIG when no INT hook is available.
  /// Returns NOT_INITIALIZED until initialization has applied the INT polarity.
  /// INT is an open-drain SOT_5X3-only signal. The application owns pullups,
  /// GPIO setup, ISR attachment, ISR-to-task signaling, debouncing, and pin
  /// lifetime; the driver only samples the configured `gpioRead` hook.
  Status readIntPinAsserted(bool& asserted) const;

  // === Configuration ===
  /// Select the package-specific address and lux scaling profile.
  /// @param variant Package variant to select.
  /// @return `INVALID_PARAM` for an invalid variant/address combination, or
  /// `INVALID_CONFIG` when selecting PicoStar while an INT hook is configured;
  /// returns `BUSY` while a staged poll job owns decoding/configuration state.
  Status setPackageVariant(PackageVariant variant);
  PackageVariant getPackageVariant() const { return _config.packageVariant; }

  Status setRange(Range range);
  Range getRange() const { return _config.range; }

  Status setConversionTime(ConversionTime time);
  ConversionTime getConversionTime() const { return _config.conversionTime; }

  /// Set stable operating mode. One-shot modes are triggered via startConversion().
  Status setMode(Mode mode);
  Mode getMode() const { return _config.mode; }

  Status setQuickWake(bool enable);
  bool getQuickWake() const { return _config.quickWake; }

  /// Set host-side CRC verification policy. Returns `BUSY` during a poll job.
  Status setVerifyCrc(bool enable);
  bool getVerifyCrc() const { return _config.verifyCrc; }

  Status setInterruptLatch(InterruptLatch latch);
  InterruptLatch getInterruptLatch() const { return _config.interruptLatch; }

  Status setInterruptPolarity(InterruptPolarity polarity);
  InterruptPolarity getInterruptPolarity() const { return _config.interruptPolarity; }

  Status setFaultCount(FaultCount count);
  FaultCount getFaultCount() const { return _config.faultCount; }

  Status setIntDirection(IntDirection direction);
  IntDirection getIntDirection() const { return _config.intDirection; }

  Status setIntConfig(IntConfig config);
  IntConfig getIntConfig() const { return _config.intConfig; }

  Status setBurstMode(bool enable);
  bool getBurstMode() const { return _config.burstMode; }

  /// Write raw threshold registers after field-range validation. This low-level
  /// setter does not enforce `low <= high`; interrupt convenience helpers do.
  /// If the low write succeeds and the high write fails, hardware dirty state is
  /// marked because cache and hardware may diverge.
  Status setThresholds(const Threshold& low, const Threshold& high);
  /// Read threshold registers from hardware and refresh the cached thresholds.
  Status getThresholds(Threshold& low, Threshold& high);
  Status getThresholdsLux(float& lowLux, float& highLux);
  Status setThresholdsLux(float lowLux, float highLux);
  /// Apply range / conversion-time / stable-mode / quick-wake together in one coherent update.
  Status configureMeasurement(Range range, ConversionTime time, Mode mode, bool quickWake = false);
  /// Restore threshold registers to their documented reset defaults.
  Status restoreDefaultThresholds();
  /// Convenience helper for common SOT_5X3 output-mode threshold interrupt use.
  /// Configures INT as open-drain output with `INT_CFG=THRESHOLD`; hardware
  /// threshold/SMBus-alert behavior still needs target-board validation.
  /// Returns `INVALID_CONFIG` for PicoStar, which has no INT pin.
  Status enableThresholdInterrupt(const Threshold& low, const Threshold& high);
  /// Convenience helper for common output-mode threshold interrupt use with lux values.
  Status enableThresholdInterruptLux(float lowLux, float highLux);
  /// Configure SOT_5X3 INT as an open-drain output pulse after every conversion.
  /// The application owns pullup, GPIO/ISR setup, and hardware validation.
  /// Returns `INVALID_CONFIG` for PicoStar, which has no INT pin.
  Status enableConversionReadyInterrupt();
  /// Configure SOT_5X3 INT as an open-drain output pulse when the 4-deep FIFO
  /// window is full. The application owns pullup, GPIO/ISR setup, and hardware
  /// validation. Returns `INVALID_CONFIG` for PicoStar.
  Status enableFifoFullInterrupt();

  Status readConfiguration(uint16_t& value);
  Status readConfiguration(ConfigurationInfo& out);
  Status writeConfiguration(uint16_t value);
  Status readIntConfiguration(uint16_t& value);
  Status readIntConfiguration(IntConfigurationInfo& out);
  Status writeIntConfiguration(uint16_t value);
  Status readConfig(uint16_t& value) { return readConfiguration(value); }
  Status writeConfig(uint16_t value) { return writeConfiguration(value); }
  Status readIntConfig(uint16_t& value) { return readIntConfiguration(value); }
  Status writeIntConfig(uint16_t value) { return writeIntConfiguration(value); }

  // === Raw Register Access ===
  /// Read a contiguous byte window from public 16-bit registers.
  /// Requires `begin()`. Blocks spanning reserved register gaps are rejected
  /// before I2C. Returns `OFFLINE` without I2C while `OFFLINE`. Blocks including
  /// FLAGS register 0x0C clear that register's latched view. With burst mode
  /// disabled, one bounded two-byte transaction is used per register instead
  /// of relying on hardware pointer auto-increment.
  /// Full FLAGS words capture readiness set-only. An odd burst read ending at
  /// the FLAGS high byte cannot capture the unread low-byte ready bit. Full
  /// INT_CONFIGURATION words also refresh the observed burst framing bit.
  Status readRegisters(uint8_t startReg, uint8_t* buf, size_t len);
  /// Read one public 16-bit register. Requires `begin()`; returns
  /// `NOT_INITIALIZED` without I2C while `UNINIT`. Reserved addresses are
  /// rejected before I2C. Returns `OFFLINE` without I2C while `OFFLINE`.
  Status readRegister16(uint8_t reg, uint16_t& value);
  /// Write one public 16-bit register. Requires `begin()`; returns
  /// `NOT_INITIALIZED` without I2C while `UNINIT`. Reserved addresses are
  /// rejected before I2C. Returns `OFFLINE` without I2C while `OFFLINE`. Raw
  /// writes can make cached settings dirty; use typed setters or recover after
  /// diagnostic writes that change configuration, INT, threshold, or FLAGS state.
  Status writeRegister16(uint8_t reg, uint16_t value);
  Status readRegister(uint8_t reg, uint16_t& value) { return readRegister16(reg, value); }
  Status writeRegister(uint8_t reg, uint16_t value) { return writeRegister16(reg, value); }

  // === Utility ===
  void decodeDeviceId(uint16_t raw, DeviceIdInfo& out) const;
  void decodeConfiguration(uint16_t raw, ConfigurationInfo& out) const;
  void decodeIntConfiguration(uint16_t raw, IntConfigurationInfo& out) const;
  /// Convert linear ADC codes to lux using the configured package scale.
  /// PicoStar uses 312.5e-6 lux/code; SOT-5X3 uses 437.5e-6 lux/code.
  float adcCodesToLux(uint32_t adcCodes) const;
  /// Decode result exponent/mantissa to linear ADC codes.
  /// Valid result exponents are 0..8 and mantissa is 20 bits
  /// (`0x00000..0xFFFFF`). Invalid inputs return `INVALID_PARAM`.
  Status rawToAdcCodes(uint8_t exponent, uint32_t mantissa, uint64_t& adcCodes) const;
  /// Status-returning raw result-to-lux conversion. On invalid input, writes
  /// quiet NaN to `lux` and returns `INVALID_PARAM`.
  Status rawToLux(uint8_t exponent, uint32_t mantissa, float& lux) const;
  /// Compatibility raw result-to-lux helper. Returns NaN for invalid input.
  float rawToLux(uint8_t exponent, uint32_t mantissa) const;
  /// Convert a threshold register to lux. Uses 64-bit ADC-code intermediates and
  /// returns NaN for invalid thresholds.
  float thresholdToLux(const Threshold& threshold) const;
  float getLuxLsb() const;
  /// Return the datasheet's rounded nominal full scale, not an exact decoded
  /// maximum for saturation comparisons. Invalid ranges return NaN.
  float getRangeFullScaleLux(Range range) const;
  float getCurrentFullScaleLux() const;
  /// Return the sample exponent's full scale, or NaN for an invalid exponent.
  float getSampleFullScaleLux(const Sample& sample) const;
  /// Invalid conversion times return 0.
  uint8_t getEffectiveBits(ConversionTime time) const;
  uint8_t getEffectiveBits() const;
  /// Invalid range or conversion-time inputs return NaN.
  float getRangeResolutionLux(Range range, ConversionTime time) const;
  float getCurrentResolutionLux() const;
  /// Return the sample exponent's resolution, or NaN for an invalid exponent.
  float getSampleResolutionLux(const Sample& sample) const;
  /// Invalid cached conversion times return 0 as an invalid-value sentinel;
  /// lifecycle and configuration APIs reject them before arming timing gates.
  uint32_t getConversionTimeUs() const;
  uint32_t getConversionTimeMs() const;
  /// Invalid one-shot mode inputs return 0.
  uint32_t getOneShotBudgetUs(Mode mode) const;
  uint32_t getOneShotBudgetMs(Mode mode) const;
  /// Encode lux to the nearest representable exponent/result threshold value.
  /// Rejects negative, non-finite, and out-of-range lux values.
  Status luxToThreshold(float lux, Threshold& out) const;
  /// Decode a threshold register to exact linear ADC codes in a 64-bit container.
  /// Formula: `result << (8 + exponent)`.
  Status thresholdToAdcCodes(const Threshold& threshold, uint64_t& adcCodes) const;
  /// Compatibility threshold decode. Saturates at UINT32_MAX instead of wrapping.
  uint32_t thresholdToAdcCodes(const Threshold& threshold) const;
  /// Return modulo-16 forward delta between hardware sample counters.
  uint8_t sampleCounterDelta(uint8_t previousCounter, uint8_t currentCounter) const;

private:
  enum class PollJob : uint8_t {
    NONE,
    ATTACH,
    READ_SAMPLE,
    READ_BURST,
    CONFIGURE_MEASUREMENT,
    RESET_AND_REAPPLY
  };

  enum class PollStep : uint8_t {
    IDLE,
    READ_DEVICE_ID,
    WAIT_READY,
    READ_FLAGS,
    READ_COUNTER,
    READ_BURST_BLOCK,
    WRITE_RESET,
    WRITE_THRESHOLD_L,
    WRITE_THRESHOLD_H,
    WRITE_INT_CONFIG,
    WRITE_CONFIG
  };

  // === Transport Wrappers ===
  Status _i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                          uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteRawAddr(uint8_t addr, const uint8_t* buf, size_t len);
  Status _i2cWriteRaw(const uint8_t* buf, size_t len);
  Status _i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                              uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteTrackedAddr(uint8_t addr, const uint8_t* buf, size_t len);
  Status _i2cWriteTracked(const uint8_t* buf, size_t len);

  // === Register Access ===
  Status _readRegister16Tracked(uint8_t reg, uint16_t& value);
  Status _writeRegister16Tracked(uint8_t reg, uint16_t value);
  Status _readRegister16Raw(uint8_t reg, uint16_t& value);
  Status _readSampleAt(uint8_t msbReg, Sample& out);
  Status _decodeSampleRegisters(uint16_t resultReg, uint16_t lsbCrcReg, Sample& out) const;
  Status _readBurstBlockTracked(BurstFrame& out);

  // === Health Tracking ===
  Status _updateHealth(const Status& st);
  Status _recordFailure(const Status& st);
  void _reassertOfflineLatch();

  // === Internal Helpers ===
  Status _applyConfig();
  Status _pollReadJob(uint32_t nowMs, uint8_t& remainingInstructions);
  Status _pollConfigJob(uint32_t nowMs, uint8_t& remainingInstructions);
  Status _pollResetAndReapplyJob(uint32_t nowMs, uint8_t& remainingInstructions);
  Status _finishPollJob(const Status& st);
  Status _pollInProgressStatus(const char* message) const;
  bool _pollReadGateElapsed(uint32_t nowMs) const;
  uint32_t _pollContinuousGateMs() const;
  bool _shouldProbeCounterForFreshness(uint32_t nowMs) const;
  uint32_t _readTimeoutMs() const;
  bool _readinessRetryElapsed(uint32_t nowMs) const;
  void _armReadinessRetry(uint32_t nowMs);
  void _captureReadinessFromFlags(uint16_t raw);
  void _finishApplyConfig(uint32_t nowMs);
  void _markHardwareConfigDirty(const Status& st);
  void _clearHardwareConfigDirty();
  void _clearRuntimeState();
  void _cacheSampleAt(const Sample& sample, uint32_t nowMs);
  void _cacheSample(const Sample& sample);
  void _markFreshSampleConsumedAt(const Sample& sample, uint32_t nowMs);
  uint16_t _buildConfigurationRegister(Mode mode) const;
  uint16_t _buildConfigurationRegister(const Config& config, Mode mode) const;
  uint16_t _buildIntConfigurationRegister() const;
  uint16_t _buildIntConfigurationRegister(const Config& config) const;
  Status _validateDeviceId(uint16_t raw) const;
  Status _pollConversionReadyFlag(bool& ready);
  Status _refreshReadinessEvidence(bool& ready);
  bool _intFreshEvidenceAsserted() const;
  bool _sampleCounterIsFresh(uint8_t counter) const;
  void _markFreshSampleConsumed(const Sample& sample);
  void _clearReadinessEvidence();
  uint16_t _packThreshold(const Threshold& threshold) const;
  bool _thresholdValid(const Threshold& threshold) const;
  uint8_t _computeCrcNibble(uint8_t exponent, uint32_t mantissa, uint8_t counter) const;
  uint32_t _nowMs() const;
  void _observeHostMs(uint32_t nowMs);
  void _cooperativeYield() const;

  // === State ===
  Config _config;
  bool _bound = false;
  bool _initialized = false;
  DriverState _driverState = DriverState::UNINIT;
  bool _allowOfflineI2c = false;
  bool _hardwareConfigDirty = false;
  Status _hardwareConfigDirtyError = Status::Ok();
  bool _hwBurstEnabled = false;
  uint32_t _hostMs = 0;
  bool _timeBaseValid = false;

  // === Health Counters ===
  uint32_t _lastOkMs = 0;
  uint32_t _lastErrorMs = 0;
  Status _lastError = Status::Ok();
  uint8_t _consecutiveFailures = 0;
  uint32_t _totalFailures = 0;
  uint32_t _totalSuccess = 0;

  // === Conversion State ===
  bool _sampleAvailable = false;
  bool _lastSampleValid = false;
  bool _conversionStarted = false;
  bool _conversionReady = false;
  uint32_t _readinessRetryMs = 0;
  bool _readinessRetryArmed = false;
  uint32_t _conversionStartMs = 0;
  uint32_t _lastSampleTimestampMs = 0;
  Sample _lastSample{};
  BurstFrame _lastBurst{};
  bool _lastBurstValid = false;
  Mode _pendingMode = Mode::POWER_DOWN;
  bool _lastFreshCounterValid = false;
  uint8_t _lastFreshCounter = 0;
  uint8_t _lastCounter = 0;
  uint32_t _lastAdcCodes = 0;
  float _lastLux = 0.0f;

  // === Poll-chunked Job State ===
  PollJob _pollJob = PollJob::NONE;
  PollStep _pollStep = PollStep::IDLE;
  Status _lastPollStatus = Status::Ok();
  Config _pollTargetConfig{};
  uint8_t _pollWritesApplied = 0;
  bool _pollResetApplied = false;
  bool _pollExecuting = false;
  uint32_t _pollReadStartMs = 0;
  bool _pollReadDeadlineArmed = false;
};

}  // namespace OPT4001
