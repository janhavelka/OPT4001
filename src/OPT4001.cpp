/// @file OPT4001.cpp
/// @brief Implementation of OPT4001 driver.

#include "OPT4001/OPT4001.h"

#include <Arduino.h>
#include <climits>

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

uint32_t ceilUsToMs(uint32_t microseconds) {
  return (microseconds + 999U) / 1000U;
}

}  // namespace

// ============================================================================
// Lifecycle
// ============================================================================

Status OPT4001::begin(const Config& config) {
  _config = config;
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _clearRuntimeState();

  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;

  if (_config.i2cWrite == nullptr || _config.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks required");
  }
  if (!isValidPackageVariant(_config.packageVariant)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid package variant");
  }
  if (_config.i2cTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "I2C timeout must be > 0");
  }
  if (!isValidAddress(_config.i2cAddress, _config.packageVariant)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid I2C address for package");
  }
  if (!isValidRange(_config.range) || !isValidConversionTime(_config.conversionTime)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid range or conversion time");
  }
  if (!isStableMode(_config.mode)) {
    return Status::Error(Err::INVALID_CONFIG,
                         "Config mode must be POWER_DOWN or CONTINUOUS");
  }
  if (!isValidInterruptLatch(_config.interruptLatch) ||
      !isValidInterruptPolarity(_config.interruptPolarity) ||
      !isValidFaultCount(_config.faultCount) ||
      !isValidIntDirection(_config.intDirection) ||
      !isValidIntConfig(_config.intConfig)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid interrupt configuration");
  }
  if (_config.intPin < -1) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid INT pin");
  }
  if (!_thresholdValid(_config.lowThreshold) || !_thresholdValid(_config.highThreshold)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid threshold register value");
  }

  if (_config.offlineThreshold == 0) {
    _config.offlineThreshold = 1;
  }

  Status st = probe();
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

  if (_config.mode == Mode::CONTINUOUS) {
    if (!_conversionReady &&
        (nowMs - _conversionStartMs) >= getConversionTimeMs()) {
      _conversionStarted = false;
      _conversionReady = true;
      _sampleAvailable = true;
    }
    return;
  }

  if (!_conversionStarted || _conversionReady) {
    return;
  }

  if ((nowMs - _conversionStartMs) < getOneShotBudgetMs(_pendingMode)) {
    return;
  }

  (void)_markConversionReadyByRegisterPoll();
}

void OPT4001::end() {
  if (_initialized) {
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
  _clearRuntimeState();
}

// ============================================================================
// Diagnostics
// ============================================================================

Status OPT4001::probe() {
  uint16_t deviceId = 0;
  Status st = _readRegister16Raw(cmd::REG_DEVICE_ID, deviceId);
  if (!st.ok()) {
    if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
      return st;
    }
    return Status::Error(Err::DEVICE_NOT_FOUND, "OPT4001 not responding", st.detail);
  }
  if ((deviceId & cmd::MASK_DIDH) != cmd::DIDH_EXPECTED) {
    return Status::Error(Err::DEVICE_ID_MISMATCH, "Unexpected device ID", deviceId);
  }
  return Status::Ok();
}

Status OPT4001::recover() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t deviceId = 0;
  Status st = readDeviceId(deviceId);
  if (!st.ok()) {
    return st;
  }
  if ((deviceId & cmd::MASK_DIDH) != cmd::DIDH_EXPECTED) {
    return Status::Error(Err::DEVICE_ID_MISMATCH, "Unexpected device ID", deviceId);
  }

  _clearRuntimeState();
  return _applyConfig();
}

Status OPT4001::softReset() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  const uint8_t payload[1] = {cmd::GENERAL_CALL_RESET};
  Status st = _i2cWriteTrackedTo(cmd::GENERAL_CALL_ADDRESS, payload, sizeof(payload));
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
  Status st = _i2cWriteTrackedTo(cmd::GENERAL_CALL_ADDRESS, payload, sizeof(payload));
  if (!st.ok()) {
    return st;
  }

  _config = savedConfig;
  _clearRuntimeState();

  st = _applyConfig();
  if (!st.ok()) {
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
  return readRegister16(cmd::REG_DEVICE_ID, value);
}

