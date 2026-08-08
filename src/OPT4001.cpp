/// @file OPT4001.cpp
/// @brief Implementation of OPT4001 driver.

#include "OPT4001/OPT4001.h"

#include <climits>
#include <cmath>
#include <limits>

namespace OPT4001 {
namespace {

bool isValidPackageVariant(PackageVariant variant) {
  return variant == PackageVariant::PICOSTAR || variant == PackageVariant::SOT_5X3;
}

bool isValidAddress(uint8_t addr, PackageVariant variant) {
  if (variant == PackageVariant::PICOSTAR) {
    return addr == cmd::I2C_ADDR_DEFAULT;
  }
  return addr == cmd::I2C_ADDR_GND ||
         addr == cmd::I2C_ADDR_DEFAULT ||
         addr == cmd::I2C_ADDR_SDA;
}

bool isValidRange(Range range) {
  const uint8_t value = static_cast<uint8_t>(range);
  return value <= 8U || value == static_cast<uint8_t>(Range::AUTO);
}

bool isValidConversionTime(ConversionTime time) {
  return static_cast<uint8_t>(time) <= static_cast<uint8_t>(ConversionTime::MS_800);
}

bool isValidMode(Mode mode) {
  return static_cast<uint8_t>(mode) <= static_cast<uint8_t>(Mode::CONTINUOUS);
}

bool isOneShotMode(Mode mode) {
  return mode == Mode::ONE_SHOT || mode == Mode::ONE_SHOT_FORCED_AUTO;
}

bool isStableMode(Mode mode) {
  return mode == Mode::POWER_DOWN || mode == Mode::CONTINUOUS;
}

bool isValidInterruptLatch(InterruptLatch latch) {
  return latch == InterruptLatch::TRANSPARENT || latch == InterruptLatch::LATCHED;
}

bool isValidInterruptPolarity(InterruptPolarity polarity) {
  return polarity == InterruptPolarity::ACTIVE_LOW ||
         polarity == InterruptPolarity::ACTIVE_HIGH;
}

bool isValidFaultCount(FaultCount count) {
  return static_cast<uint8_t>(count) <= static_cast<uint8_t>(FaultCount::FAULTS_8);
}

bool isValidIntDirection(IntDirection direction) {
  return direction == IntDirection::PIN_INPUT ||
         direction == IntDirection::PIN_OUTPUT;
}

bool isValidIntConfig(IntConfig config) {
  return config == IntConfig::THRESHOLD ||
         config == IntConfig::EVERY_CONVERSION ||
         config == IntConfig::FIFO_FULL;
}

bool isValidSampleSlot(uint8_t slot) {
  return slot < cmd::SAMPLE_SLOT_COUNT;
}

bool isValidPublicRegisterAddress(uint8_t reg) {
  return reg <= cmd::REG_FLAGS || reg == cmd::REG_DEVICE_ID;
}

bool isValidPublicRegisterBlock(uint8_t startReg, size_t len) {
  if (len == 0) {
    return false;
  }
  const size_t registerSpan = (len - 1U) / 2U;
  if (registerSpan > static_cast<size_t>(UINT8_MAX - startReg)) {
    return false;
  }
  const uint16_t endReg = static_cast<uint16_t>(startReg) +
                          static_cast<uint16_t>(registerSpan);
  for (uint16_t reg = startReg; reg <= endReg; ++reg) {
    if (!isValidPublicRegisterAddress(static_cast<uint8_t>(reg))) {
      return false;
    }
  }
  return true;
}

bool rawWriteCanDirtyCachedSettings(uint8_t reg) {
  return reg == cmd::REG_CONFIGURATION ||
         reg == cmd::REG_INT_CONFIGURATION ||
         reg == cmd::REG_THRESHOLD_L ||
         reg == cmd::REG_THRESHOLD_H;
}

static constexpr uint8_t RESULT_EXPONENT_MAX = 8U;
static constexpr uint32_t RESULT_MANTISSA_MAX = 0x000FFFFFU;
static constexpr uint8_t THRESHOLD_SHIFT_BASE = 8U;
static constexpr uint64_t THRESHOLD_ADC_CODES_MAX =
    static_cast<uint64_t>(cmd::THRESHOLD_RESULT_MAX)
    << (THRESHOLD_SHIFT_BASE + cmd::THRESHOLD_EXPONENT_MAX);
// Allows thresholdToLux(max) float round-trip while still rejecting clearly high inputs.
static constexpr double THRESHOLD_ADC_FLOAT_GUARD_CODES = 4096.0;

float invalidLuxValue() {
  return std::numeric_limits<float>::quiet_NaN();
}

float adcCodesToLux64(uint64_t adcCodes, float luxLsb) {
  return static_cast<float>(static_cast<double>(adcCodes) *
                            static_cast<double>(luxLsb));
}

uint32_t ceilUsToMs(uint32_t microseconds) {
  return (microseconds + 999U) / 1000U;
}

uint32_t blockingPollLimit(uint32_t timeoutMs, uint32_t extraMs = 0) {
  static constexpr uint32_t MAX_POLLS = 1000000U;
  uint64_t polls = (static_cast<uint64_t>(timeoutMs) + extraMs + 1ULL) * 16ULL + 16ULL;
  if (polls > MAX_POLLS) {
    return MAX_POLLS;
  }
  return static_cast<uint32_t>(polls);
}

Status offlineStatus() {
  return Status::Error(Err::OFFLINE, "Driver is offline; call recover()");
}

bool mergeSampleDecodeStatus(Status& aggregate, const Status& slotStatus) {
  if (slotStatus.ok()) {
    return true;
  }
  if (slotStatus.code == Err::CRC_ERROR) {
    if (aggregate.ok()) {
      aggregate = slotStatus;
    }
    return true;
  }
  if (aggregate.ok() || aggregate.code == Err::CRC_ERROR) {
    aggregate = slotStatus;
  }
  return false;
}

class ScopedOfflineI2cAllowance {
public:
  explicit ScopedOfflineI2cAllowance(bool& flag, bool allow) : _flag(flag), _old(flag) {
    _flag = allow;
  }

  ~ScopedOfflineI2cAllowance() {
    _flag = _old;
  }

  ScopedOfflineI2cAllowance(const ScopedOfflineI2cAllowance&) = delete;
  ScopedOfflineI2cAllowance& operator=(const ScopedOfflineI2cAllowance&) = delete;

private:
  bool& _flag;
  bool _old;
};

}  // namespace

// ============================================================================
// Lifecycle
// ============================================================================

Status OPT4001::bind(const Config& config) {
  _config = Config{};
  _bound = false;
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _allowOfflineI2c = false;
  _pollJob = PollJob::NONE;
  _pollStep = PollStep::IDLE;
  _lastPollStatus = Status::Ok();
  _pollWritesApplied = 0;
  _pollResetApplied = false;
  _pollExecuting = false;
  _lastBurstValid = false;
  _clearHardwareConfigDirty();
  _clearRuntimeState();

  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;

  if (config.i2cWrite == nullptr || config.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks required");
  }
  if (!isValidPackageVariant(config.packageVariant)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid package variant");
  }
  if (config.i2cTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "I2C timeout must be > 0");
  }
  if (!isValidAddress(config.i2cAddress, config.packageVariant)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid I2C address for package");
  }
  if (!isValidRange(config.range) || !isValidConversionTime(config.conversionTime)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid range or conversion time");
  }
  if (!isStableMode(config.mode)) {
    return Status::Error(Err::INVALID_CONFIG,
                         "Config mode must be POWER_DOWN or CONTINUOUS");
  }
  if (!isValidInterruptLatch(config.interruptLatch) ||
      !isValidInterruptPolarity(config.interruptPolarity) ||
      !isValidFaultCount(config.faultCount) ||
      !isValidIntDirection(config.intDirection) ||
      !isValidIntConfig(config.intConfig)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid interrupt configuration");
  }
  if (config.intPin < -1) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid INT pin");
  }
  if (config.packageVariant == PackageVariant::PICOSTAR &&
      (config.intPin >= 0 || config.gpioRead != nullptr)) {
    return Status::Error(Err::INVALID_CONFIG,
                         "PicoStar package has no INT pin");
  }
  if (!_thresholdValid(config.lowThreshold) || !_thresholdValid(config.highThreshold)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid threshold register value");
  }

  _config = config;
  if (_config.offlineThreshold == 0) {
    _config.offlineThreshold = 1;
  }

  _bound = true;
  return Status::Ok();
}

void OPT4001::unbind() {
  _config = Config{};
  _bound = false;
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _allowOfflineI2c = false;
  _pollJob = PollJob::NONE;
  _pollStep = PollStep::IDLE;
  _lastPollStatus = Status::Ok();
  _pollWritesApplied = 0;
  _pollResetApplied = false;
  _pollExecuting = false;
  _clearHardwareConfigDirty();
  _clearRuntimeState();

  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;
}

Status OPT4001::begin(const Config& config) {
  Status st = bind(config);
  if (!st.ok()) {
    return st;
  }

  st = probe();
  if (!st.ok()) {
    return st;
  }

  st = _applyConfig();
  if (!st.ok()) {
    return st;
  }

  _initialized = true;
  _driverState = DriverState::READY;
  return Status::Ok();
}

void OPT4001::tick(uint32_t nowMs) {
  if (!_initialized) {
    return;
  }
  if (_driverState == DriverState::OFFLINE) {
    return;
  }
  if (_pollJob != PollJob::NONE) {
    return;
  }

  if (_config.mode == Mode::CONTINUOUS) {
    if (!_conversionReady && (nowMs - _conversionStartMs) >= getConversionTimeMs()) {
      bool ready = false;
      (void)_refreshReadinessEvidence(ready);
    }
    return;
  }

  if (!_conversionStarted || _conversionReady) {
    return;
  }

  if ((nowMs - _conversionStartMs) < getOneShotBudgetMs(_pendingMode)) {
    return;
  }

  bool ready = false;
  (void)_refreshReadinessEvidence(ready);
}

void OPT4001::end() {
  if (_initialized && _driverState != DriverState::OFFLINE) {
    const uint16_t powerDownCfg = _buildConfigurationRegister(Mode::POWER_DOWN);
    const uint8_t payload[3] = {
      cmd::REG_CONFIGURATION,
      static_cast<uint8_t>(powerDownCfg >> 8),
      static_cast<uint8_t>(powerDownCfg & 0xFF)
    };
    (void)_i2cWriteRaw(payload, sizeof(payload));
  }

  _initialized = false;
  _driverState = DriverState::UNINIT;
  _allowOfflineI2c = false;
  _pollJob = PollJob::NONE;
  _pollStep = PollStep::IDLE;
  _lastPollStatus = Status::Ok();
  _pollWritesApplied = 0;
  _pollResetApplied = false;
  _pollExecuting = false;
  _clearRuntimeState();
}

Status OPT4001::startAttach() {
  if (!_bound) {
    return Status::Error(Err::NOT_BOUND, "Driver not bound");
  }
  if (_initialized) {
    return Status::Error(Err::BUSY, "Driver already initialized");
  }
  if (_pollJob != PollJob::NONE) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }

  _pollTargetConfig = _config;
  _pollWritesApplied = 0;
  _pollResetApplied = false;
  _pollJob = PollJob::ATTACH;
  _pollStep = PollStep::READ_DEVICE_ID;
  _lastPollStatus = _pollInProgressStatus("Attach job started");
  return _lastPollStatus;
}

