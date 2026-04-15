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
  UNINIT,
  READY,
  DEGRADED,
  OFFLINE
};

/// Decoded measurement sample from RESULT/FIFO registers.
struct Sample {
  uint16_t resultReg = 0;
  uint16_t resultLsbCrcReg = 0;
  uint8_t exponent = 0;
  uint32_t mantissa = 0;
  uint32_t adcCodes = 0;
  uint8_t counter = 0;
  uint8_t crc = 0;
  bool crcValid = false;
  float lux = 0.0f;
};

/// Four-deep burst frame: newest sample plus three FIFO shadows.
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

/// Snapshot of cached configuration and runtime state without I2C access.
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
  bool lastSampleValid = false;
  bool conversionStarted = false;
  bool conversionReady = false;
  uint32_t conversionStartMs = 0;
  uint32_t sampleTimestampMs = 0;
  uint8_t lastCounter = 0;
  uint32_t lastAdcCodes = 0;
  float lastLux = 0.0f;
};

/// OPT4001 driver class.
class OPT4001 {
public:
  // === Lifecycle ===
  Status begin(const Config& config);
  void tick(uint32_t nowMs);
  void end();

  bool isInitialized() const { return _initialized; }
  const Config& getConfig() const { return _config; }

  // === Diagnostics (probe uses raw transport, recover uses tracked transport) ===
  Status probe();
  Status recover();
  /// Perform the documented general-call reset (address 0x00, data 0x06).
  /// This is bus-wide and leaves the driver in UNINIT state.
  Status softReset();
  /// General-call reset followed by re-applying the cached configuration.
  Status resetAndReapply();
  Status readDeviceId(uint16_t& value);
  Status getSettings(SettingsSnapshot& out) const;

  // === Driver State ===
  DriverState state() const { return _driverState; }
  bool isOnline() const {
    return _driverState == DriverState::READY ||
           _driverState == DriverState::DEGRADED;
  }

  // === Health Tracking ===
  uint32_t lastOkMs() const { return _lastOkMs; }
  uint32_t lastErrorMs() const { return _lastErrorMs; }
  Status lastError() const { return _lastError; }
  uint8_t consecutiveFailures() const { return _consecutiveFailures; }
  uint32_t totalFailures() const { return _totalFailures; }
  uint32_t totalSuccess() const { return _totalSuccess; }

  // === Measurement API ===
  Status startConversion();
  Status startConversion(Mode mode);
  bool conversionReady();
  Status readSample(Sample& out);
  Status readBurst(BurstFrame& out);
  Status getLastSample(Sample& out) const;
  uint32_t sampleTimestampMs() const;
  uint32_t sampleAgeMs(uint32_t nowMs) const;
  Status readLux(float& lux);
  Status readMilliLux(uint32_t& milliLux);
  Status readMicroLux(uint64_t& microLux);
  Status readBlocking(Sample& out, uint32_t timeoutMs = 1000);
  Status readBlocking(Sample& out, Mode mode, uint32_t timeoutMs);
  Status readBlockingLux(float& lux, uint32_t timeoutMs = 1000);
  Status readBlockingLux(float& lux, Mode mode, uint32_t timeoutMs);

  // === Flags / Status ===
  /// Read FLAGS register. Reading register 0x0C clears latched flags and ready flag.
  Status readFlags(Flags& out);
  Status clearFlags();

  // === Configuration ===
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

  Status setThresholds(const Threshold& low, const Threshold& high);
  Status getThresholds(Threshold& low, Threshold& high);
  Status setThresholdsLux(float lowLux, float highLux);

  Status readConfiguration(uint16_t& value);
  Status writeConfiguration(uint16_t value);
  Status readIntConfiguration(uint16_t& value);
  Status writeIntConfiguration(uint16_t value);
  Status readConfig(uint16_t& value) { return readConfiguration(value); }
  Status writeConfig(uint16_t value) { return writeConfiguration(value); }
  Status readIntConfig(uint16_t& value) { return readIntConfiguration(value); }
  Status writeIntConfig(uint16_t value) { return writeIntConfiguration(value); }

  // === Raw Register Access ===
  Status readRegister16(uint8_t reg, uint16_t& value);
  Status writeRegister16(uint8_t reg, uint16_t value);
  Status readRegister(uint8_t reg, uint16_t& value) { return readRegister16(reg, value); }
  Status writeRegister(uint8_t reg, uint16_t value) { return writeRegister16(reg, value); }

  // === Utility ===
  float adcCodesToLux(uint32_t adcCodes) const;
  float rawToLux(uint8_t exponent, uint32_t mantissa) const;
  float getLuxLsb() const;
  uint32_t getConversionTimeUs() const;
  uint32_t getConversionTimeMs() const;
  uint32_t getOneShotBudgetUs(Mode mode) const;
  uint32_t getOneShotBudgetMs(Mode mode) const;
  Status luxToThreshold(float lux, Threshold& out) const;
  uint32_t thresholdToAdcCodes(const Threshold& threshold) const;

private:
  // === Transport Wrappers ===
  Status _i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                          uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteRawTo(uint8_t addr, const uint8_t* buf, size_t len);
  Status _i2cWriteRaw(const uint8_t* buf, size_t len);
  Status _i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                              uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteTrackedTo(uint8_t addr, const uint8_t* buf, size_t len);
  Status _i2cWriteTracked(const uint8_t* buf, size_t len);

  // === Register Access ===
  Status _readRegister16Raw(uint8_t reg, uint16_t& value);
  Status _readSampleAt(uint8_t msbReg, Sample& out);
  Status _decodeSampleRegisters(uint16_t resultReg, uint16_t lsbCrcReg, Sample& out) const;

  // === Health Tracking ===
  Status _updateHealth(const Status& st);

  // === Internal Helpers ===
  Status _applyConfig();
  void _clearRuntimeState();
  void _cacheSample(const Sample& sample);
  Status _markConversionReadyByRegisterPoll();
  uint16_t _buildConfigurationRegister(Mode mode) const;
  uint16_t _buildIntConfigurationRegister() const;
  uint16_t _packThreshold(const Threshold& threshold) const;
  bool _thresholdValid(const Threshold& threshold) const;
  uint8_t _computeCrcNibble(uint8_t exponent, uint32_t mantissa, uint8_t counter) const;
  uint32_t _nowMs() const;
  void _cooperativeYield() const;

  // === State ===
  Config _config;
  bool _initialized = false;
  DriverState _driverState = DriverState::UNINIT;

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
  uint32_t _conversionStartMs = 0;
  uint32_t _lastSampleTimestampMs = 0;
  Sample _lastSample{};
  Mode _pendingMode = Mode::POWER_DOWN;
  uint8_t _lastCounter = 0;
  uint32_t _lastAdcCodes = 0;
  float _lastLux = 0.0f;
};

}  // namespace OPT4001