Status OPT4001::getSettings(SettingsSnapshot& out) const {
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
  out.lastSampleValid = _lastSampleValid;
  out.conversionStarted = _conversionStarted;
  out.conversionReady = _conversionReady;
  out.conversionStartMs = _conversionStartMs;
  out.sampleTimestampMs = _lastSampleTimestampMs;
  out.lastCounter = _lastCounter;
  out.lastAdcCodes = _lastAdcCodes;
  out.lastLux = _lastLux;
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

  Status st = writeRegister16(cmd::REG_CONFIGURATION, _buildConfigurationRegister(mode));
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

bool OPT4001::conversionReady() {
  if (!_initialized) {
    return false;
  }

  if (_config.mode == Mode::CONTINUOUS) {
    if (_conversionReady) {
      return true;
    }
    if ((_nowMs() - _conversionStartMs) >= getConversionTimeMs()) {
      _conversionStarted = false;
      _conversionReady = true;
      _sampleAvailable = true;
    }
    return _conversionReady;
  }

  if (_conversionReady) {
    return true;
  }
  if (!_conversionStarted) {
    return false;
  }
  if ((_nowMs() - _conversionStartMs) < getOneShotBudgetMs(_pendingMode)) {
    return false;
  }

  return _markConversionReadyByRegisterPoll().ok() && _conversionReady;
}

Status OPT4001::readSample(Sample& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  if (_config.mode == Mode::CONTINUOUS) {
    if (!_conversionReady && !conversionReady()) {
      return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
    }
  } else {
    if (_conversionStarted && !conversionReady()) {
      return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
    }
    if (!_sampleAvailable) {
      return Status::Error(Err::MEASUREMENT_NOT_READY, "No sample available");
    }
  }

  Status st = _readSampleAt(cmd::REG_RESULT, out);
  if (!st.ok() && st.code != Err::CRC_ERROR) {
    return st;
  }

  _cacheSample(out);

  if (_config.mode == Mode::POWER_DOWN) {
    _conversionReady = false;
  }

  return st;
}

Status OPT4001::readBurst(BurstFrame& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  if (_config.mode == Mode::CONTINUOUS) {
    if (!_conversionReady && !conversionReady()) {
      return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
    }
  } else {
    if (_conversionStarted && !conversionReady()) {
      return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
    }
    if (!_sampleAvailable) {
      return Status::Error(Err::MEASUREMENT_NOT_READY, "No sample available");
    }
  }

  Status status = Status::Ok();
  if (_config.burstMode) {
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

    status = _decodeSampleRegisters(regs[0], regs[1], out.newest);
    if (status.ok()) status = _decodeSampleRegisters(regs[2], regs[3], out.fifo0);
    if (status.ok()) status = _decodeSampleRegisters(regs[4], regs[5], out.fifo1);
    if (status.ok()) status = _decodeSampleRegisters(regs[6], regs[7], out.fifo2);
  } else {
    status = _readSampleAt(cmd::REG_RESULT, out.newest);
    if (status.ok()) status = _readSampleAt(cmd::REG_FIFO0_MSB, out.fifo0);
    if (status.ok()) status = _readSampleAt(cmd::REG_FIFO1_MSB, out.fifo1);
    if (status.ok()) status = _readSampleAt(cmd::REG_FIFO2_MSB, out.fifo2);
  }

  _cacheSample(out.newest);

  if (_config.mode == Mode::POWER_DOWN) {
    _conversionReady = false;
  }

  return status;
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
  if (_lastSampleTimestampMs == 0) {
    return 0;
  }
  return nowMs - _lastSampleTimestampMs;
}

Status OPT4001::readLux(float& lux) {
  Sample sample;
  Status st = readSample(sample);
  if (!st.ok()) {
    return st;
  }
  lux = sample.lux;
  return Status::Ok();
}

Status OPT4001::readMilliLux(uint32_t& milliLux) {
  uint64_t microLux = 0;
  Status st = readMicroLux(microLux);
  if (!st.ok()) {
    return st;
  }
  milliLux = static_cast<uint32_t>((microLux + 500ULL) / 1000ULL);
  return Status::Ok();
}

Status OPT4001::readMicroLux(uint64_t& microLux) {
  Sample sample;
  Status st = readSample(sample);
  if (!st.ok()) {
    return st;
  }

  const uint64_t numerator =
      (_config.packageVariant == PackageVariant::PICOSTAR)
          ? cmd::MICRO_LUX_NUMERATOR_PICOSTAR
          : cmd::MICRO_LUX_NUMERATOR_SOT_5X3;
  microLux = (static_cast<uint64_t>(sample.adcCodes) * numerator + 5ULL) / 10ULL;
  return Status::Ok();
}

Status OPT4001::readBlocking(Sample& out, uint32_t timeoutMs) {
  return readBlocking(out, Mode::ONE_SHOT, timeoutMs);
}

Status OPT4001::readBlocking(Sample& out, Mode mode, uint32_t timeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_config.mode == Mode::CONTINUOUS) {
    const uint32_t deadlineMs = _nowMs() + timeoutMs;
    while (static_cast<int32_t>(_nowMs() - deadlineMs) < 0) {
      if (conversionReady()) {
        return readSample(out);
      }
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

  const uint32_t nowMs = _nowMs();
  const uint32_t budgetMs = getOneShotBudgetMs(_pendingMode);
  const uint32_t deadlineMs = nowMs + timeoutMs;

  uint32_t readyAtMs = nowMs + budgetMs;
  if (st.code == Err::BUSY) {
    const uint32_t elapsed = nowMs - _conversionStartMs;
    readyAtMs = (elapsed >= budgetMs) ? nowMs : (nowMs + budgetMs - elapsed);
  }

  while (static_cast<int32_t>(_nowMs() - deadlineMs) < 0) {
    if (static_cast<int32_t>(_nowMs() - readyAtMs) < 0) {
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
  }

  _conversionStarted = false;
  _conversionReady = false;
  _sampleAvailable = false;
  _pendingMode = Mode::POWER_DOWN;
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

// ============================================================================
// Flags / Status
// ============================================================================

Status OPT4001::readFlags(Flags& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t raw = 0;
  Status st = readRegister16(cmd::REG_FLAGS, raw);
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

Status OPT4001::clearFlags() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  return writeRegister16(cmd::REG_FLAGS, 0x0001);
}

// ============================================================================
// Configuration
// ============================================================================

Status OPT4001::setPackageVariant(PackageVariant variant) {
  if (!isValidPackageVariant(variant)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid package variant");
  }
  if (!isValidAddress(_config.i2cAddress, variant)) {
    return Status::Error(Err::INVALID_PARAM, "Current I2C address invalid for package");
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
  _config.range = range;
  return _applyConfig();
}

Status OPT4001::setConversionTime(ConversionTime time) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidConversionTime(time)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid conversion time");
  }
  _config.conversionTime = time;
  return _applyConfig();
}

Status OPT4001::setMode(Mode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isStableMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Use startConversion() for one-shot modes");
  }
  _config.mode = mode;
  return _applyConfig();
}

Status OPT4001::setQuickWake(bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  _config.quickWake = enable;
  return _applyConfig();
}

Status OPT4001::setVerifyCrc(bool enable) {
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
  _config.interruptLatch = latch;
  return _applyConfig();
}

Status OPT4001::setInterruptPolarity(InterruptPolarity polarity) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidInterruptPolarity(polarity)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid interrupt polarity");
  }
  _config.interruptPolarity = polarity;
  return _applyConfig();
}