Status OPT4001::powerDown() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_pollJob != PollJob::NONE) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }

  const uint16_t configReg = _buildConfigurationRegister(Mode::POWER_DOWN);
  Status st = _writeRegister16Tracked(cmd::REG_CONFIGURATION, configReg);
  if (!st.ok()) {
    return st;
  }
  _config.mode = Mode::POWER_DOWN;
  _clearRuntimeState();
  return Status::Ok();
}

// ============================================================================
// Diagnostics
// ============================================================================

Status OPT4001::probe() {
  uint16_t deviceId = 0;
  Status st = _readRegister16Raw(cmd::REG_DEVICE_ID, deviceId);
  if (!st.ok()) {
    return st;
  }
  return _validateDeviceId(deviceId);
}

Status OPT4001::recover() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  const bool startedOffline = (_driverState == DriverState::OFFLINE);
  ScopedOfflineI2cAllowance allowOfflineI2c(_allowOfflineI2c, true);

  uint16_t deviceId = 0;
  Status st = readDeviceId(deviceId);
  if (!st.ok()) {
    if (startedOffline) {
      _reassertOfflineLatch();
    }
    return st;
  }
  st = _validateDeviceId(deviceId);
  if (!st.ok()) {
    st = _recordFailure(st);
    if (startedOffline) {
      _reassertOfflineLatch();
    }
    return st;
  }

  _clearRuntimeState();
  st = _applyConfig();
  if (!st.ok() && startedOffline) {
    _reassertOfflineLatch();
  }
  return st;
}

Status OPT4001::softReset() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  const uint8_t payload[1] = {cmd::GENERAL_CALL_RESET};
  ScopedOfflineI2cAllowance allowOfflineI2c(_allowOfflineI2c, true);
  Status st = _i2cWriteTrackedAddr(cmd::GENERAL_CALL_ADDRESS, payload, sizeof(payload));
  if (!st.ok()) {
    return st;
  }

  _initialized = false;
  _driverState = DriverState::UNINIT;
  _clearRuntimeState();
  return Status::Ok();
}

Status OPT4001::resetAndReapply() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  const Config savedConfig = _config;
  const uint8_t payload[1] = {cmd::GENERAL_CALL_RESET};
  ScopedOfflineI2cAllowance allowOfflineI2c(_allowOfflineI2c, true);
  Status st = _i2cWriteTrackedAddr(cmd::GENERAL_CALL_ADDRESS, payload, sizeof(payload));
  if (!st.ok()) {
    return st;
  }

  _config = savedConfig;
  _clearRuntimeState();

  st = _applyConfig();
  if (!st.ok()) {
    _markHardwareConfigDirty(st);
    _initialized = false;
    _driverState = DriverState::UNINIT;
    return st;
  }

  _initialized = true;
  _driverState = DriverState::READY;
  _consecutiveFailures = 0;
  return Status::Ok();
}

Status OPT4001::readDeviceId(uint16_t& value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return _readRegister16Tracked(cmd::REG_DEVICE_ID, value);
}

Status OPT4001::readDeviceId(DeviceIdInfo& out) {
  uint16_t raw = 0;
  Status st = readDeviceId(raw);
  if (!st.ok()) {
    return st;
  }
  decodeDeviceId(raw, out);
  return Status::Ok();
}

Status OPT4001::getSettings(SettingsSnapshot& out) const {
  out.bound = _bound;
  out.initialized = _initialized;
  out.state = _driverState;
  out.packageVariant = _config.packageVariant;
  out.i2cAddress = _config.i2cAddress;
  out.i2cTimeoutMs = _config.i2cTimeoutMs;
  out.offlineThreshold = _config.offlineThreshold;
  out.verifyCrc = _config.verifyCrc;
  out.hasNowMsHook = (_config.nowMs != nullptr);
  out.hasGpioReadHook = (_config.gpioRead != nullptr);
  out.hasCooperativeYieldHook = (_config.cooperativeYield != nullptr);
  out.intPin = _config.intPin;
  out.quickWake = _config.quickWake;
  out.range = _config.range;
  out.conversionTime = _config.conversionTime;
  out.mode = _config.mode;
  out.pendingMode = _pendingMode;
  out.interruptLatch = _config.interruptLatch;
  out.interruptPolarity = _config.interruptPolarity;
  out.faultCount = _config.faultCount;
  out.intDirection = _config.intDirection;
  out.intConfig = _config.intConfig;
  out.burstMode = _config.burstMode;
  out.lowThreshold = _config.lowThreshold;
  out.highThreshold = _config.highThreshold;
  out.sampleAvailable = _sampleAvailable;
  out.hasSample = _lastSampleValid;
  out.lastSampleValid = _lastSampleValid;
  out.conversionStarted = _conversionStarted;
  out.conversionReady = _conversionReady;
  out.conversionStartMs = _conversionStartMs;
  out.sampleTimestampMs = _lastSampleTimestampMs;
  out.lastCounter = _lastCounter;
  out.lastAdcCodes = _lastAdcCodes;
  out.lastLux = _lastLux;
  out.hardwareConfigDirty = _hardwareConfigDirty;
  out.hardwareConfigDirtyError = _hardwareConfigDirtyError;
  return Status::Ok();
}

// ============================================================================
// Measurement API
// ============================================================================

Status OPT4001::startConversion() {
  return startConversion(Mode::ONE_SHOT);
}

Status OPT4001::startConversion(Mode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isOneShotMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Mode must be a one-shot mode");
  }
  if (_config.mode == Mode::CONTINUOUS) {
    return Status::Error(Err::BUSY, "Continuous mode active");
  }
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  Status st = _writeRegister16Tracked(cmd::REG_CONFIGURATION, _buildConfigurationRegister(mode));
  if (!st.ok()) {
    return st;
  }

  _sampleAvailable = false;
  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = _nowMs();
  _pendingMode = mode;
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

Status OPT4001::poll(uint32_t nowMs, uint8_t maxInstructions) {
  if (_pollJob == PollJob::NONE) {
    return _lastPollStatus;
  }

  uint8_t remaining = maxInstructions;
  Status st = _pollInProgressStatus("Poll job in progress");

  while (_pollJob != PollJob::NONE) {
    const uint8_t before = remaining;
    const bool wasPollExecuting = _pollExecuting;
    _pollExecuting = true;
    switch (_pollJob) {
      case PollJob::ATTACH:
        if (_pollStep == PollStep::READ_DEVICE_ID) {
          if (remaining == 0U) {
            st = _pollInProgressStatus("Poll instruction budget exhausted");
            break;
          }
          uint16_t deviceId = 0;
          st = _readRegister16Raw(cmd::REG_DEVICE_ID, deviceId);
          --remaining;
          if (!st.ok()) {
            st = _finishPollJob(st);
            break;
          }
          st = _validateDeviceId(deviceId);
          if (!st.ok()) {
            st = _finishPollJob(st);
            break;
          }
          _pollStep = PollStep::WRITE_THRESHOLD_L;
        }
        if (_pollJob != PollJob::NONE && remaining > 0U) {
          st = _pollConfigJob(nowMs, remaining);
        } else if (_pollJob != PollJob::NONE) {
          st = _pollInProgressStatus("Poll instruction budget exhausted");
        }
        break;
      case PollJob::READ_SAMPLE:
      case PollJob::READ_BURST:
        st = _pollReadJob(nowMs, remaining);
        break;
      case PollJob::CONFIGURE_MEASUREMENT:
        st = _pollConfigJob(nowMs, remaining);
        break;
      case PollJob::RESET_AND_REAPPLY:
        st = _pollResetAndReapplyJob(nowMs, remaining);
        break;
      case PollJob::NONE:
        st = Status::Ok();
        break;
    }
    _pollExecuting = wasPollExecuting;

    _lastPollStatus = st;
    if (_pollJob == PollJob::NONE || !st.inProgress()) {
      return st;
    }
    if (remaining == 0U || remaining == before) {
      return st;
    }
  }

  _lastPollStatus = st;
  return st;
}

bool OPT4001::pollBusy() const {
  return _pollJob != PollJob::NONE;
}

Status OPT4001::startReadSample() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_driverState == DriverState::OFFLINE) {
    return offlineStatus();
  }
  if (_pollJob != PollJob::NONE) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }
  if (!_config.burstMode) {
    return Status::Error(Err::INVALID_CONFIG, "Poll sample requires I2C burst mode");
  }

  _pollJob = PollJob::READ_SAMPLE;
  _pollStep = PollStep::WAIT_READY;
  _lastBurstValid = false;
  _lastPollStatus = _pollInProgressStatus("Read sample job started");
  return _lastPollStatus;
}

Status OPT4001::startReadBurst() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_driverState == DriverState::OFFLINE) {
    return offlineStatus();
  }
  if (_pollJob != PollJob::NONE) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }
  if (!_config.burstMode) {
    return Status::Error(Err::INVALID_CONFIG, "Poll burst requires I2C burst mode");
  }

  _pollJob = PollJob::READ_BURST;
  _pollStep = PollStep::WAIT_READY;
  _lastBurstValid = false;
  _lastPollStatus = _pollInProgressStatus("Read burst job started");
  return _lastPollStatus;
}

Status OPT4001::getLastBurst(BurstFrame& out) const {
  if (!_lastBurstValid) {
    return Status::Error(Err::MEASUREMENT_NOT_READY, "No cached burst frame");
  }
  out = _lastBurst;
  return Status::Ok();
}

Status OPT4001::startConfigureMeasurement(Range range,
                                          ConversionTime time,
                                          Mode mode,
                                          bool quickWake) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_driverState == DriverState::OFFLINE) {
    return offlineStatus();
  }
  if (_pollJob != PollJob::NONE) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }
  if (!isValidRange(range)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid range");
  }
  if (!isValidConversionTime(time)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  if (!isStableMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Mode must be POWER_DOWN or CONTINUOUS");
  }

  _pollTargetConfig = _config;
  _pollTargetConfig.range = range;
  _pollTargetConfig.conversionTime = time;
  _pollTargetConfig.mode = mode;
  _pollTargetConfig.quickWake = quickWake;
  _pollWritesApplied = 0;
  _pollResetApplied = false;
  _pollJob = PollJob::CONFIGURE_MEASUREMENT;
  _pollStep = PollStep::WRITE_THRESHOLD_L;
  _lastPollStatus = _pollInProgressStatus("Configure measurement job started");
  return _lastPollStatus;
}

Status OPT4001::startResetAndReapply() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_pollJob != PollJob::NONE) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }

  _pollTargetConfig = _config;
  _pollWritesApplied = 0;
  _pollResetApplied = false;
  _pollJob = PollJob::RESET_AND_REAPPLY;
  _pollStep = PollStep::WRITE_RESET;
  _lastPollStatus = _pollInProgressStatus("Reset and reapply job started");
  return _lastPollStatus;
}

Status OPT4001::cancelPollJob() {
  if (_pollJob == PollJob::NONE) {
    return Status::Ok();
  }

  const PollJob cancelledJob = _pollJob;
  const bool configurationMayDiffer =
      _pollWritesApplied > 0U || _pollResetApplied;
  const Status cancelled =
      Status::Error(Err::CANCELLED, "Poll job cancelled");
  if (configurationMayDiffer) {
    _markHardwareConfigDirty(cancelled);
  }
  if (cancelledJob == PollJob::ATTACH ||
      (cancelledJob == PollJob::RESET_AND_REAPPLY && _pollResetApplied)) {
    _initialized = false;
    _driverState = DriverState::UNINIT;
    _clearRuntimeState();
  }
  return _finishPollJob(cancelled);
}

Status OPT4001::conversionReady(bool& ready) {
  ready = false;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_driverState == DriverState::OFFLINE) {
    return offlineStatus();
  }

  return _refreshReadinessEvidence(ready);
}

bool OPT4001::conversionReady() {
  bool ready = false;
  Status st = conversionReady(ready);
  return st.ok() && ready;
}

Status OPT4001::readSample(Sample& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  bool ready = false;
  Status readySt = _refreshReadinessEvidence(ready);
  if (!readySt.ok()) {
    return readySt;
  }
  if (!ready) {
    return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
  }

  Status st = _readSampleAt(cmd::REG_RESULT, out);
  if (!st.ok() && st.code != Err::CRC_ERROR) {
    return st;
  }

  if (!_sampleCounterIsFresh(out.counter)) {
    _clearReadinessEvidence();
    return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
  }

  _markFreshSampleConsumed(out);

  return st;
}

Status OPT4001::readLatestSample(Sample& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  Status st = _readSampleAt(cmd::REG_RESULT, out);
  if (!st.ok() && st.code != Err::CRC_ERROR) {
    return st;
  }

  _cacheSample(out);
  return st;
}

Status OPT4001::readBurst(BurstFrame& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  bool ready = false;
  Status readySt = _refreshReadinessEvidence(ready);
  if (!readySt.ok()) {
    return readySt;
  }
  if (!ready) {
    return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
  }

  if (_config.burstMode) {
    Status status = _readBurstBlockTracked(out);
    if (!status.ok() && status.code != Err::CRC_ERROR) {
      return status;
    }

    if (!_sampleCounterIsFresh(out.newest.counter)) {
      _clearReadinessEvidence();
      return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
    }

    _lastBurst = out;
    _lastBurstValid = true;
    _markFreshSampleConsumed(out.newest);
    return status;
  }

  Status status = Status::Ok();
  Status slotStatus = _readSampleAt(cmd::REG_RESULT, out.newest);
  if (!mergeSampleDecodeStatus(status, slotStatus)) {
    return status;
  }
  slotStatus = _readSampleAt(cmd::REG_FIFO0_MSB, out.fifo0);
  if (!mergeSampleDecodeStatus(status, slotStatus)) {
    return status;
  }
  slotStatus = _readSampleAt(cmd::REG_FIFO1_MSB, out.fifo1);
  if (!mergeSampleDecodeStatus(status, slotStatus)) {
    return status;
  }
  slotStatus = _readSampleAt(cmd::REG_FIFO2_MSB, out.fifo2);
  if (!mergeSampleDecodeStatus(status, slotStatus)) {
    return status;
  }

  if (!status.ok() && status.code != Err::CRC_ERROR) {
    return status;
  }

  if (!_sampleCounterIsFresh(out.newest.counter)) {
    _clearReadinessEvidence();
    return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
  }

  _lastBurst = out;
  _lastBurstValid = true;
  _markFreshSampleConsumed(out.newest);
  return status;
}

Status OPT4001::readSampleSlot(uint8_t slot, Sample& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidSampleSlot(slot)) {
    return Status::Error(Err::INVALID_PARAM, "Sample slot must be 0..3");
  }

  if (slot == 0U) {
    bool ready = false;
    Status readySt = _refreshReadinessEvidence(ready);
    if (!readySt.ok()) {
      return readySt;
    }
    if (!ready) {
      return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
    }
  }

  const uint8_t msbReg = static_cast<uint8_t>(cmd::REG_RESULT + (slot * 2U));
  Status st = _readSampleAt(msbReg, out);
  if (!st.ok() && st.code != Err::CRC_ERROR) {
    return st;
  }

  if (slot == 0U) {
    if (!_sampleCounterIsFresh(out.counter)) {
      _clearReadinessEvidence();
      return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
    }
    _markFreshSampleConsumed(out);
  }

  return st;
}

Status OPT4001::getLastSample(Sample& out) const {
  if (!_lastSampleValid) {
    return Status::Error(Err::MEASUREMENT_NOT_READY, "No cached sample");
  }
  out = _lastSample;
  return Status::Ok();
}

uint32_t OPT4001::sampleTimestampMs() const {
  return _lastSampleTimestampMs;
}

uint32_t OPT4001::sampleAgeMs(uint32_t nowMs) const {
  if (!_lastSampleValid) {
    return 0;
  }
  return nowMs - _lastSampleTimestampMs;
}

Status OPT4001::readLux(float& lux) {
  Sample sample;
  Status st = readSample(sample);
  if (!st.ok() && st.code != Err::CRC_ERROR) {
    return st;
  }
  lux = sample.lux;
  return st;
}

Status OPT4001::readMilliLux(uint32_t& milliLux) {
  uint64_t microLux = 0;
  Status st = readMicroLux(microLux);
  if (!st.ok() && st.code != Err::CRC_ERROR) {
    return st;
  }
  milliLux = static_cast<uint32_t>((microLux + 500ULL) / 1000ULL);
  return st;
}

Status OPT4001::readMicroLux(uint64_t& microLux) {
  Sample sample;
  Status st = readSample(sample);
  if (!st.ok() && st.code != Err::CRC_ERROR) {
    return st;
  }

  const uint64_t numerator =
      (_config.packageVariant == PackageVariant::PICOSTAR)
          ? cmd::MICRO_LUX_NUMERATOR_PICOSTAR
          : cmd::MICRO_LUX_NUMERATOR_SOT_5X3;
  microLux = (static_cast<uint64_t>(sample.adcCodes) * numerator + 5ULL) / 10ULL;
  return st;
}

Status OPT4001::readBlocking(Sample& out, uint32_t timeoutMs) {
  return readFreshBlocking(out, Mode::ONE_SHOT, timeoutMs);
}

Status OPT4001::readBlocking(Sample& out, Mode mode, uint32_t timeoutMs) {
  return readFreshBlocking(out, mode, timeoutMs);
}

Status OPT4001::readFreshBlocking(Sample& out, uint32_t timeoutMs) {
  return readFreshBlocking(out, Mode::ONE_SHOT, timeoutMs);
}

Status OPT4001::readFreshBlocking(Sample& out, Mode mode, uint32_t timeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_config.nowMs == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "Blocking reads require Config::nowMs");
  }
  if (_config.mode == Mode::CONTINUOUS) {
    const uint32_t startMs = _nowMs();
    uint32_t polls = 0;
    const uint32_t maxPolls = blockingPollLimit(timeoutMs);
    while (static_cast<uint32_t>(_nowMs() - startMs) < timeoutMs &&
           polls < maxPolls) {
      bool didRead = false;
      Status st = tryReadFreshSample(out, didRead);
      if (!st.ok() && st.code != Err::CRC_ERROR) {
        return st;
      }
      if (didRead) {
        return st;
      }
      ++polls;
      _cooperativeYield();
    }
    return Status::Error(Err::TIMEOUT, "Continuous sample timeout");
  }
  if (!isOneShotMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Mode must be a one-shot mode");
  }

  Status st = startConversion(mode);
  if (st.code != Err::IN_PROGRESS && st.code != Err::BUSY) {
    return st;
  }

  const uint32_t startMs = _nowMs();
  const uint32_t budgetMs = getOneShotBudgetMs(_pendingMode);

  uint32_t polls = 0;
  const uint32_t maxPolls = blockingPollLimit(timeoutMs, budgetMs);
  while (static_cast<uint32_t>(_nowMs() - startMs) < timeoutMs &&
         polls < maxPolls) {
    if (static_cast<uint32_t>(_nowMs() - _conversionStartMs) < budgetMs) {
      ++polls;
      _cooperativeYield();
      continue;
    }

    Status readSt = readSample(out);
    if (readSt.ok() || readSt.code == Err::CRC_ERROR) {
      return readSt;
    }
    if (readSt.code != Err::MEASUREMENT_NOT_READY) {
      return readSt;
    }
    ++polls;
    _cooperativeYield();
  }

  return Status::Error(Err::TIMEOUT, "Conversion timeout");
}

Status OPT4001::readBlockingLux(float& lux, uint32_t timeoutMs) {
  return readBlockingLux(lux, Mode::ONE_SHOT, timeoutMs);
}

Status OPT4001::readBlockingLux(float& lux, Mode mode, uint32_t timeoutMs) {
  Sample sample;
  Status st = readBlocking(sample, mode, timeoutMs);
  if (!st.ok() && st.code != Err::CRC_ERROR) {
    return st;
  }
  lux = sample.lux;
  return st;
}

Status OPT4001::tryReadSample(Sample& out, bool& didRead) {
  return tryReadFreshSample(out, didRead);
}

Status OPT4001::tryReadFreshSample(Sample& out, bool& didRead) {
  didRead = false;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  bool ready = false;
  Status readySt = _refreshReadinessEvidence(ready);
  if (!readySt.ok()) {
    return readySt;
  }

  if (!ready) {
    return Status::Ok();
  }

  Status st = readSample(out);
  if (st.ok() || st.code == Err::CRC_ERROR) {
    didRead = true;
    return st;
  }
  if (st.code == Err::MEASUREMENT_NOT_READY) {
    return Status::Ok();
  }
  return st;
}

Status OPT4001::tryReadLux(float& lux, bool& didRead) {
  Sample sample;
  Status st = tryReadSample(sample, didRead);
  if (!st.ok() && st.code != Err::CRC_ERROR) {
    return st;
  }
  if (didRead) {
    lux = sample.lux;
  }
  return st;
}

// ============================================================================
// Flags / Status
// ============================================================================

Status OPT4001::readFlags(Flags& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t raw = 0;
  Status st = _readRegister16Tracked(cmd::REG_FLAGS, raw);
  if (!st.ok()) {
    return st;
  }

  out.raw = raw;
  out.overload = (raw & cmd::MASK_OVERLOAD_FLAG) != 0;
  out.conversionReady = (raw & cmd::MASK_CONVERSION_READY_FLAG) != 0;
  out.highThreshold = (raw & cmd::MASK_FLAG_H) != 0;
  out.lowThreshold = (raw & cmd::MASK_FLAG_L) != 0;

  if (out.conversionReady) {
    _sampleAvailable = true;
    _conversionReady = true;
    _conversionStarted = false;
    _pendingMode = Mode::POWER_DOWN;
  }

  return Status::Ok();
}

Status OPT4001::readFlagsRaw(uint16_t& value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status st = _readRegister16Tracked(cmd::REG_FLAGS, value);
  if (st.ok()) {
    _clearReadinessEvidence();
  }
  return st;
}

Status OPT4001::clearConversionReadyFlag() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  Status st = _writeRegister16Tracked(cmd::REG_FLAGS, 0x0001);
  if (st.ok()) {
    _clearReadinessEvidence();
  }
  return st;
}

Status OPT4001::clearFlags() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  Flags flags;
  Status st = readFlags(flags);
  if (st.ok()) {
    _clearReadinessEvidence();
  }
  return st;
}

Status OPT4001::readIntPinAsserted(bool& asserted) const {
  asserted = false;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_config.packageVariant != PackageVariant::SOT_5X3) {
    return Status::Error(Err::INVALID_CONFIG, "INT pin unavailable on PicoStar package");
  }
  if (_config.intPin < 0 || _config.gpioRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "INT GPIO hook not configured");
  }

  const bool levelHigh = _config.gpioRead(_config.intPin, _config.gpioUser);
  asserted = (_config.interruptPolarity == InterruptPolarity::ACTIVE_HIGH)
                 ? levelHigh
                 : !levelHigh;
  return Status::Ok();
}