Status OPT4001::setFaultCount(FaultCount count) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidFaultCount(count)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid fault count");
  }
  _config.faultCount = count;
  return _applyConfig();
}

Status OPT4001::setIntDirection(IntDirection direction) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidIntDirection(direction)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid INT direction");
  }
  _config.intDirection = direction;
  return _applyConfig();
}

Status OPT4001::setIntConfig(IntConfig config) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidIntConfig(config)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid INT configuration");
  }
  _config.intConfig = config;
  return _applyConfig();
}

Status OPT4001::setBurstMode(bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  _config.burstMode = enable;
  return _applyConfig();
}

Status OPT4001::setThresholds(const Threshold& low, const Threshold& high) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!_thresholdValid(low) || !_thresholdValid(high)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid threshold");
  }

  _config.lowThreshold = low;
  _config.highThreshold = high;

  Status st = writeRegister16(cmd::REG_THRESHOLD_L, _packThreshold(low));
  if (!st.ok()) {
    return st;
  }
  return writeRegister16(cmd::REG_THRESHOLD_H, _packThreshold(high));
}

Status OPT4001::getThresholds(Threshold& low, Threshold& high) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t lowRaw = 0;
  uint16_t highRaw = 0;
  Status st = readRegister16(cmd::REG_THRESHOLD_L, lowRaw);
  if (!st.ok()) {
    return st;
  }
  st = readRegister16(cmd::REG_THRESHOLD_H, highRaw);
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