// ============================================================================
// Configuration
// ============================================================================

Status OPT4001::setPackageVariant(PackageVariant variant) {
  if (_pollJob != PollJob::NONE) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }
  if (!isValidPackageVariant(variant)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid package variant");
  }
  if (!isValidAddress(_config.i2cAddress, variant)) {
    return Status::Error(Err::INVALID_PARAM, "Current I2C address invalid for package");
  }
  if (variant == PackageVariant::PICOSTAR &&
      (_config.intPin >= 0 || _config.gpioRead != nullptr)) {
    return Status::Error(Err::INVALID_CONFIG,
                         "PicoStar package has no INT pin");
  }
  _config.packageVariant = variant;
  return Status::Ok();
}

Status OPT4001::setRange(Range range) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidRange(range)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid range");
  }
  const Config oldConfig = _config;
  _config.range = range;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::setConversionTime(ConversionTime time) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidConversionTime(time)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  const Config oldConfig = _config;
  _config.conversionTime = time;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::setMode(Mode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isStableMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Use startConversion() for one-shot modes");
  }
  const Config oldConfig = _config;
  _config.mode = mode;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::setQuickWake(bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  const Config oldConfig = _config;
  _config.quickWake = enable;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::setVerifyCrc(bool enable) {
  if (_pollJob != PollJob::NONE) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }
  _config.verifyCrc = enable;
  return Status::Ok();
}

Status OPT4001::setInterruptLatch(InterruptLatch latch) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidInterruptLatch(latch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid interrupt latch");
  }
  const Config oldConfig = _config;
  _config.interruptLatch = latch;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::setInterruptPolarity(InterruptPolarity polarity) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidInterruptPolarity(polarity)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid interrupt polarity");
  }
  const Config oldConfig = _config;
  _config.interruptPolarity = polarity;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::setFaultCount(FaultCount count) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidFaultCount(count)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid fault count");
  }
  const Config oldConfig = _config;
  _config.faultCount = count;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::setIntDirection(IntDirection direction) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidIntDirection(direction)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid INT direction");
  }
  const Config oldConfig = _config;
  _config.intDirection = direction;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::setIntConfig(IntConfig config) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidIntConfig(config)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid INT configuration");
  }
  const Config oldConfig = _config;
  _config.intConfig = config;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::setBurstMode(bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  const Config oldConfig = _config;
  _config.burstMode = enable;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::setThresholds(const Threshold& low, const Threshold& high) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!_thresholdValid(low) || !_thresholdValid(high)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid threshold");
  }

  const Config oldConfig = _config;
  _config.lowThreshold = low;
  _config.highThreshold = high;

  Status st = _writeRegister16Tracked(cmd::REG_THRESHOLD_L, _packThreshold(low));
  if (!st.ok()) {
    _config = oldConfig;
    return st;
  }
  st = _writeRegister16Tracked(cmd::REG_THRESHOLD_H, _packThreshold(high));
  if (!st.ok()) {
    _markHardwareConfigDirty(st);
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::getThresholds(Threshold& low, Threshold& high) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t lowRaw = 0;
  uint16_t highRaw = 0;
  Status st = _readRegister16Tracked(cmd::REG_THRESHOLD_L, lowRaw);
  if (!st.ok()) {
    return st;
  }
  st = _readRegister16Tracked(cmd::REG_THRESHOLD_H, highRaw);
  if (!st.ok()) {
    return st;
  }

  low.exponent = static_cast<uint8_t>((lowRaw & cmd::MASK_THRESHOLD_EXPONENT) >>
                                      cmd::BIT_THRESHOLD_EXPONENT);
  low.result = lowRaw & cmd::MASK_THRESHOLD_RESULT;
  high.exponent = static_cast<uint8_t>((highRaw & cmd::MASK_THRESHOLD_EXPONENT) >>
                                       cmd::BIT_THRESHOLD_EXPONENT);
  high.result = highRaw & cmd::MASK_THRESHOLD_RESULT;

  _config.lowThreshold = low;
  _config.highThreshold = high;
  return Status::Ok();
}

Status OPT4001::getThresholdsLux(float& lowLux, float& highLux) {
  Threshold low;
  Threshold high;
  Status st = getThresholds(low, high);
  if (!st.ok()) {
    return st;
  }
  lowLux = thresholdToLux(low);
  highLux = thresholdToLux(high);
  return Status::Ok();
}

Status OPT4001::setThresholdsLux(float lowLux, float highLux) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  Threshold low;
  Threshold high;
  Status st = luxToThreshold(lowLux, low);
  if (!st.ok()) {
    return st;
  }
  st = luxToThreshold(highLux, high);
  if (!st.ok()) {
    return st;
  }
  return setThresholds(low, high);
}

Status OPT4001::configureMeasurement(Range range,
                                     ConversionTime time,
                                     Mode mode,
                                     bool quickWake) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidRange(range)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid range");
  }
  if (!isValidConversionTime(time)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  if (!isStableMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Mode must be POWER_DOWN or CONTINUOUS");
  }

  const Config oldConfig = _config;
  _config.range = range;
  _config.conversionTime = time;
  _config.mode = mode;
  _config.quickWake = quickWake;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::restoreDefaultThresholds() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  const Threshold low{
      static_cast<uint8_t>((cmd::THRESHOLD_L_RESET & cmd::MASK_THRESHOLD_EXPONENT) >>
                           cmd::BIT_THRESHOLD_EXPONENT),
      static_cast<uint16_t>(cmd::THRESHOLD_L_RESET & cmd::MASK_THRESHOLD_RESULT)};
  const Threshold high{
      static_cast<uint8_t>((cmd::THRESHOLD_H_RESET & cmd::MASK_THRESHOLD_EXPONENT) >>
                           cmd::BIT_THRESHOLD_EXPONENT),
      static_cast<uint16_t>(cmd::THRESHOLD_H_RESET & cmd::MASK_THRESHOLD_RESULT)};
  return setThresholds(low, high);
}

Status OPT4001::enableThresholdInterrupt(const Threshold& low, const Threshold& high) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_config.packageVariant != PackageVariant::SOT_5X3) {
    return Status::Error(Err::INVALID_CONFIG,
                         "Threshold INT requires SOT-5X3 package");
  }
  if (!_thresholdValid(low) || !_thresholdValid(high)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid threshold");
  }
  uint64_t lowCodes = 0;
  uint64_t highCodes = 0;
  Status st = thresholdToAdcCodes(low, lowCodes);
  if (!st.ok()) {
    return st;
  }
  st = thresholdToAdcCodes(high, highCodes);
  if (!st.ok()) {
    return st;
  }
  if (lowCodes > highCodes) {
    return Status::Error(Err::INVALID_PARAM, "Low threshold must be <= high threshold");
  }

  const Config oldConfig = _config;
  _config.lowThreshold = low;
  _config.highThreshold = high;
  _config.intDirection = IntDirection::PIN_OUTPUT;
  _config.intConfig = IntConfig::THRESHOLD;
  st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::enableThresholdInterruptLux(float lowLux, float highLux) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (lowLux > highLux) {
    return Status::Error(Err::INVALID_PARAM, "Low lux threshold must be <= high lux threshold");
  }

  Threshold low;
  Threshold high;
  Status st = luxToThreshold(lowLux, low);
  if (!st.ok()) {
    return st;
  }
  st = luxToThreshold(highLux, high);
  if (!st.ok()) {
    return st;
  }
  return enableThresholdInterrupt(low, high);
}

Status OPT4001::enableConversionReadyInterrupt() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_config.packageVariant != PackageVariant::SOT_5X3) {
    return Status::Error(Err::INVALID_CONFIG,
                         "Conversion INT requires SOT-5X3 package");
  }

  const Config oldConfig = _config;
  _config.intDirection = IntDirection::PIN_OUTPUT;
  _config.intConfig = IntConfig::EVERY_CONVERSION;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::enableFifoFullInterrupt() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_config.packageVariant != PackageVariant::SOT_5X3) {
    return Status::Error(Err::INVALID_CONFIG,
                         "FIFO INT requires SOT-5X3 package");
  }

  const Config oldConfig = _config;
  _config.intDirection = IntDirection::PIN_OUTPUT;
  _config.intConfig = IntConfig::FIFO_FULL;
  Status st = _applyConfig();
  if (!st.ok()) {
    _config = oldConfig;
  }
  return st;
}

Status OPT4001::readConfiguration(uint16_t& value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return _readRegister16Tracked(cmd::REG_CONFIGURATION, value);
}

Status OPT4001::readConfiguration(ConfigurationInfo& out) {
  uint16_t raw = 0;
  Status st = readConfiguration(raw);
  if (!st.ok()) {
    return st;
  }
  decodeConfiguration(raw, out);
  return Status::Ok();
}

Status OPT4001::writeConfiguration(uint16_t value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if ((value & cmd::MASK_CONFIGURATION_RESERVED) != 0) {
    return Status::Error(Err::INVALID_PARAM, "Configuration reserved bit must be 0");
  }

  const Range range =
      static_cast<Range>((value & cmd::MASK_RANGE) >> cmd::BIT_RANGE);
  const ConversionTime convTime =
      static_cast<ConversionTime>((value & cmd::MASK_CONVERSION_TIME) >>
                                  cmd::BIT_CONVERSION_TIME);
  const Mode mode =
      static_cast<Mode>((value & cmd::MASK_MODE) >> cmd::BIT_MODE);
  const InterruptLatch latch =
      static_cast<InterruptLatch>((value & cmd::MASK_LATCH) >> cmd::BIT_LATCH);
  const InterruptPolarity polarity =
      static_cast<InterruptPolarity>((value & cmd::MASK_INT_POL) >> cmd::BIT_INT_POL);
  const FaultCount faultCount =
      static_cast<FaultCount>((value & cmd::MASK_FAULT_COUNT) >> cmd::BIT_FAULT_COUNT);

  if (!isValidRange(range) ||
      !isValidConversionTime(convTime) ||
      !isValidMode(mode) ||
      !isValidInterruptLatch(latch) ||
      !isValidInterruptPolarity(polarity) ||
      !isValidFaultCount(faultCount)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid configuration value");
  }

  Status st = _writeRegister16Tracked(cmd::REG_CONFIGURATION, value);
  if (!st.ok()) {
    return st;
  }

  _config.quickWake = (value & cmd::MASK_QWAKE) != 0;
  _config.range = range;
  _config.conversionTime = convTime;
  _config.interruptLatch = latch;
  _config.interruptPolarity = polarity;
  _config.faultCount = faultCount;

  if (mode == Mode::CONTINUOUS) {
    _config.mode = Mode::CONTINUOUS;
    _sampleAvailable = false;
    _conversionStarted = true;
    _conversionReady = false;
    _conversionStartMs = _nowMs();
    _pendingMode = Mode::POWER_DOWN;
    return Status::Ok();
  }
  if (mode == Mode::POWER_DOWN) {
    _config.mode = Mode::POWER_DOWN;
    _sampleAvailable = false;
    _conversionStarted = false;
    _conversionReady = false;
    _conversionStartMs = 0;
    _pendingMode = Mode::POWER_DOWN;
    return Status::Ok();
  }

  _config.mode = Mode::POWER_DOWN;
  _sampleAvailable = false;
  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = _nowMs();
  _pendingMode = mode;
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

Status OPT4001::readIntConfiguration(uint16_t& value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return _readRegister16Tracked(cmd::REG_INT_CONFIGURATION, value);
}

Status OPT4001::readIntConfiguration(IntConfigurationInfo& out) {
  uint16_t raw = 0;
  Status st = readIntConfiguration(raw);
  if (!st.ok()) {
    return st;
  }
  decodeIntConfiguration(raw, out);
  return Status::Ok();
}

Status OPT4001::writeIntConfiguration(uint16_t value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if ((value & cmd::MASK_INTCFG_FIXED) != cmd::INTCFG_FIXED_BITS) {
    return Status::Error(Err::INVALID_PARAM, "INT configuration fixed pattern mismatch");
  }
  if ((value & cmd::MASK_INTCFG_RESERVED) != 0) {
    return Status::Error(Err::INVALID_PARAM, "INT configuration reserved bit must be 0");
  }

  const IntDirection direction =
      static_cast<IntDirection>((value & cmd::MASK_INT_DIR) >> cmd::BIT_INT_DIR);
  const IntConfig config =
      static_cast<IntConfig>((value & cmd::MASK_INT_CFG) >> cmd::BIT_INT_CFG);
  if (!isValidIntDirection(direction) || !isValidIntConfig(config)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid INT configuration value");
  }

  Status st = _writeRegister16Tracked(cmd::REG_INT_CONFIGURATION, value);
  if (!st.ok()) {
    return st;
  }

  _config.intDirection = direction;
  _config.intConfig = config;
  _config.burstMode = (value & cmd::MASK_I2C_BURST) != 0;
  return Status::Ok();
}

// ============================================================================
// Raw Register Access
// ============================================================================

Status OPT4001::readRegisters(uint8_t startReg, uint8_t* buf, size_t len) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (buf == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register block buffer");
  }
  if (!isValidPublicRegisterBlock(startReg, len)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register block");
  }
  Status st = Status::Ok();
  if (_config.burstMode || len <= 2U) {
    st = _i2cWriteReadTracked(&startReg, 1, buf, len);
  } else {
    size_t offset = 0;
    while (offset < len) {
      const uint8_t reg = static_cast<uint8_t>(
          static_cast<size_t>(startReg) + (offset / 2U));
      uint16_t value = 0;
      st = _readRegister16Tracked(reg, value);
      if (!st.ok()) {
        return st;
      }
      buf[offset++] = static_cast<uint8_t>(value >> 8);
      if (offset < len) {
        buf[offset++] = static_cast<uint8_t>(value & 0xFFU);
      }
    }
  }
  if (st.ok()) {
    const size_t registerSpan = (len - 1U) / 2U;
    const uint16_t endReg = static_cast<uint16_t>(startReg) +
                            static_cast<uint16_t>(registerSpan);
    if (startReg <= cmd::REG_FLAGS && endReg >= cmd::REG_FLAGS) {
      _clearReadinessEvidence();
    }
  }
  return st;
}

Status OPT4001::readRegister16(uint8_t reg, uint16_t& value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status st = _readRegister16Tracked(reg, value);
  if (st.ok() && reg == cmd::REG_FLAGS) {
    _clearReadinessEvidence();
  }
  return st;
}

Status OPT4001::writeRegister16(uint8_t reg, uint16_t value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status st = _writeRegister16Tracked(reg, value);
  if (st.ok() && reg == cmd::REG_FLAGS && value != 0U) {
    _clearReadinessEvidence();
  }
  if (rawWriteCanDirtyCachedSettings(reg) &&
      st.code != Err::INVALID_PARAM &&
      st.code != Err::OFFLINE &&
      st.code != Err::BUSY) {
    if (st.ok()) {
      if (!_hardwareConfigDirty) {
        _hardwareConfigDirty = true;
        _hardwareConfigDirtyError = Status::Ok();
      }
    } else {
      _markHardwareConfigDirty(st);
    }
  }
  return st;
}