Status OPT4001::readConfiguration(uint16_t& value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  return readRegister16(cmd::REG_CONFIGURATION, value);
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

  Status st = writeRegister16(cmd::REG_CONFIGURATION, value);
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
  return readRegister16(cmd::REG_INT_CONFIGURATION, value);
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

  Status st = writeRegister16(cmd::REG_INT_CONFIGURATION, value);
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

Status OPT4001::readRegister16(uint8_t reg, uint16_t& value) {
  uint8_t rx[2] = {0, 0};
  Status st = _i2cWriteReadTracked(&reg, 1, rx, sizeof(rx));
  if (!st.ok()) {
    return st;
  }
  value = static_cast<uint16_t>((rx[0] << 8) | rx[1]);
  return Status::Ok();
}

Status OPT4001::writeRegister16(uint8_t reg, uint16_t value) {
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

float OPT4001::adcCodesToLux(uint32_t adcCodes) const {
  return static_cast<float>(adcCodes) * getLuxLsb();
}

float OPT4001::rawToLux(uint8_t exponent, uint32_t mantissa) const {
  return adcCodesToLux(static_cast<uint32_t>(mantissa << exponent));
}

float OPT4001::getLuxLsb() const {
  return (_config.packageVariant == PackageVariant::PICOSTAR)
             ? cmd::LUX_LSB_PICOSTAR
             : cmd::LUX_LSB_SOT_5X3;
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
  if (lux < 0.0f) {
    return Status::Error(Err::INVALID_PARAM, "Lux threshold must be >= 0");
  }

  const float lsb = getLuxLsb();
  uint32_t adcCodes = 0;
  if (lsb > 0.0f) {
    adcCodes = static_cast<uint32_t>((lux / lsb) + 0.5f);
  }

  uint8_t exponent = 0;
  while (exponent < cmd::THRESHOLD_EXPONENT_MAX) {
    const uint32_t result = adcCodes >> (8U + exponent);
    if (result <= cmd::THRESHOLD_RESULT_MAX) {
      out.exponent = exponent;
      out.result = static_cast<uint16_t>(result);
      return Status::Ok();
    }
    ++exponent;
  }

  out.exponent = cmd::THRESHOLD_EXPONENT_MAX;
  out.result = cmd::THRESHOLD_RESULT_MAX;
  return Status::Ok();
}

uint32_t OPT4001::thresholdToAdcCodes(const Threshold& threshold) const {
  return static_cast<uint32_t>(threshold.result) << (8U + threshold.exponent);
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

Status OPT4001::_i2cWriteRawTo(uint8_t addr, const uint8_t* buf, size_t len) {
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
  return _i2cWriteRawTo(_config.i2cAddress, buf, len);
}

Status OPT4001::_i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                                     uint8_t* rxBuf, size_t rxLen) {
  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

Status OPT4001::_i2cWriteTracked(const uint8_t* buf, size_t len) {
  Status st = _i2cWriteRaw(buf, len);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

Status OPT4001::_i2cWriteTrackedTo(uint8_t addr, const uint8_t* buf, size_t len) {
  Status st = _i2cWriteRawTo(addr, buf, len);
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
  uint16_t result = 0;
  uint16_t lsbCrc = 0;
  Status st = readRegister16(msbReg, result);
  if (!st.ok()) {
    return st;
  }
  st = readRegister16(static_cast<uint8_t>(msbReg + 1U), lsbCrc);
  if (!st.ok()) {
    return st;
  }
  return _decodeSampleRegisters(result, lsbCrc, out);
}

Status OPT4001::_decodeSampleRegisters(uint16_t resultReg, uint16_t lsbCrcReg,
                                       Sample& out) const {
  out.resultReg = resultReg;
  out.resultLsbCrcReg = lsbCrcReg;
  out.exponent = static_cast<uint8_t>((resultReg & cmd::MASK_EXPONENT) >> cmd::BIT_EXPONENT);
  out.mantissa = (static_cast<uint32_t>(resultReg & cmd::MASK_RESULT_MSB) << 8) |
                 static_cast<uint32_t>((lsbCrcReg & cmd::MASK_RESULT_LSB) >> cmd::BIT_RESULT_LSB);
  out.adcCodes = static_cast<uint32_t>(out.mantissa << out.exponent);
  out.counter = static_cast<uint8_t>((lsbCrcReg & cmd::MASK_COUNTER) >> cmd::BIT_COUNTER);
  out.crc = static_cast<uint8_t>(lsbCrcReg & cmd::MASK_CRC);
  out.crcValid = (_computeCrcNibble(out.exponent, out.mantissa, out.counter) == out.crc);
  out.lux = adcCodesToLux(out.adcCodes);

  if (_config.verifyCrc && !out.crcValid) {
    return Status::Error(Err::CRC_ERROR, "Sample CRC mismatch", out.crc);
  }
  return Status::Ok();
}

// ============================================================================
// Health Tracking
// ============================================================================

Status OPT4001::_updateHealth(const Status& st) {
  const uint32_t nowMs = _nowMs();

  if (st.ok() || st.inProgress()) {
    _lastOkMs = nowMs;
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      _totalSuccess++;
    }
    if (_initialized) {
      _driverState = DriverState::READY;
    }
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
  if (_initialized) {
    _driverState = (_consecutiveFailures >= _config.offlineThreshold)
                       ? DriverState::OFFLINE
                       : DriverState::DEGRADED;
  }
  return st;
}

// ============================================================================
// Internal
// ============================================================================

Status OPT4001::_applyConfig() {
  Status st = writeRegister16(cmd::REG_THRESHOLD_L, _packThreshold(_config.lowThreshold));
  if (!st.ok()) {
    return st;
  }
  st = writeRegister16(cmd::REG_THRESHOLD_H, _packThreshold(_config.highThreshold));
  if (!st.ok()) {
    return st;
  }
  st = writeRegister16(cmd::REG_INT_CONFIGURATION, _buildIntConfigurationRegister());
  if (!st.ok()) {
    return st;
  }
  st = writeRegister16(cmd::REG_CONFIGURATION, _buildConfigurationRegister(_config.mode));
  if (!st.ok()) {
    return st;
  }

  _clearRuntimeState();

  if (_config.mode == Mode::CONTINUOUS) {
    _conversionStarted = true;
    _conversionStartMs = _nowMs();
  } else {
    _conversionStarted = false;
    _conversionStartMs = 0;
  }

  return Status::Ok();
}

void OPT4001::_clearRuntimeState() {
  _sampleAvailable = false;
  _lastSampleValid = false;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  _lastSampleTimestampMs = 0;
  _lastSample = Sample{};
  _pendingMode = Mode::POWER_DOWN;
  _lastCounter = 0;
  _lastAdcCodes = 0;
  _lastLux = 0.0f;
}

void OPT4001::_cacheSample(const Sample& sample) {
  _lastSample = sample;
  _lastSampleValid = true;
  _lastSampleTimestampMs = _nowMs();
  _lastCounter = sample.counter;
  _lastAdcCodes = sample.adcCodes;
  _lastLux = sample.lux;
}

Status OPT4001::_markConversionReadyByRegisterPoll() {
  uint16_t configReg = 0;
  Status st = readRegister16(cmd::REG_CONFIGURATION, configReg);
  if (!st.ok()) {
    return st;
  }

  const Mode hardwareMode =
      static_cast<Mode>((configReg & cmd::MASK_MODE) >> cmd::BIT_MODE);
  if (hardwareMode != Mode::POWER_DOWN) {
    return Status::Error(Err::MEASUREMENT_NOT_READY, "Measurement not ready");
  }

  _conversionStarted = false;
  _conversionReady = true;
  _sampleAvailable = true;
  _pendingMode = Mode::POWER_DOWN;
  return Status::Ok();
}

uint16_t OPT4001::_buildConfigurationRegister(Mode mode) const {
  uint16_t value = 0;
  if (_config.quickWake) {
    value |= cmd::MASK_QWAKE;
  }
  value |= (static_cast<uint16_t>(_config.range) << cmd::BIT_RANGE) & cmd::MASK_RANGE;
  value |= (static_cast<uint16_t>(_config.conversionTime) << cmd::BIT_CONVERSION_TIME) &
           cmd::MASK_CONVERSION_TIME;
  value |= (static_cast<uint16_t>(mode) << cmd::BIT_MODE) & cmd::MASK_MODE;
  value |= (static_cast<uint16_t>(_config.interruptLatch) << cmd::BIT_LATCH) & cmd::MASK_LATCH;
  value |= (static_cast<uint16_t>(_config.interruptPolarity) << cmd::BIT_INT_POL) &
           cmd::MASK_INT_POL;
  value |= (static_cast<uint16_t>(_config.faultCount) << cmd::BIT_FAULT_COUNT) &
           cmd::MASK_FAULT_COUNT;
  return value;
}

uint16_t OPT4001::_buildIntConfigurationRegister() const {
  uint16_t value = cmd::INTCFG_FIXED_BITS;
  value |= (static_cast<uint16_t>(_config.intDirection) << cmd::BIT_INT_DIR) &
           cmd::MASK_INT_DIR;
  value |= (static_cast<uint16_t>(_config.intConfig) << cmd::BIT_INT_CFG) &
           cmd::MASK_INT_CFG;
  if (_config.burstMode) {
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
  return millis();
}

void OPT4001::_cooperativeYield() const {
  if (_config.cooperativeYield != nullptr) {
    _config.cooperativeYield(_config.timeUser);
    return;
  }
  yield();
}

}  // namespace OPT4001