Status OPT4001::_readRegister16Tracked(uint8_t reg, uint16_t& value) {
  if (!isValidPublicRegisterAddress(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register address");
  }
  uint8_t rx[2] = {0, 0};
  Status st = _i2cWriteReadTracked(&reg, 1, rx, sizeof(rx));
  if (!st.ok()) {
    return st;
  }
  value = static_cast<uint16_t>((rx[0] << 8) | rx[1]);
  return Status::Ok();
}

Status OPT4001::_writeRegister16Tracked(uint8_t reg, uint16_t value) {
  if (!isValidPublicRegisterAddress(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register address");
  }
  const uint8_t tx[3] = {
    reg,
    static_cast<uint8_t>((value >> 8) & 0xFF),
    static_cast<uint8_t>(value & 0xFF)
  };
  return _i2cWriteTracked(tx, sizeof(tx));
}

// ============================================================================
// Utility
// ============================================================================

void OPT4001::decodeDeviceId(uint16_t raw, DeviceIdInfo& out) const {
  out.raw = raw;
  out.didh = static_cast<uint16_t>(raw & cmd::MASK_DIDH);
  out.didl = static_cast<uint8_t>((raw & cmd::MASK_DIDL) >> cmd::BIT_DIDL);
  out.reservedBitsClear = (raw & cmd::MASK_DEVICE_ID_RESERVED) == 0U;
  out.matchesExpected = out.reservedBitsClear &&
                        (out.didh == cmd::DIDH_EXPECTED) &&
                        (out.didl == cmd::DIDL_EXPECTED);
}

Status OPT4001::_validateDeviceId(uint16_t raw) const {
  DeviceIdInfo info;
  decodeDeviceId(raw, info);
  if (!info.matchesExpected) {
    return Status::Error(Err::DEVICE_ID_MISMATCH, "Unexpected device ID", raw);
  }
  return Status::Ok();
}

Status OPT4001::_pollConversionReadyFlag(bool& ready) {
  ready = false;
  Flags flags;
  Status st = readFlags(flags);
  if (!st.ok()) {
    return st;
  }
  ready = flags.conversionReady;
  return Status::Ok();
}

Status OPT4001::_refreshReadinessEvidence(bool& ready) {
  ready = false;
  if (_conversionReady || _sampleAvailable) {
    ready = true;
    return Status::Ok();
  }

  bool evidence = _intFreshEvidenceAsserted();
  Status st = Status::Ok();

  if (!evidence && _shouldProbeCounterForFreshness()) {
    uint16_t lsbCrc = 0;
    st = _readRegister16Tracked(cmd::REG_RESULT_LSB_CRC, lsbCrc);
    if (!st.ok()) {
      return st;
    }
    const uint8_t counter =
        static_cast<uint8_t>((lsbCrc & cmd::MASK_COUNTER) >> cmd::BIT_COUNTER);
    evidence = _sampleCounterIsFresh(counter);
  }

  if (!evidence) {
    bool flagReady = false;
    st = _pollConversionReadyFlag(flagReady);
    if (!st.ok()) {
      return st;
    }
    evidence = flagReady;
  }

  if (evidence) {
    _sampleAvailable = true;
    _conversionReady = true;
    if (_config.mode != Mode::CONTINUOUS) {
      _conversionStarted = false;
      _pendingMode = Mode::POWER_DOWN;
    }
    ready = true;
  }
  return Status::Ok();
}

bool OPT4001::_intFreshEvidenceAsserted() const {
  if (_config.packageVariant != PackageVariant::SOT_5X3 ||
      _config.intPin < 0 ||
      _config.gpioRead == nullptr ||
      _config.intDirection != IntDirection::PIN_OUTPUT) {
    return false;
  }
  if (_config.intConfig != IntConfig::EVERY_CONVERSION &&
      _config.intConfig != IntConfig::FIFO_FULL) {
    return false;
  }

  const bool levelHigh = _config.gpioRead(_config.intPin, _config.gpioUser);
  return (_config.interruptPolarity == InterruptPolarity::ACTIVE_HIGH)
             ? levelHigh
             : !levelHigh;
}

bool OPT4001::_oneShotBudgetElapsed() const {
  return _conversionStarted &&
         (static_cast<uint32_t>(_nowMs() - _conversionStartMs) >=
          getOneShotBudgetMs(_pendingMode));
}

bool OPT4001::_shouldProbeCounterForFreshness() const {
  if (!_lastFreshCounterValid) {
    return false;
  }
  if (_config.mode == Mode::CONTINUOUS) {
    return static_cast<uint32_t>(_nowMs() - _conversionStartMs) >= getConversionTimeMs();
  }
  return _oneShotBudgetElapsed();
}

bool OPT4001::_sampleCounterIsFresh(uint8_t counter) const {
  return !_lastFreshCounterValid ||
         ((counter & (cmd::SAMPLE_COUNT_MODULO - 1U)) !=
          (_lastFreshCounter & (cmd::SAMPLE_COUNT_MODULO - 1U)));
}

void OPT4001::_markFreshSampleConsumed(const Sample& sample) {
  _markFreshSampleConsumedAt(sample, _nowMs());
}

void OPT4001::_clearReadinessEvidence() {
  _sampleAvailable = false;
  _conversionReady = false;
}

void OPT4001::decodeConfiguration(uint16_t raw, ConfigurationInfo& out) const {
  out.raw = raw;
  out.quickWake = (raw & cmd::MASK_QWAKE) != 0;
  out.reservedBitSet = (raw & cmd::MASK_CONFIGURATION_RESERVED) != 0;
  out.range = static_cast<Range>((raw & cmd::MASK_RANGE) >> cmd::BIT_RANGE);
  out.conversionTime =
      static_cast<ConversionTime>((raw & cmd::MASK_CONVERSION_TIME) >> cmd::BIT_CONVERSION_TIME);
  out.mode = static_cast<Mode>((raw & cmd::MASK_MODE) >> cmd::BIT_MODE);
  out.interruptLatch =
      static_cast<InterruptLatch>((raw & cmd::MASK_LATCH) >> cmd::BIT_LATCH);
  out.interruptPolarity =
      static_cast<InterruptPolarity>((raw & cmd::MASK_INT_POL) >> cmd::BIT_INT_POL);
  out.faultCount =
      static_cast<FaultCount>((raw & cmd::MASK_FAULT_COUNT) >> cmd::BIT_FAULT_COUNT);
  out.valid = !out.reservedBitSet &&
              isValidRange(out.range) &&
              isValidConversionTime(out.conversionTime) &&
              isValidMode(out.mode) &&
              isValidInterruptLatch(out.interruptLatch) &&
              isValidInterruptPolarity(out.interruptPolarity) &&
              isValidFaultCount(out.faultCount);
}

void OPT4001::decodeIntConfiguration(uint16_t raw, IntConfigurationInfo& out) const {
  out.raw = raw;
  out.fixedPatternValid = (raw & cmd::MASK_INTCFG_FIXED) == cmd::INTCFG_FIXED_BITS;
  out.reservedBitSet = (raw & cmd::MASK_INTCFG_RESERVED) != 0;
  out.intDirection =
      static_cast<IntDirection>((raw & cmd::MASK_INT_DIR) >> cmd::BIT_INT_DIR);
  out.intConfig =
      static_cast<IntConfig>((raw & cmd::MASK_INT_CFG) >> cmd::BIT_INT_CFG);
  out.burstMode = (raw & cmd::MASK_I2C_BURST) != 0;
  out.valid = out.fixedPatternValid &&
              !out.reservedBitSet &&
              isValidIntDirection(out.intDirection) &&
              isValidIntConfig(out.intConfig);
}

float OPT4001::adcCodesToLux(uint32_t adcCodes) const {
  return adcCodesToLux64(adcCodes, getLuxLsb());
}

Status OPT4001::rawToAdcCodes(uint8_t exponent, uint32_t mantissa,
                              uint64_t& adcCodes) const {
  adcCodes = 0;
  if (exponent > RESULT_EXPONENT_MAX) {
    return Status::Error(Err::INVALID_PARAM, "Result exponent out of range", exponent);
  }
  if (mantissa > RESULT_MANTISSA_MAX) {
    const int32_t detail =
        (mantissa > static_cast<uint32_t>(INT32_MAX))
            ? INT32_MAX
            : static_cast<int32_t>(mantissa);
    return Status::Error(Err::INVALID_PARAM, "Result mantissa exceeds 20 bits",
                         detail);
  }

  adcCodes = static_cast<uint64_t>(mantissa) << exponent;
  return Status::Ok();
}

Status OPT4001::rawToLux(uint8_t exponent, uint32_t mantissa, float& lux) const {
  uint64_t adcCodes = 0;
  Status st = rawToAdcCodes(exponent, mantissa, adcCodes);
  if (!st.ok()) {
    lux = invalidLuxValue();
    return st;
  }

  lux = adcCodesToLux64(adcCodes, getLuxLsb());
  return Status::Ok();
}

float OPT4001::rawToLux(uint8_t exponent, uint32_t mantissa) const {
  float lux = invalidLuxValue();
  (void)rawToLux(exponent, mantissa, lux);
  return lux;
}

float OPT4001::thresholdToLux(const Threshold& threshold) const {
  uint64_t adcCodes = 0;
  Status st = thresholdToAdcCodes(threshold, adcCodes);
  if (!st.ok()) {
    return invalidLuxValue();
  }
  return adcCodesToLux64(adcCodes, getLuxLsb());
}

float OPT4001::getLuxLsb() const {
  return (_config.packageVariant == PackageVariant::PICOSTAR)
             ? cmd::LUX_LSB_PICOSTAR
             : cmd::LUX_LSB_SOT_5X3;
}

float OPT4001::getRangeFullScaleLux(Range range) const {
  if (!isValidRange(range)) {
    return invalidLuxValue();
  }
  uint8_t index = 8U;
  if (range != Range::AUTO) {
    const uint8_t value = static_cast<uint8_t>(range);
    if (value <= 8U) {
      index = value;
    }
  }
  return (_config.packageVariant == PackageVariant::PICOSTAR)
             ? cmd::RANGE_FULL_SCALE_LUX_PICOSTAR[index]
             : cmd::RANGE_FULL_SCALE_LUX_SOT_5X3[index];
}

float OPT4001::getCurrentFullScaleLux() const {
  return getRangeFullScaleLux(_config.range);
}

float OPT4001::getSampleFullScaleLux(const Sample& sample) const {
  if (sample.exponent > RESULT_EXPONENT_MAX) {
    return invalidLuxValue();
  }
  return getRangeFullScaleLux(static_cast<Range>(sample.exponent));
}

uint8_t OPT4001::getEffectiveBits(ConversionTime time) const {
  if (!isValidConversionTime(time)) {
    return 0;
  }
  return cmd::CONVERSION_EFFECTIVE_BITS[static_cast<uint8_t>(time)];
}

uint8_t OPT4001::getEffectiveBits() const {
  return getEffectiveBits(_config.conversionTime);
}

float OPT4001::getRangeResolutionLux(Range range, ConversionTime time) const {
  if (!isValidRange(range) || !isValidConversionTime(time)) {
    return invalidLuxValue();
  }
  const uint8_t effectiveBits = getEffectiveBits(time);

  uint8_t exponent = 8U;
  if (range != Range::AUTO) {
    const uint8_t value = static_cast<uint8_t>(range);
    if (value <= 8U) {
      exponent = value;
    }
  }

  const uint8_t paddedBits = static_cast<uint8_t>(20U - effectiveBits);
  const uint64_t adcStep = 1ULL << (paddedBits + exponent);
  return adcCodesToLux64(adcStep, getLuxLsb());
}

float OPT4001::getCurrentResolutionLux() const {
  return getRangeResolutionLux(_config.range, _config.conversionTime);
}

float OPT4001::getSampleResolutionLux(const Sample& sample) const {
  if (sample.exponent > RESULT_EXPONENT_MAX) {
    return invalidLuxValue();
  }
  return getRangeResolutionLux(static_cast<Range>(sample.exponent),
                               _config.conversionTime);
}

uint32_t OPT4001::getConversionTimeUs() const {
  const uint8_t index = static_cast<uint8_t>(_config.conversionTime);
  return cmd::CONVERSION_TIME_US[index];
}

uint32_t OPT4001::getConversionTimeMs() const {
  const uint8_t index = static_cast<uint8_t>(_config.conversionTime);
  return cmd::CONVERSION_TIME_MS_CEIL[index];
}

uint32_t OPT4001::getOneShotBudgetUs(Mode mode) const {
  if (!isOneShotMode(mode)) {
    return 0U;
  }
  uint32_t budgetUs = getConversionTimeUs();
  if (!_config.quickWake) {
    budgetUs += cmd::ONE_SHOT_STANDBY_US;
  }
  if (mode == Mode::ONE_SHOT_FORCED_AUTO) {
    budgetUs += cmd::FORCED_AUTO_RANGE_EXTRA_US;
  }
  return budgetUs;
}

uint32_t OPT4001::getOneShotBudgetMs(Mode mode) const {
  return ceilUsToMs(getOneShotBudgetUs(mode));
}

Status OPT4001::luxToThreshold(float lux, Threshold& out) const {
  if (!std::isfinite(lux) || lux < 0.0f) {
    return Status::Error(Err::INVALID_PARAM, "Lux threshold must be finite and >= 0");
  }

  const float lsb = getLuxLsb();
  double scaledCodes = 0.0;
  if (lsb > 0.0f) {
    scaledCodes = static_cast<double>(lux) / static_cast<double>(lsb);
    if (scaledCodes > static_cast<double>(THRESHOLD_ADC_CODES_MAX) +
                          THRESHOLD_ADC_FLOAT_GUARD_CODES) {
      return Status::Error(Err::INVALID_PARAM, "Lux threshold exceeds register range");
    }
  }

  double bestError = std::numeric_limits<double>::infinity();
  for (uint8_t exponent = 0; exponent <= cmd::THRESHOLD_EXPONENT_MAX; ++exponent) {
    const uint8_t shift = static_cast<uint8_t>(THRESHOLD_SHIFT_BASE + exponent);
    const uint64_t quantum = 1ULL << shift;
    uint64_t result = static_cast<uint64_t>(
        (scaledCodes / static_cast<double>(quantum)) + 0.5);
    if (result > cmd::THRESHOLD_RESULT_MAX) {
      result = cmd::THRESHOLD_RESULT_MAX;
    }
    const double representedCodes = static_cast<double>(result * quantum);
    const double error = std::fabs(representedCodes - scaledCodes);
    if (error < bestError) {
      bestError = error;
      out.exponent = exponent;
      out.result = static_cast<uint16_t>(result);
    }
  }
  return Status::Ok();
}

Status OPT4001::thresholdToAdcCodes(const Threshold& threshold,
                                    uint64_t& adcCodes) const {
  adcCodes = 0;
  if (!_thresholdValid(threshold)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid threshold");
  }

  adcCodes = static_cast<uint64_t>(threshold.result) <<
             (THRESHOLD_SHIFT_BASE + threshold.exponent);
  return Status::Ok();
}

uint32_t OPT4001::thresholdToAdcCodes(const Threshold& threshold) const {
  uint64_t adcCodes = 0;
  Status st = thresholdToAdcCodes(threshold, adcCodes);
  if (!st.ok() || adcCodes > static_cast<uint64_t>(UINT32_MAX)) {
    return UINT32_MAX;
  }
  return static_cast<uint32_t>(adcCodes);
}

uint8_t OPT4001::sampleCounterDelta(uint8_t previousCounter, uint8_t currentCounter) const {
  return static_cast<uint8_t>((currentCounter - previousCounter) &
                              (cmd::SAMPLE_COUNT_MODULO - 1U));
}

// ============================================================================
// Transport Wrappers
// ============================================================================

Status OPT4001::_i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                                 uint8_t* rxBuf, size_t rxLen) {
  if (_config.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C read callback missing");
  }
  if (txBuf == nullptr || txLen == 0 || rxBuf == nullptr || rxLen == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C read parameters");
  }
  return _config.i2cWriteRead(_config.i2cAddress, txBuf, txLen,
                              rxBuf, rxLen, _config.i2cTimeoutMs,
                              _config.i2cUser);
}

Status OPT4001::_i2cWriteRawAddr(uint8_t addr, const uint8_t* buf, size_t len) {
  if (_config.i2cWrite == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback missing");
  }
  if (buf == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C write parameters");
  }
  return _config.i2cWrite(addr, buf, len,
                          _config.i2cTimeoutMs, _config.i2cUser);
}

Status OPT4001::_i2cWriteRaw(const uint8_t* buf, size_t len) {
  return _i2cWriteRawAddr(_config.i2cAddress, buf, len);
}

Status OPT4001::_i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                                     uint8_t* rxBuf, size_t rxLen) {
  if (_initialized && _driverState == DriverState::OFFLINE && !_allowOfflineI2c) {
    return offlineStatus();
  }
  if (_pollJob != PollJob::NONE && !_pollExecuting) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }
  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

Status OPT4001::_i2cWriteTracked(const uint8_t* buf, size_t len) {
  if (_initialized && _driverState == DriverState::OFFLINE && !_allowOfflineI2c) {
    return offlineStatus();
  }
  if (_pollJob != PollJob::NONE && !_pollExecuting) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }
  Status st = _i2cWriteRaw(buf, len);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

Status OPT4001::_i2cWriteTrackedAddr(uint8_t addr, const uint8_t* buf, size_t len) {
  if (_initialized && _driverState == DriverState::OFFLINE && !_allowOfflineI2c) {
    return offlineStatus();
  }
  if (_pollJob != PollJob::NONE && !_pollExecuting) {
    return Status::Error(Err::BUSY, "Poll job already active");
  }
  Status st = _i2cWriteRawAddr(addr, buf, len);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

// ============================================================================
// Register Access
// ============================================================================

Status OPT4001::_readRegister16Raw(uint8_t reg, uint16_t& value) {
  uint8_t rx[2] = {0, 0};
  Status st = _i2cWriteReadRaw(&reg, 1, rx, sizeof(rx));
  if (!st.ok()) {
    return st;
  }
  value = static_cast<uint16_t>((rx[0] << 8) | rx[1]);
  return Status::Ok();
}

Status OPT4001::_readSampleAt(uint8_t msbReg, Sample& out) {
  if (_config.burstMode) {
    uint8_t buffer[4] = {};
    Status st = _i2cWriteReadTracked(&msbReg, 1, buffer, sizeof(buffer));
    if (!st.ok()) {
      return st;
    }
    const uint16_t result =
        static_cast<uint16_t>((static_cast<uint16_t>(buffer[0]) << 8) | buffer[1]);
    const uint16_t lsbCrc =
        static_cast<uint16_t>((static_cast<uint16_t>(buffer[2]) << 8) | buffer[3]);
    return _decodeSampleRegisters(result, lsbCrc, out);
  }

  uint16_t result = 0;
  uint16_t lsbCrc = 0;
  Status st = _readRegister16Tracked(msbReg, result);
  if (!st.ok()) {
    return st;
  }
  st = _readRegister16Tracked(static_cast<uint8_t>(msbReg + 1U), lsbCrc);
  if (!st.ok()) {
    return st;
  }
  return _decodeSampleRegisters(result, lsbCrc, out);
}

Status OPT4001::_readBurstBlockTracked(BurstFrame& out) {
  const uint8_t reg = cmd::REG_RESULT;
  uint8_t buffer[16] = {};
  Status st = _i2cWriteReadTracked(&reg, 1, buffer, sizeof(buffer));
  if (!st.ok()) {
    return st;
  }

  const uint16_t regs[8] = {
    static_cast<uint16_t>((buffer[0] << 8) | buffer[1]),
    static_cast<uint16_t>((buffer[2] << 8) | buffer[3]),
    static_cast<uint16_t>((buffer[4] << 8) | buffer[5]),
    static_cast<uint16_t>((buffer[6] << 8) | buffer[7]),
    static_cast<uint16_t>((buffer[8] << 8) | buffer[9]),
    static_cast<uint16_t>((buffer[10] << 8) | buffer[11]),
    static_cast<uint16_t>((buffer[12] << 8) | buffer[13]),
    static_cast<uint16_t>((buffer[14] << 8) | buffer[15]),
  };

  Status aggregate = Status::Ok();
  Status slotStatus = _decodeSampleRegisters(regs[0], regs[1], out.newest);
  (void)mergeSampleDecodeStatus(aggregate, slotStatus);
  slotStatus = _decodeSampleRegisters(regs[2], regs[3], out.fifo0);
  (void)mergeSampleDecodeStatus(aggregate, slotStatus);
  slotStatus = _decodeSampleRegisters(regs[4], regs[5], out.fifo1);
  (void)mergeSampleDecodeStatus(aggregate, slotStatus);
  slotStatus = _decodeSampleRegisters(regs[6], regs[7], out.fifo2);
  (void)mergeSampleDecodeStatus(aggregate, slotStatus);
  return aggregate;
}

Status OPT4001::_decodeSampleRegisters(uint16_t resultReg, uint16_t lsbCrcReg,
                                       Sample& out) const {
  out.resultReg = resultReg;
  out.resultLsbCrcReg = lsbCrcReg;
  out.exponent = static_cast<uint8_t>((resultReg & cmd::MASK_EXPONENT) >> cmd::BIT_EXPONENT);
  out.mantissa = (static_cast<uint32_t>(resultReg & cmd::MASK_RESULT_MSB) << 8) |
                 static_cast<uint32_t>((lsbCrcReg & cmd::MASK_RESULT_LSB) >> cmd::BIT_RESULT_LSB);
  out.counter = static_cast<uint8_t>((lsbCrcReg & cmd::MASK_COUNTER) >> cmd::BIT_COUNTER);
  out.crc = static_cast<uint8_t>(lsbCrcReg & cmd::MASK_CRC);
  out.crcValid = (_computeCrcNibble(out.exponent, out.mantissa, out.counter) == out.crc);

  uint64_t adcCodes = 0;
  Status numericStatus = rawToAdcCodes(out.exponent, out.mantissa, adcCodes);
  if (!numericStatus.ok() || adcCodes > static_cast<uint64_t>(UINT32_MAX)) {
    out.adcCodes = 0;
    out.lux = invalidLuxValue();
    return numericStatus.ok()
               ? Status::Error(Err::INVALID_PARAM, "Result ADC codes out of range")
               : numericStatus;
  }
  out.adcCodes = static_cast<uint32_t>(adcCodes);
  out.lux = adcCodesToLux64(adcCodes, getLuxLsb());

  if (_config.verifyCrc && !out.crcValid) {
    return Status::Error(Err::CRC_ERROR, "Sample CRC mismatch", out.crc);
  }
  return Status::Ok();
}

// ============================================================================
// Health Tracking
// ============================================================================

Status OPT4001::_updateHealth(const Status& st) {
  if (!_initialized) {
    return st;
  }
  if (st.inProgress()) {
    return st;
  }

  const uint32_t nowMs = _nowMs();

  if (st.ok()) {
    _lastOkMs = nowMs;
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      _totalSuccess++;
    }
    _driverState = DriverState::READY;
    return st;
  }

  _lastErrorMs = nowMs;
  _lastError = st;
  if (_consecutiveFailures < UINT8_MAX) {
    _consecutiveFailures++;
  }
  if (_totalFailures < UINT32_MAX) {
    _totalFailures++;
  }
  _driverState = (_consecutiveFailures >= _config.offlineThreshold)
                     ? DriverState::OFFLINE
                     : DriverState::DEGRADED;
  return st;
}

Status OPT4001::_recordFailure(const Status& st) {
  if (st.ok() || st.inProgress() ||
      st.code == Err::INVALID_CONFIG ||
      st.code == Err::INVALID_PARAM ||
      st.code == Err::NOT_INITIALIZED) {
    return st;
  }

  const uint32_t nowMs = _nowMs();
  _lastErrorMs = nowMs;
  _lastError = st;
  if (_consecutiveFailures < UINT8_MAX) {
    _consecutiveFailures++;
  }
  if (_totalFailures < UINT32_MAX) {
    _totalFailures++;
  }
  if (_initialized) {
    _driverState = (_consecutiveFailures >= _config.offlineThreshold)
                       ? DriverState::OFFLINE
                       : DriverState::DEGRADED;
  }
  return st;
}

void OPT4001::_reassertOfflineLatch() {
  _driverState = DriverState::OFFLINE;
  const uint8_t threshold = _config.offlineThreshold == 0 ? 1 : _config.offlineThreshold;
  if (_consecutiveFailures < threshold) {
    _consecutiveFailures = threshold;
  }
}

// ============================================================================
// Internal
// ============================================================================

Status OPT4001::_applyConfig() {
  uint8_t writesApplied = 0;
  Status st = _writeRegister16Tracked(cmd::REG_THRESHOLD_L, _packThreshold(_config.lowThreshold));
  if (!st.ok()) {
    return st;
  }
  ++writesApplied;
  st = _writeRegister16Tracked(cmd::REG_THRESHOLD_H, _packThreshold(_config.highThreshold));
  if (!st.ok()) {
    if (writesApplied > 0U) {
      _markHardwareConfigDirty(st);
    }
    return st;
  }
  ++writesApplied;
  st = _writeRegister16Tracked(cmd::REG_INT_CONFIGURATION, _buildIntConfigurationRegister());
  if (!st.ok()) {
    if (writesApplied > 0U) {
      _markHardwareConfigDirty(st);
    }
    return st;
  }
  ++writesApplied;
  st = _writeRegister16Tracked(cmd::REG_CONFIGURATION, _buildConfigurationRegister(_config.mode));
  if (!st.ok()) {
    if (writesApplied > 0U) {
      _markHardwareConfigDirty(st);
    }
    return st;
  }
  ++writesApplied;

  _finishApplyConfig(_nowMs());
  return Status::Ok();
}

Status OPT4001::_pollReadJob(uint32_t nowMs, uint8_t& remainingInstructions) {
  while (true) {
    if (_pollStep == PollStep::WAIT_READY) {
      if (_conversionReady || _sampleAvailable || _intFreshEvidenceAsserted()) {
        _sampleAvailable = true;
        _conversionReady = true;
        if (_config.mode != Mode::CONTINUOUS) {
          _conversionStarted = false;
          _pendingMode = Mode::POWER_DOWN;
        }
        _pollStep = PollStep::READ_BURST_BLOCK;
        continue;
      }
      if (!_pollReadGateElapsed(nowMs)) {
        return _pollInProgressStatus("Waiting for conversion gate");
      }
      _pollStep = PollStep::READ_FLAGS;
      continue;
    }

    if (_pollStep == PollStep::READ_FLAGS) {
      if (remainingInstructions == 0U) {
        return _pollInProgressStatus("Poll instruction budget exhausted");
      }
      Flags flags;
      Status st = readFlags(flags);
      --remainingInstructions;
      if (!st.ok()) {
        return _finishPollJob(st);
      }
      if (flags.conversionReady) {
        _pollStep = PollStep::READ_BURST_BLOCK;
        continue;
      }
      if (_shouldProbeCounterForFreshness(nowMs)) {
        _pollStep = PollStep::READ_COUNTER;
        continue;
      }
      return _pollInProgressStatus("Measurement not ready");
    }

    if (_pollStep == PollStep::READ_COUNTER) {
      if (remainingInstructions == 0U) {
        return _pollInProgressStatus("Poll instruction budget exhausted");
      }
      uint16_t lsbCrc = 0;
      Status st = _readRegister16Tracked(cmd::REG_RESULT_LSB_CRC, lsbCrc);
      --remainingInstructions;
      if (!st.ok()) {
        return _finishPollJob(st);
      }
      const uint8_t counter =
          static_cast<uint8_t>((lsbCrc & cmd::MASK_COUNTER) >> cmd::BIT_COUNTER);
      if (!_sampleCounterIsFresh(counter)) {
        _pollStep = PollStep::READ_FLAGS;
        return _pollInProgressStatus("Measurement not ready");
      }
      _sampleAvailable = true;
      _conversionReady = true;
      _pollStep = PollStep::READ_BURST_BLOCK;
      continue;
    }

    if (_pollStep == PollStep::READ_BURST_BLOCK) {
      if (remainingInstructions == 0U) {
        return _pollInProgressStatus("Poll instruction budget exhausted");
      }
      Status st = _readBurstBlockTracked(_pollBurst);
      --remainingInstructions;
      if (!st.ok() && st.code != Err::CRC_ERROR) {
        return _finishPollJob(st);
      }
      if (!_sampleCounterIsFresh(_pollBurst.newest.counter)) {
        _clearReadinessEvidence();
        return _finishPollJob(
            Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready"));
      }

      if (_pollJob == PollJob::READ_BURST) {
        _lastBurst = _pollBurst;
        _lastBurstValid = true;
      }
      _markFreshSampleConsumedAt(_pollBurst.newest, nowMs);
      return _finishPollJob(st);
    }

    return _finishPollJob(Status::Error(Err::INVALID_CONFIG, "Invalid poll read state"));
  }
}

Status OPT4001::_pollConfigJob(uint32_t nowMs, uint8_t& remainingInstructions) {
  const bool resetJob = (_pollJob == PollJob::RESET_AND_REAPPLY);
  const bool attachJob = (_pollJob == PollJob::ATTACH);
  ScopedOfflineI2cAllowance allowOfflineI2c(
      _allowOfflineI2c, resetJob || _allowOfflineI2c);

  while (remainingInstructions > 0U) {
    Status st = Status::Ok();

    if (_pollStep == PollStep::WRITE_THRESHOLD_L) {
      st = _writeRegister16Tracked(cmd::REG_THRESHOLD_L,
                                   _packThreshold(_pollTargetConfig.lowThreshold));
      --remainingInstructions;
      if (!st.ok()) {
        if (resetJob && _pollResetApplied) {
          _markHardwareConfigDirty(st);
        }
        return _finishPollJob(st);
      }
      ++_pollWritesApplied;
      _pollStep = PollStep::WRITE_THRESHOLD_H;
      continue;
    }

    if (_pollStep == PollStep::WRITE_THRESHOLD_H) {
      st = _writeRegister16Tracked(cmd::REG_THRESHOLD_H,
                                   _packThreshold(_pollTargetConfig.highThreshold));
      --remainingInstructions;
      if (!st.ok()) {
        if (_pollWritesApplied > 0U) {
          _markHardwareConfigDirty(st);
        }
        return _finishPollJob(st);
      }
      ++_pollWritesApplied;
      _pollStep = PollStep::WRITE_INT_CONFIG;
      continue;
    }

    if (_pollStep == PollStep::WRITE_INT_CONFIG) {
      st = _writeRegister16Tracked(cmd::REG_INT_CONFIGURATION,
                                   _buildIntConfigurationRegister(_pollTargetConfig));
      --remainingInstructions;
      if (!st.ok()) {
        if (_pollWritesApplied > 0U) {
          _markHardwareConfigDirty(st);
        }
        return _finishPollJob(st);
      }
      ++_pollWritesApplied;
      _pollStep = PollStep::WRITE_CONFIG;
      continue;
    }

    if (_pollStep == PollStep::WRITE_CONFIG) {
      st = _writeRegister16Tracked(cmd::REG_CONFIGURATION,
                                   _buildConfigurationRegister(_pollTargetConfig,
                                                               _pollTargetConfig.mode));
      --remainingInstructions;
      if (!st.ok()) {
        if (_pollWritesApplied > 0U) {
          _markHardwareConfigDirty(st);
        }
        return _finishPollJob(st);
      }
      ++_pollWritesApplied;
      _config = _pollTargetConfig;
      _finishApplyConfig(nowMs);
      if (attachJob) {
        _initialized = true;
        _driverState = DriverState::READY;
        _consecutiveFailures = 0;
      }
      return _finishPollJob(Status::Ok());
    }

    return _finishPollJob(Status::Error(Err::INVALID_CONFIG, "Invalid poll config state"));
  }

  return _pollInProgressStatus("Poll instruction budget exhausted");
}

Status OPT4001::_pollResetAndReapplyJob(uint32_t nowMs,
                                        uint8_t& remainingInstructions) {
  while (remainingInstructions > 0U) {
    if (_pollStep == PollStep::WRITE_RESET) {
      const uint8_t payload[1] = {cmd::GENERAL_CALL_RESET};
      ScopedOfflineI2cAllowance allowOfflineI2c(_allowOfflineI2c, true);
      Status st = _i2cWriteTrackedAddr(cmd::GENERAL_CALL_ADDRESS, payload, sizeof(payload));
      --remainingInstructions;
      if (!st.ok()) {
        return _finishPollJob(st);
      }
      _pollResetApplied = true;
      _clearRuntimeState();
      _pollStep = PollStep::WRITE_THRESHOLD_L;
      continue;
    }

    const bool resetApplied = _pollResetApplied;
    const Status st = _pollConfigJob(nowMs, remainingInstructions);
    if (_pollJob == PollJob::NONE || !st.inProgress()) {
      if (!st.ok() && resetApplied) {
        _markHardwareConfigDirty(st);
        _initialized = false;
        _driverState = DriverState::UNINIT;
      }
      if (st.ok()) {
        _initialized = true;
        _driverState = DriverState::READY;
        _consecutiveFailures = 0;
      }
      return st;
    }
    return st;
  }

  return _pollInProgressStatus("Poll instruction budget exhausted");
}

Status OPT4001::_finishPollJob(const Status& st) {
  _pollJob = PollJob::NONE;
  _pollStep = PollStep::IDLE;
  _pollWritesApplied = 0;
  _pollResetApplied = false;
  _pollExecuting = false;
  _lastPollStatus = st;
  return st;
}

Status OPT4001::_pollInProgressStatus(const char* message) const {
  return Status{Err::IN_PROGRESS, 0, message};
}

bool OPT4001::_pollReadGateElapsed(uint32_t nowMs) const {
  if (_config.mode == Mode::CONTINUOUS) {
    return static_cast<uint32_t>(nowMs - _conversionStartMs) >= _pollContinuousGateMs();
  }
  if (_conversionStarted) {
    return static_cast<uint32_t>(nowMs - _conversionStartMs) >=
           getOneShotBudgetMs(_pendingMode);
  }
  return true;
}

uint32_t OPT4001::_pollContinuousGateMs() const {
  const uint32_t conversionMs = getConversionTimeMs();
  if (_pollJob == PollJob::READ_BURST &&
      _config.intConfig == IntConfig::FIFO_FULL) {
    return conversionMs * cmd::SAMPLE_SLOT_COUNT;
  }
  return conversionMs;
}

bool OPT4001::_shouldProbeCounterForFreshness(uint32_t nowMs) const {
  if (!_lastFreshCounterValid) {
    return false;
  }
  if (_config.mode == Mode::CONTINUOUS) {
    return static_cast<uint32_t>(nowMs - _conversionStartMs) >= _pollContinuousGateMs();
  }
  if (_conversionStarted) {
    return static_cast<uint32_t>(nowMs - _conversionStartMs) >=
           getOneShotBudgetMs(_pendingMode);
  }
  return true;
}

void OPT4001::_finishApplyConfig(uint32_t nowMs) {
  _clearHardwareConfigDirty();
  _clearRuntimeState();

  if (_config.mode == Mode::CONTINUOUS) {
    _conversionStarted = true;
    _conversionStartMs = nowMs;
  } else {
    _conversionStarted = false;
    _conversionStartMs = 0;
  }
}

void OPT4001::_markHardwareConfigDirty(const Status& st) {
  if (st.ok() || st.inProgress()) {
    return;
  }
  if (_hardwareConfigDirty) {
    return;
  }
  _hardwareConfigDirty = true;
  _hardwareConfigDirtyError = st;
}

void OPT4001::_clearHardwareConfigDirty() {
  _hardwareConfigDirty = false;
  _hardwareConfigDirtyError = Status::Ok();
}

void OPT4001::_clearRuntimeState() {
  _sampleAvailable = false;
  _lastSampleValid = false;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  _lastSampleTimestampMs = 0;
  _lastSample = Sample{};
  _lastBurst = BurstFrame{};
  _lastBurstValid = false;
  _pendingMode = Mode::POWER_DOWN;
  _lastFreshCounterValid = false;
  _lastFreshCounter = 0;
  _lastCounter = 0;
  _lastAdcCodes = 0;
  _lastLux = 0.0f;
}

void OPT4001::_cacheSampleAt(const Sample& sample, uint32_t nowMs) {
  _lastSample = sample;
  _lastSampleValid = true;
  _lastSampleTimestampMs = nowMs;
  _lastCounter = sample.counter;
  _lastAdcCodes = sample.adcCodes;
  _lastLux = sample.lux;
}

void OPT4001::_cacheSample(const Sample& sample) {
  _cacheSampleAt(sample, _nowMs());
}

void OPT4001::_markFreshSampleConsumedAt(const Sample& sample, uint32_t nowMs) {
  _cacheSampleAt(sample, nowMs);
  _lastFreshCounterValid = true;
  _lastFreshCounter = sample.counter;
  _clearReadinessEvidence();
  if (_config.mode == Mode::CONTINUOUS) {
    _conversionStarted = true;
    _conversionStartMs = nowMs;
  } else {
    _conversionStarted = false;
    _pendingMode = Mode::POWER_DOWN;
  }
}

uint16_t OPT4001::_buildConfigurationRegister(Mode mode) const {
  return _buildConfigurationRegister(_config, mode);
}

uint16_t OPT4001::_buildConfigurationRegister(const Config& config, Mode mode) const {
  uint16_t value = 0;
  if (config.quickWake) {
    value |= cmd::MASK_QWAKE;
  }
  value |= (static_cast<uint16_t>(config.range) << cmd::BIT_RANGE) & cmd::MASK_RANGE;
  value |= (static_cast<uint16_t>(config.conversionTime) << cmd::BIT_CONVERSION_TIME) &
           cmd::MASK_CONVERSION_TIME;
  value |= (static_cast<uint16_t>(mode) << cmd::BIT_MODE) & cmd::MASK_MODE;
  value |= (static_cast<uint16_t>(config.interruptLatch) << cmd::BIT_LATCH) & cmd::MASK_LATCH;
  value |= (static_cast<uint16_t>(config.interruptPolarity) << cmd::BIT_INT_POL) &
           cmd::MASK_INT_POL;
  value |= (static_cast<uint16_t>(config.faultCount) << cmd::BIT_FAULT_COUNT) &
           cmd::MASK_FAULT_COUNT;
  return value;
}

uint16_t OPT4001::_buildIntConfigurationRegister() const {
  return _buildIntConfigurationRegister(_config);
}

uint16_t OPT4001::_buildIntConfigurationRegister(const Config& config) const {
  uint16_t value = cmd::INTCFG_FIXED_BITS;
  value |= (static_cast<uint16_t>(config.intDirection) << cmd::BIT_INT_DIR) &
           cmd::MASK_INT_DIR;
  value |= (static_cast<uint16_t>(config.intConfig) << cmd::BIT_INT_CFG) &
           cmd::MASK_INT_CFG;
  if (config.burstMode) {
    value |= cmd::MASK_I2C_BURST;
  }
  return value;
}

uint16_t OPT4001::_packThreshold(const Threshold& threshold) const {
  return static_cast<uint16_t>(
      ((static_cast<uint16_t>(threshold.exponent) << cmd::BIT_THRESHOLD_EXPONENT) &
       cmd::MASK_THRESHOLD_EXPONENT) |
      (threshold.result & cmd::MASK_THRESHOLD_RESULT));
}

bool OPT4001::_thresholdValid(const Threshold& threshold) const {
  return threshold.exponent <= cmd::THRESHOLD_EXPONENT_MAX &&
         threshold.result <= cmd::THRESHOLD_RESULT_MAX;
}

uint8_t OPT4001::_computeCrcNibble(uint8_t exponent, uint32_t mantissa, uint8_t counter) const {
  auto getBit = [](uint32_t value, uint8_t index) -> uint8_t {
    return static_cast<uint8_t>((value >> index) & 0x1U);
  };

  uint8_t crc0 = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    crc0 ^= getBit(exponent, i);
    crc0 ^= getBit(counter, i);
  }
  for (uint8_t i = 0; i < 20; ++i) {
    crc0 ^= getBit(mantissa, i);
  }

  uint8_t crc1 = getBit(counter, 1) ^ getBit(counter, 3) ^
                 getBit(mantissa, 1) ^ getBit(mantissa, 3) ^ getBit(mantissa, 5) ^
                 getBit(mantissa, 7) ^ getBit(mantissa, 9) ^ getBit(mantissa, 11) ^
                 getBit(mantissa, 13) ^ getBit(mantissa, 15) ^ getBit(mantissa, 17) ^
                 getBit(mantissa, 19) ^ getBit(exponent, 1) ^ getBit(exponent, 3);

  uint8_t crc2 = getBit(counter, 3) ^
                 getBit(mantissa, 3) ^ getBit(mantissa, 7) ^ getBit(mantissa, 11) ^
                 getBit(mantissa, 15) ^ getBit(mantissa, 19) ^ getBit(exponent, 3);

  uint8_t crc3 = getBit(mantissa, 3) ^ getBit(mantissa, 11) ^ getBit(mantissa, 19);

  return static_cast<uint8_t>((crc3 << 3) | (crc2 << 2) | (crc1 << 1) | crc0);
}

uint32_t OPT4001::_nowMs() const {
  if (_config.nowMs != nullptr) {
    return _config.nowMs(_config.timeUser);
  }
  return 0;
}

void OPT4001::_cooperativeYield() const {
  if (_config.cooperativeYield != nullptr) {
    _config.cooperativeYield(_config.timeUser);
    return;
  }
}

}  // namespace OPT4001
