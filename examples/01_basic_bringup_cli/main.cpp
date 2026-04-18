/// @file main.cpp
/// @brief OPT4001 basic bring-up CLI example.
/// @note This is an EXAMPLE, not part of the library.

#include <Arduino.h>
#include <cstdlib>

#include "examples/common/BoardConfig.h"
#include "examples/common/BusDiag.h"
#include "examples/common/HealthDiag.h"
#include "examples/common/HealthView.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/Log.h"

#include "OPT4001/OPT4001.h"

OPT4001::OPT4001 device;
bool verboseMode = false;
static constexpr uint32_t STRESS_PROGRESS_UPDATES = 10U;
static constexpr uint32_t BLOCKING_READ_TIMEOUT_MS = 1500U;
static constexpr uint32_t HEALTH_MONITOR_DEFAULT_INTERVAL_MS = 1000U;
bool healthMonitorEnabled = false;
uint32_t healthMonitorIntervalMs = HEALTH_MONITOR_DEFAULT_INTERVAL_MS;
diag::HealthMonitor healthMonitor;

void printScale();

const char* errToStr(OPT4001::Err err) {
  using OPT4001::Err;
  switch (err) {
    case Err::OK: return "OK";
    case Err::NOT_INITIALIZED: return "NOT_INITIALIZED";
    case Err::INVALID_CONFIG: return "INVALID_CONFIG";
    case Err::I2C_ERROR: return "I2C_ERROR";
    case Err::TIMEOUT: return "TIMEOUT";
    case Err::INVALID_PARAM: return "INVALID_PARAM";
    case Err::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
    case Err::DEVICE_ID_MISMATCH: return "DEVICE_ID_MISMATCH";
    case Err::CRC_ERROR: return "CRC_ERROR";
    case Err::MEASUREMENT_NOT_READY: return "MEASUREMENT_NOT_READY";
    case Err::BUSY: return "BUSY";
    case Err::IN_PROGRESS: return "IN_PROGRESS";
    case Err::I2C_NACK_ADDR: return "I2C_NACK_ADDR";
    case Err::I2C_NACK_DATA: return "I2C_NACK_DATA";
    case Err::I2C_TIMEOUT: return "I2C_TIMEOUT";
    case Err::I2C_BUS: return "I2C_BUS";
    default: return "UNKNOWN";
  }
}

const char* stateToStr(OPT4001::DriverState state) {
  switch (state) {
    case OPT4001::DriverState::UNINIT: return "UNINIT";
    case OPT4001::DriverState::READY: return "READY";
    case OPT4001::DriverState::DEGRADED: return "DEGRADED";
    case OPT4001::DriverState::OFFLINE: return "OFFLINE";
    default: return "UNKNOWN";
  }
}

const char* modeToStr(OPT4001::Mode mode) {
  switch (mode) {
    case OPT4001::Mode::POWER_DOWN: return "POWER_DOWN";
    case OPT4001::Mode::ONE_SHOT_FORCED_AUTO: return "ONE_SHOT_FORCED_AUTO";
    case OPT4001::Mode::ONE_SHOT: return "ONE_SHOT";
    case OPT4001::Mode::CONTINUOUS: return "CONTINUOUS";
    default: return "UNKNOWN";
  }
}

const char* packageToStr(OPT4001::PackageVariant variant) {
  return (variant == OPT4001::PackageVariant::PICOSTAR) ? "PICOSTAR" : "SOT_5X3";
}

const char* rangeToStr(OPT4001::Range range) {
  switch (range) {
    case OPT4001::Range::RANGE_0: return "RANGE_0";
    case OPT4001::Range::RANGE_1: return "RANGE_1";
    case OPT4001::Range::RANGE_2: return "RANGE_2";
    case OPT4001::Range::RANGE_3: return "RANGE_3";
    case OPT4001::Range::RANGE_4: return "RANGE_4";
    case OPT4001::Range::RANGE_5: return "RANGE_5";
    case OPT4001::Range::RANGE_6: return "RANGE_6";
    case OPT4001::Range::RANGE_7: return "RANGE_7";
    case OPT4001::Range::RANGE_8: return "RANGE_8";
    case OPT4001::Range::AUTO: return "AUTO";
    default: return "UNKNOWN";
  }
}

const char* conversionTimeToStr(OPT4001::ConversionTime time) {
  static constexpr const char* kNames[] = {
    "600us", "1ms", "1.8ms", "3.4ms", "6.5ms", "12.7ms",
    "25ms", "50ms", "100ms", "200ms", "400ms", "800ms"
  };
  const uint8_t idx = static_cast<uint8_t>(time);
  return (idx < 12U) ? kNames[idx] : "UNKNOWN";
}

const char* latchToStr(OPT4001::InterruptLatch latch) {
  return (latch == OPT4001::InterruptLatch::LATCHED) ? "LATCHED" : "TRANSPARENT";
}

const char* polarityToStr(OPT4001::InterruptPolarity polarity) {
  return (polarity == OPT4001::InterruptPolarity::ACTIVE_HIGH) ? "ACTIVE_HIGH" : "ACTIVE_LOW";
}

const char* faultCountToStr(OPT4001::FaultCount count) {
  switch (count) {
    case OPT4001::FaultCount::FAULTS_1: return "1";
    case OPT4001::FaultCount::FAULTS_2: return "2";
    case OPT4001::FaultCount::FAULTS_4: return "4";
    case OPT4001::FaultCount::FAULTS_8: return "8";
    default: return "?";
  }
}

const char* intDirectionToStr(OPT4001::IntDirection direction) {
  return (direction == OPT4001::IntDirection::PIN_INPUT) ? "INPUT" : "OUTPUT";
}

const char* intConfigToStr(OPT4001::IntConfig config) {
  switch (config) {
    case OPT4001::IntConfig::THRESHOLD: return "THRESHOLD";
    case OPT4001::IntConfig::EVERY_CONVERSION: return "EVERY_CONVERSION";
    case OPT4001::IntConfig::FIFO_FULL: return "FIFO_FULL";
    default: return "UNKNOWN";
  }
}

const char* intConfigBehaviorToStr(OPT4001::IntConfig config) {
  switch (config) {
    case OPT4001::IntConfig::THRESHOLD:
      return "threshold / SMBus-alert mode";
    case OPT4001::IntConfig::EVERY_CONVERSION:
      return "pulse after every conversion";
    case OPT4001::IntConfig::FIFO_FULL:
      return "pulse every 4 conversions (FIFO full)";
    default:
      return "invalid / reserved";
  }
}

const char* stateColor(OPT4001::DriverState state, bool online, uint8_t consecutiveFailures) {
  if (state == OPT4001::DriverState::UNINIT) {
    return LOG_COLOR_YELLOW;
  }
  return LOG_COLOR_STATE(online, consecutiveFailures);
}

const char* goodIfZeroColor(uint32_t value) {
  return (value == 0U) ? LOG_COLOR_GREEN : LOG_COLOR_RED;
}

const char* goodIfNonZeroColor(uint32_t value) {
  return (value > 0U) ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

const char* warnCountColor(uint32_t value) {
  return (value == 0U) ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

const char* onOffColor(bool enabled) {
  return enabled ? LOG_COLOR_GREEN : LOG_COLOR_RESET;
}

const char* yesNoColor(bool value) {
  return value ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

const char* skipCountColor(uint32_t value) {
  return (value > 0U) ? LOG_COLOR_YELLOW : LOG_COLOR_RESET;
}

const char* successRateColor(float pct) {
  if (pct >= 99.9f) return LOG_COLOR_GREEN;
  if (pct >= 80.0f) return LOG_COLOR_YELLOW;
  return LOG_COLOR_RED;
}

const char* staleTimeColor(bool isErrorTimestamp) {
  return isErrorTimestamp ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

const char* crcColor(bool crcValid) {
  return crcValid ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

bool parseI32(const String& token, int32_t& out) {
  char* end = nullptr;
  const long value = strtol(token.c_str(), &end, 0);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = static_cast<int32_t>(value);
  return true;
}

bool parseU32(const String& token, uint32_t& out) {
  char* end = nullptr;
  const unsigned long value = strtoul(token.c_str(), &end, 0);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = static_cast<uint32_t>(value);
  return true;
}

bool parseF32(const String& token, float& out) {
  char* end = nullptr;
  const float value = strtof(token.c_str(), &end);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = value;
  return true;
}

bool parseBool01(const String& token, bool& out) {
  int32_t value = 0;
  if (!parseI32(token, value) || (value != 0 && value != 1)) {
    return false;
  }
  out = (value != 0);
  return true;
}

bool sampleStatusHasData(const OPT4001::Status& st) {
  return st.ok() || st.code == OPT4001::Err::CRC_ERROR;
}

bool sampleStatusWarn(const OPT4001::Status& st) {
  return st.code == OPT4001::Err::CRC_ERROR;
}

uint32_t stressProgressStep(uint32_t total) {
  if (total == 0U) {
    return 0U;
  }
  const uint32_t step = total / STRESS_PROGRESS_UPDATES;
  return (step == 0U) ? 1U : step;
}

void printStressProgress(uint32_t completed,
                         uint32_t total,
                         uint32_t okCount,
                         uint32_t warnCount,
                         uint32_t failCount) {
  if (completed == 0U || total == 0U) {
    return;
  }

  const uint32_t step = stressProgressStep(total);
  if (step == 0U || (completed != total && (completed % step) != 0U)) {
    return;
  }

  const float pct = (100.0f * static_cast<float>(completed)) / static_cast<float>(total);
  Serial.printf("  Progress: %lu/%lu (%s%.0f%%%s, ok=%s%lu%s, warn=%s%lu%s, fail=%s%lu%s)\n",
                static_cast<unsigned long>(completed),
                static_cast<unsigned long>(total),
                successRateColor(pct),
                pct,
                LOG_COLOR_RESET,
                goodIfNonZeroColor(okCount),
                static_cast<unsigned long>(okCount),
                LOG_COLOR_RESET,
                warnCountColor(warnCount),
                static_cast<unsigned long>(warnCount),
                LOG_COLOR_RESET,
                goodIfZeroColor(failCount),
                static_cast<unsigned long>(failCount),
                LOG_COLOR_RESET);
}

OPT4001::Config makeDefaultConfig() {
  OPT4001::Config cfg;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cUser = &Wire;
  cfg.i2cAddress = OPT4001::cmd::I2C_ADDR_DEFAULT;
  cfg.i2cTimeoutMs = board::I2C_TIMEOUT_MS;
  cfg.packageVariant = OPT4001::PackageVariant::SOT_5X3;
  cfg.mode = OPT4001::Mode::POWER_DOWN;
  cfg.offlineThreshold = 5;
  if (board::INT_PIN >= 0) {
    cfg.intPin = board::INT_PIN;
    cfg.gpioRead = board::readIntPin;
  }
  return cfg;
}

void printStatus(const OPT4001::Status& st) {
  Serial.printf("  Status: %s%s%s (code=%u, detail=%ld)\n",
                LOG_COLOR_RESULT(st.ok()),
                errToStr(st.code),
                LOG_COLOR_RESET,
                static_cast<unsigned>(st.code),
                static_cast<long>(st.detail));
  if (st.msg && st.msg[0]) {
    Serial.printf("  Message: %s%s%s\n", LOG_COLOR_YELLOW, st.msg, LOG_COLOR_RESET);
  }
}

void printSample(const OPT4001::Sample& sample) {
  Serial.printf("  Lux=%.6f lx adc=%lu exp=%u mant=0x%05lX ctr=%u crc=%s0x%X (%s)%s\n",
                sample.lux,
                static_cast<unsigned long>(sample.adcCodes),
                sample.exponent,
                static_cast<unsigned long>(sample.mantissa),
                sample.counter,
                crcColor(sample.crcValid),
                sample.crc,
                sample.crcValid ? "OK" : "BAD",
                LOG_COLOR_RESET);
  Serial.printf("  Full-scale=%.3f lx resolution=%.9f lx\n",
                device.getSampleFullScaleLux(sample),
                device.getSampleResolutionLux(sample));
}

void printVersionInfo() {
  Serial.println("=== Version Info ===");
  Serial.printf("  Example firmware build: %s %s\n", __DATE__, __TIME__);
  Serial.printf("  OPT4001 library version: %s\n", OPT4001::VERSION);
  Serial.printf("  OPT4001 library full: %s\n", OPT4001::VERSION_FULL);
  Serial.printf("  OPT4001 library build: %s\n", OPT4001::BUILD_TIMESTAMP);
  Serial.printf("  OPT4001 library commit: %s (%s)\n",
                OPT4001::GIT_COMMIT,
                OPT4001::GIT_STATUS);
  Serial.printf("  OPT4001 version code: %d (major=%u minor=%u patch=%u)\n",
                OPT4001::VERSION_INT,
                static_cast<unsigned>(OPT4001::VERSION_MAJOR),
                static_cast<unsigned>(OPT4001::VERSION_MINOR),
                static_cast<unsigned>(OPT4001::VERSION_PATCH));
}

void printDriverHealth() {
  const uint32_t now = millis();
  const uint32_t totalOk = device.totalSuccess();
  const uint32_t totalFail = device.totalFailures();
  const uint32_t total = totalOk + totalFail;
  const float successRate = (total > 0U)
                                ? (100.0f * static_cast<float>(totalOk) / static_cast<float>(total))
                                : 0.0f;
  const OPT4001::Status lastErr = device.lastError();
  const OPT4001::DriverState st = device.state();
  const bool online = device.isOnline();

  Serial.println("=== Driver Health ===");
  Serial.printf("  State: %s%s%s\n",
                stateColor(st, online, device.consecutiveFailures()),
                stateToStr(st),
                LOG_COLOR_RESET);
  Serial.printf("  Online: %s%s%s\n",
                online ? LOG_COLOR_GREEN : LOG_COLOR_RED,
                log_bool_str(online),
                LOG_COLOR_RESET);
  Serial.printf("  Consecutive failures: %s%u%s\n",
                goodIfZeroColor(device.consecutiveFailures()),
                device.consecutiveFailures(),
                LOG_COLOR_RESET);
  Serial.printf("  Total success: %s%lu%s\n",
                goodIfNonZeroColor(totalOk),
                static_cast<unsigned long>(totalOk),
                LOG_COLOR_RESET);
  Serial.printf("  Total failures: %s%lu%s\n",
                goodIfZeroColor(totalFail),
                static_cast<unsigned long>(totalFail),
                LOG_COLOR_RESET);
  Serial.printf("  Success rate: %s%.1f%%%s\n",
                successRateColor(successRate),
                successRate,
                LOG_COLOR_RESET);

  const uint32_t lastOkMs = device.lastOkMs();
  if (lastOkMs > 0U) {
    Serial.printf("  Last OK: %s%lu ms ago (at %lu ms)%s\n",
                  LOG_COLOR_GREEN,
                  static_cast<unsigned long>(now - lastOkMs),
                  static_cast<unsigned long>(lastOkMs),
                  LOG_COLOR_RESET);
  } else {
    Serial.printf("  Last OK: %snever%s\n", staleTimeColor(false), LOG_COLOR_RESET);
  }

  const uint32_t lastErrorMs = device.lastErrorMs();
  if (lastErrorMs > 0U) {
    Serial.printf("  Last error: %s%lu ms ago (at %lu ms)%s\n",
                  LOG_COLOR_RED,
                  static_cast<unsigned long>(now - lastErrorMs),
                  static_cast<unsigned long>(lastErrorMs),
                  LOG_COLOR_RESET);
  } else {
    Serial.printf("  Last error: %snever%s\n", staleTimeColor(true), LOG_COLOR_RESET);
  }

  if (!lastErr.ok()) {
    Serial.printf("  Error code: %s%s%s\n",
                  LOG_COLOR_RED,
                  errToStr(lastErr.code),
                  LOG_COLOR_RESET);
    Serial.printf("  Error detail: %ld\n", static_cast<long>(lastErr.detail));
    if (lastErr.msg && lastErr.msg[0]) {
      Serial.printf("  Error msg: %s%s%s\n", LOG_COLOR_YELLOW, lastErr.msg, LOG_COLOR_RESET);
    }
  }
}

void printHealthMonitorState() {
  Serial.printf("  Health monitor: %s%s%s interval=%lu ms\n",
                onOffColor(healthMonitorEnabled),
                healthMonitorEnabled ? "ON" : "OFF",
                LOG_COLOR_RESET,
                static_cast<unsigned long>(healthMonitorIntervalMs));
}

uint16_t packThresholdRaw(const OPT4001::Threshold& threshold) {
  return static_cast<uint16_t>(
      (static_cast<uint16_t>(threshold.exponent) << OPT4001::cmd::BIT_THRESHOLD_EXPONENT) |
      (threshold.result & OPT4001::cmd::MASK_THRESHOLD_RESULT));
}

void printThresholdDetails(const char* name, const OPT4001::Threshold& threshold) {
  Serial.printf("  %-4s raw=(%u,0x%03X) packed=0x%04X adc=%lu lux=%.6f\n",
                name,
                threshold.exponent,
                threshold.result,
                packThresholdRaw(threshold),
                static_cast<unsigned long>(device.thresholdToAdcCodes(threshold)),
                device.thresholdToLux(threshold));
}

void printAddressInfo() {
  const OPT4001::Config cfg = device.isInitialized() ? device.getConfig() : makeDefaultConfig();
  Serial.println("=== Addressing ===");
  Serial.printf("  Package: %s\n", packageToStr(cfg.packageVariant));
  Serial.printf("  Current address: 0x%02X\n", cfg.i2cAddress);
  if (cfg.packageVariant == OPT4001::PackageVariant::PICOSTAR) {
    Serial.printf("  Valid addresses: 0x%02X only\n", OPT4001::cmd::I2C_ADDR_DEFAULT);
  } else {
    Serial.printf("  Valid addresses: 0x%02X / 0x%02X / 0x%02X\n",
                  OPT4001::cmd::I2C_ADDR_GND,
                  OPT4001::cmd::I2C_ADDR_DEFAULT,
                  OPT4001::cmd::I2C_ADDR_SDA);
  }
}

void printSampleAge() {
  const uint32_t ts = device.sampleTimestampMs();
  if (ts == 0U) {
    Serial.printf("  Sample age: %snever%s\n", LOG_COLOR_YELLOW, LOG_COLOR_RESET);
    return;
  }
  const uint32_t age = device.sampleAgeMs(millis());
  Serial.printf("  Sample age: %lu ms (at %lu ms)\n",
                static_cast<unsigned long>(age),
                static_cast<unsigned long>(ts));
}

void printSnapshot() {
  OPT4001::SettingsSnapshot snap;
  OPT4001::Status st = device.getSettings(snap);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  const bool online = snap.state == OPT4001::DriverState::READY ||
                      snap.state == OPT4001::DriverState::DEGRADED;
  const uint32_t ageMs = snap.sampleTimestampMs > 0U ? (millis() - snap.sampleTimestampMs) : 0U;

  Serial.println("=== Cached Settings ===");
  Serial.printf("  Initialized: %s\n", log_bool_str(snap.initialized));
  Serial.printf("  State: %s%s%s\n",
                stateColor(snap.state, online, 0),
                stateToStr(snap.state),
                LOG_COLOR_RESET);
  Serial.printf("  Package / address: %s / 0x%02X\n",
                packageToStr(snap.packageVariant),
                snap.i2cAddress);
  Serial.printf("  Timeout / offline threshold: %lu ms / %u\n",
                static_cast<unsigned long>(snap.i2cTimeoutMs),
                snap.offlineThreshold);
  Serial.printf("  Hooks now/gpio/yield: %s / %s / %s\n",
                log_bool_str(snap.hasNowMsHook),
                log_bool_str(snap.hasGpioReadHook),
                log_bool_str(snap.hasCooperativeYieldHook));
  Serial.printf("  INT pin: %d  verify CRC: %s\n",
                snap.intPin,
                log_bool_str(snap.verifyCrc));
  Serial.printf("  Mode / pending: %s / %s\n",
                modeToStr(snap.mode),
                modeToStr(snap.pendingMode));
  Serial.printf("  Range / ctime: %s / %s\n",
                rangeToStr(snap.range),
                conversionTimeToStr(snap.conversionTime));
  Serial.printf("  Quick wake / burst: %s / %s\n",
                log_bool_str(snap.quickWake),
                log_bool_str(snap.burstMode));
  Serial.printf("  Latch / polarity / faults: %s / %s / %s\n",
                latchToStr(snap.interruptLatch),
                polarityToStr(snap.interruptPolarity),
                faultCountToStr(snap.faultCount));
  Serial.printf("  INT dir / cfg: %s / %s\n",
                intDirectionToStr(snap.intDirection),
                intConfigToStr(snap.intConfig));
  Serial.printf("  Conversion started / ready / sample available: %s / %s / %s\n",
                log_bool_str(snap.conversionStarted),
                log_bool_str(snap.conversionReady),
                log_bool_str(snap.sampleAvailable));
  Serial.printf("  Conversion start: %lu ms\n",
                static_cast<unsigned long>(snap.conversionStartMs));
  Serial.printf("  Cached sample valid / age: %s / %lu ms\n",
                log_bool_str(snap.lastSampleValid),
                static_cast<unsigned long>(ageMs));
  Serial.printf("  Cached lux / adc / ctr: %.6f lx / %lu / %u\n",
                snap.lastLux,
                static_cast<unsigned long>(snap.lastAdcCodes),
                snap.lastCounter);
  Serial.printf("  Thresholds low=(%u,0x%03X) high=(%u,0x%03X)\n",
                snap.lowThreshold.exponent,
                snap.lowThreshold.result,
                snap.highThreshold.exponent,
                snap.highThreshold.result);
}

void printDeviceIdInfo() {
  OPT4001::DeviceIdInfo info;
  OPT4001::Status st = device.readDeviceId(info);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  Serial.println("=== DEVICE_ID ===");
  Serial.printf("  Raw: 0x%04X\n", info.raw);
  Serial.printf("  DIDH: 0x%03X\n", info.didh);
  Serial.printf("  DIDL: %u\n", info.didl);
  Serial.printf("  Expected DIDH / DIDL: 0x%03X / 0\n", OPT4001::cmd::DIDH_EXPECTED);
  Serial.printf("  Matches expected: %s%s%s\n",
                info.matchesExpected ? LOG_COLOR_GREEN : LOG_COLOR_RED,
                log_bool_str(info.matchesExpected),
                LOG_COLOR_RESET);
}

void printConfigurationInfo() {
  OPT4001::ConfigurationInfo info;
  OPT4001::Status st = device.readConfiguration(info);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  Serial.println("=== CONFIGURATION ===");
  Serial.printf("  Raw: 0x%04X\n", info.raw);
  Serial.printf("  Quick wake: %s\n", log_bool_str(info.quickWake));
  Serial.printf("  Range: %s\n", rangeToStr(info.range));
  Serial.printf("  Conversion time: %s\n", conversionTimeToStr(info.conversionTime));
  Serial.printf("  Mode: %s\n", modeToStr(info.mode));
  Serial.printf("  Interrupt latch: %s\n", latchToStr(info.interruptLatch));
  Serial.printf("  Interrupt polarity: %s\n", polarityToStr(info.interruptPolarity));
  Serial.printf("  Fault count: %s\n", faultCountToStr(info.faultCount));
  Serial.printf("  Full-scale / resolution: %.3f lx / %.9f lx\n",
                device.getRangeFullScaleLux(info.range),
                device.getRangeResolutionLux(info.range, info.conversionTime));
  Serial.printf("  Reserved bit set: %s\n", log_bool_str(info.reservedBitSet));
  Serial.printf("  Decode valid: %s\n", log_bool_str(info.valid));
}

void printIntConfigurationInfo() {
  OPT4001::IntConfigurationInfo info;
  OPT4001::Status st = device.readIntConfiguration(info);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  Serial.println("=== INT_CONFIGURATION ===");
  Serial.printf("  Raw: 0x%04X\n", info.raw);
  Serial.printf("  INT direction: %s\n", intDirectionToStr(info.intDirection));
  Serial.printf("  INT config: %s\n", intConfigToStr(info.intConfig));
  Serial.printf("  INT behavior: %s\n", intConfigBehaviorToStr(info.intConfig));
  Serial.printf("  Burst mode: %s\n", log_bool_str(info.burstMode));
  Serial.printf("  Fixed pattern valid: %s\n", log_bool_str(info.fixedPatternValid));
  Serial.printf("  Reserved bit set: %s\n", log_bool_str(info.reservedBitSet));
  Serial.printf("  Decode valid: %s\n", log_bool_str(info.valid));
  if (info.intDirection == OPT4001::IntDirection::PIN_INPUT) {
    Serial.printf("  Note: %sboard/application must generate the INT trigger pulse%s\n",
                  LOG_COLOR_YELLOW,
                  LOG_COLOR_RESET);
  }
  if (!info.valid) {
    Serial.printf("  Note: %sINT_CFG=2 is reserved/invalid per datasheet%s\n",
                  LOG_COLOR_RED,
                  LOG_COLOR_RESET);
  }
}

OPT4001::Threshold unpackThreshold(uint16_t raw) {
  OPT4001::Threshold threshold;
  threshold.exponent = static_cast<uint8_t>(
      (raw & OPT4001::cmd::MASK_THRESHOLD_EXPONENT) >> OPT4001::cmd::BIT_THRESHOLD_EXPONENT);
  threshold.result = static_cast<uint16_t>(raw & OPT4001::cmd::MASK_THRESHOLD_RESULT);
  return threshold;
}

void printThresholdLux() {
  OPT4001::Threshold low;
  OPT4001::Threshold high;
  OPT4001::Status st = device.getThresholds(low, high);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  Serial.println("=== Thresholds ===");
  printThresholdDetails("Low", low);
  printThresholdDetails("High", high);
}

void printLiveConfig() {
  if (!device.isInitialized()) {
    LOGW("Driver not initialized.");
    return;
  }

  printAddressInfo();
  printDeviceIdInfo();
  printConfigurationInfo();
  printIntConfigurationInfo();
  printThresholdLux();
}

void printDiagnosticReport() {
  Serial.println("=== OPT4001 Diagnostic Report ===");
  printHealthView(device);
  printDriverHealth();
  printHealthMonitorState();
  if (!device.isInitialized()) {
    Serial.printf("  Note: %sdriver not initialized; register diagnostics skipped%s\n",
                  LOG_COLOR_YELLOW,
                  LOG_COLOR_RESET);
    return;
  }
  printAddressInfo();
  printDeviceIdInfo();
  printConfigurationInfo();
  printIntConfigurationInfo();
  printScale();
  printThresholdLux();
  printSampleAge();
  OPT4001::Sample sample;
  if (device.getLastSample(sample).ok()) {
    Serial.println("=== Cached Sample ===");
    printSample(sample);
  }
  printSnapshot();
  Serial.printf("  FLAGS note: %suse 'status' or 'status_raw' explicitly; FLAGS is clear-on-read%s\n",
                LOG_COLOR_YELLOW,
                LOG_COLOR_RESET);
}

void printScale() {
  Serial.println("=== Scale / Timing ===");
  Serial.printf("  Package: %s\n", packageToStr(device.getPackageVariant()));
  Serial.printf("  LSB: %.9f lx/code\n", device.getLuxLsb());
  Serial.printf("  Current range: %s\n", rangeToStr(device.getRange()));
  Serial.printf("  Current full-scale: %.3f lx\n", device.getCurrentFullScaleLux());
  Serial.printf("  Current resolution: %.9f lx\n", device.getCurrentResolutionLux());
  Serial.printf("  Effective bits: %u\n", static_cast<unsigned>(device.getEffectiveBits()));
  Serial.printf("  Conversion time: %s (%lu us)\n",
                conversionTimeToStr(device.getConversionTime()),
                static_cast<unsigned long>(device.getConversionTimeUs()));
  Serial.printf("  One-shot budgets: regular=%lu us forced=%lu us\n",
                static_cast<unsigned long>(device.getOneShotBudgetUs(OPT4001::Mode::ONE_SHOT)),
                static_cast<unsigned long>(device.getOneShotBudgetUs(OPT4001::Mode::ONE_SHOT_FORCED_AUTO)));
  Serial.println("  Range table:");
  for (uint8_t idx = 0; idx <= 8U; ++idx) {
    const auto range = static_cast<OPT4001::Range>(idx);
    Serial.printf("    %-8s full-scale=%9.3f lx  resolution=%0.9f lx\n",
                  rangeToStr(range),
                  device.getRangeFullScaleLux(range),
                  device.getRangeResolutionLux(range, device.getConversionTime()));
  }
}

void printAdcToLux(uint32_t adcCodes) {
  Serial.println("=== ADC Codes -> Lux ===");
  Serial.printf("  Package: %s\n", packageToStr(device.getPackageVariant()));
  Serial.printf("  ADC codes: %lu\n", static_cast<unsigned long>(adcCodes));
  Serial.printf("  Lux: %.6f lx\n", device.adcCodesToLux(adcCodes));
}

void printRawFieldsToLux(uint8_t exponent, uint32_t mantissa) {
  Serial.println("=== Raw Fields -> Lux ===");
  Serial.printf("  Package: %s\n", packageToStr(device.getPackageVariant()));
  Serial.printf("  Exponent / mantissa: %u / 0x%05lX\n",
                static_cast<unsigned>(exponent),
                static_cast<unsigned long>(mantissa));
  Serial.printf("  ADC codes: %lu\n",
                static_cast<unsigned long>(mantissa << exponent));
  Serial.printf("  Lux: %.6f lx\n", device.rawToLux(exponent, mantissa));
}

void printThresholdEncodingFromLux(float lux) {
  OPT4001::Threshold threshold;
  OPT4001::Status st = device.luxToThreshold(lux, threshold);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  Serial.println("=== Threshold Encode ===");
  Serial.printf("  Input lux: %.6f lx\n", lux);
  printThresholdDetails("Calc", threshold);
}

void printThresholdDecodingFromRaw(uint16_t raw) {
  const OPT4001::Threshold threshold = unpackThreshold(raw);
  Serial.println("=== Threshold Decode ===");
  Serial.printf("  Raw register: 0x%04X\n", raw);
  printThresholdDetails("Calc", threshold);
}

bool rebeginWithConfig(const OPT4001::Config& cfg) {
  device.end();
  OPT4001::Status st = device.begin(cfg);
  printStatus(st);
  if (st.ok()) {
    printDriverHealth();
  }
  return st.ok();
}

void printRegisterDump() {
  if (!device.isInitialized()) {
    LOGW("Driver not initialized.");
    return;
  }

  struct DumpReg {
    uint8_t addr;
    const char* name;
    bool clearOnRead;
  };

  static constexpr DumpReg kRegs[] = {
    {OPT4001::cmd::REG_RESULT, "RESULT", false},
    {OPT4001::cmd::REG_RESULT_LSB_CRC, "RESULT_LSB_CRC", false},
    {OPT4001::cmd::REG_FIFO0_MSB, "FIFO0_MSB", false},
    {OPT4001::cmd::REG_FIFO0_LSB_CRC, "FIFO0_LSB_CRC", false},
    {OPT4001::cmd::REG_FIFO1_MSB, "FIFO1_MSB", false},
    {OPT4001::cmd::REG_FIFO1_LSB_CRC, "FIFO1_LSB_CRC", false},
    {OPT4001::cmd::REG_FIFO2_MSB, "FIFO2_MSB", false},
    {OPT4001::cmd::REG_FIFO2_LSB_CRC, "FIFO2_LSB_CRC", false},
    {OPT4001::cmd::REG_THRESHOLD_L, "THRESHOLD_L", false},
    {OPT4001::cmd::REG_THRESHOLD_H, "THRESHOLD_H", false},
    {OPT4001::cmd::REG_CONFIGURATION, "CONFIGURATION", false},
    {OPT4001::cmd::REG_INT_CONFIGURATION, "INT_CONFIGURATION", false},
    {OPT4001::cmd::REG_FLAGS, "FLAGS", true},
    {OPT4001::cmd::REG_DEVICE_ID, "DEVICE_ID", false},
  };

  Serial.println("=== Register Dump ===");
  for (const DumpReg& reg : kRegs) {
    uint16_t value = 0;
    OPT4001::Status st = device.readRegister16(reg.addr, value);
    if (!st.ok()) {
      Serial.printf("  0x%02X %-18s : <error>\n", reg.addr, reg.name);
      printStatus(st);
      return;
    }
    Serial.printf("  0x%02X %-18s : 0x%04X%s\n",
                  reg.addr,
                  reg.name,
                  value,
                  reg.clearOnRead ? "  (clear-on-read)" : "");
  }
}

void printBurstFrame(const OPT4001::BurstFrame& frame) {
  auto printNamed = [](const char* name, const OPT4001::Sample& sample) {
    Serial.printf("  %s%s%s\n", LOG_COLOR_CYAN, name, LOG_COLOR_RESET);
    printSample(sample);
  };

  Serial.println("=== Burst Frame ===");
  printNamed("RESULT/newest", frame.newest);
  printNamed("FIFO0", frame.fifo0);
  printNamed("FIFO1", frame.fifo1);
  printNamed("FIFO2", frame.fifo2);
  Serial.printf("  Counter delta: newest<-fifo0=%u fifo0<-fifo1=%u fifo1<-fifo2=%u\n",
                static_cast<unsigned>(device.sampleCounterDelta(frame.fifo0.counter, frame.newest.counter)),
                static_cast<unsigned>(device.sampleCounterDelta(frame.fifo1.counter, frame.fifo0.counter)),
                static_cast<unsigned>(device.sampleCounterDelta(frame.fifo2.counter, frame.fifo1.counter)));
}

void printFlagsDecoded() {
  OPT4001::Flags flags;
  OPT4001::Status st = device.readFlags(flags);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  Serial.println("=== FLAGS / Status ===");
  Serial.printf("  Raw: 0x%04X\n", flags.raw);
  Serial.printf("  Overload: %s%s%s\n",
                yesNoColor(flags.overload),
                flags.overload ? "YES" : "NO",
                LOG_COLOR_RESET);
  Serial.printf("  Conversion ready: %s%s%s\n",
                yesNoColor(flags.conversionReady),
                flags.conversionReady ? "YES" : "NO",
                LOG_COLOR_RESET);
  Serial.printf("  High threshold: %s%s%s\n",
                yesNoColor(flags.highThreshold),
                flags.highThreshold ? "YES" : "NO",
                LOG_COLOR_RESET);
  Serial.printf("  Low threshold: %s%s%s\n",
                yesNoColor(flags.lowThreshold),
                flags.lowThreshold ? "YES" : "NO",
                LOG_COLOR_RESET);
  if (flags.overload) {
    Serial.printf("  Hint: %smanual-range operation may need a higher range; auto-range should recover on the next conversion%s\n",
                  LOG_COLOR_YELLOW,
                  LOG_COLOR_RESET);
  }
  if (flags.highThreshold || flags.lowThreshold) {
    Serial.printf("  Threshold mode: latch=%s int_cfg=%s\n",
                  latchToStr(device.getInterruptLatch()),
                  intConfigBehaviorToStr(device.getIntConfig()));
  }
  Serial.printf("  Note: %sreading FLAGS clears latched flags on the device%s\n",
                LOG_COLOR_YELLOW,
                LOG_COLOR_RESET);
}

void printFlagsRawOnly() {
  uint16_t raw = 0;
  OPT4001::Status st = device.readFlagsRaw(raw);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  Serial.printf("  FLAGS raw = 0x%04X (clear-on-read)\n", raw);
}

bool primePowerDownSample(OPT4001::Mode mode) {
  if (device.getMode() != OPT4001::Mode::POWER_DOWN) {
    return true;
  }

  OPT4001::Sample sample;
  OPT4001::Status st = device.readBlocking(sample, mode, BLOCKING_READ_TIMEOUT_MS);
  if (!sampleStatusHasData(st)) {
    printStatus(st);
    return false;
  }
  LOGV(verboseMode, "Prime sample complete before cached read path.");
  if (sampleStatusWarn(st)) {
    Serial.println("  Prime sample completed with warning:");
    printStatus(st);
  }
  return true;
}

bool blockingReadAndPrint(OPT4001::Mode mode) {
  OPT4001::Sample sample;
  OPT4001::Status st = device.readBlocking(sample, mode, BLOCKING_READ_TIMEOUT_MS);
  if (!sampleStatusHasData(st)) {
    printStatus(st);
    return false;
  }
  printSample(sample);
  if (sampleStatusWarn(st)) {
    printStatus(st);
  }
  return true;
}

void runStress(int32_t count) {
  uint32_t okCount = 0;
  uint32_t warnCount = 0;
  uint32_t failCount = 0;
  uint32_t counterGapCount = 0;
  bool hasWarn = false;
  bool hasFail = false;
  bool haveSample = false;
  bool havePrevCounter = false;
  float minLux = 0.0f;
  float maxLux = 0.0f;
  uint32_t minAdc = 0U;
  uint32_t maxAdc = 0U;
  uint8_t prevCounter = 0U;
  OPT4001::Status firstWarn = OPT4001::Status::Ok();
  OPT4001::Status lastWarn = OPT4001::Status::Ok();
  OPT4001::Status firstFail = OPT4001::Status::Ok();
  OPT4001::Status lastFail = OPT4001::Status::Ok();
  HealthSnapshot<OPT4001::OPT4001> before;
  before.capture(device);
  const uint32_t startMs = millis();

  auto recordSampleStats = [&](const OPT4001::Sample& sample) {
    if (!haveSample) {
      haveSample = true;
      minLux = sample.lux;
      maxLux = sample.lux;
      minAdc = sample.adcCodes;
      maxAdc = sample.adcCodes;
    } else {
      if (sample.lux < minLux) minLux = sample.lux;
      if (sample.lux > maxLux) maxLux = sample.lux;
      if (sample.adcCodes < minAdc) minAdc = sample.adcCodes;
      if (sample.adcCodes > maxAdc) maxAdc = sample.adcCodes;
    }

    if (havePrevCounter && device.sampleCounterDelta(prevCounter, sample.counter) != 1U) {
      counterGapCount++;
    }
    prevCounter = sample.counter;
    havePrevCounter = true;
  };

  for (int32_t i = 0; i < count; ++i) {
    OPT4001::Sample sample;
    OPT4001::Status st = device.readBlocking(sample, BLOCKING_READ_TIMEOUT_MS);
    if (st.ok()) {
      okCount++;
      recordSampleStats(sample);
      LOGV(verboseMode, "[%ld] lux=%.6f adc=%lu ctr=%u",
           static_cast<long>(i + 1),
           sample.lux,
           static_cast<unsigned long>(sample.adcCodes),
           sample.counter);
    } else if (sampleStatusWarn(st)) {
      warnCount++;
      recordSampleStats(sample);
      if (!hasWarn) {
        firstWarn = st;
        hasWarn = true;
      }
      lastWarn = st;
      LOGV(verboseMode, "[%ld] CRC warning lux=%.6f adc=%lu ctr=%u",
           static_cast<long>(i + 1),
           sample.lux,
           static_cast<unsigned long>(sample.adcCodes),
           sample.counter);
    } else {
      failCount++;
      if (!hasFail) {
        firstFail = st;
        hasFail = true;
      }
      lastFail = st;
      if (verboseMode) {
        printStatus(st);
      }
    }
    printStressProgress(static_cast<uint32_t>(i + 1),
                        static_cast<uint32_t>(count),
                        okCount,
                        warnCount,
                        failCount);
  }

  const uint32_t elapsedMs = millis() - startMs;
  const uint32_t successLike = okCount + warnCount;
  const float pct = (count > 0)
                        ? (100.0f * static_cast<float>(successLike) / static_cast<float>(count))
                        : 0.0f;
  HealthSnapshot<OPT4001::OPT4001> after;
  after.capture(device);

  Serial.println("=== stress summary ===");
  Serial.printf("  Results: %sok=%lu%s %swarn=%lu%s %sfail=%lu%s (%s%.2f%%%s success-like)\n",
                goodIfNonZeroColor(okCount),
                static_cast<unsigned long>(okCount),
                LOG_COLOR_RESET,
                warnCountColor(warnCount),
                static_cast<unsigned long>(warnCount),
                LOG_COLOR_RESET,
                goodIfZeroColor(failCount),
                static_cast<unsigned long>(failCount),
                LOG_COLOR_RESET,
                successRateColor(pct),
                pct,
                LOG_COLOR_RESET);
  Serial.printf("  Duration: %lu ms\n", static_cast<unsigned long>(elapsedMs));
  if (elapsedMs > 0U) {
    Serial.printf("  Rate: %.2f ops/s\n",
                  (1000.0f * static_cast<float>(count)) / static_cast<float>(elapsedMs));
  }
  if (haveSample) {
    Serial.printf("  Lux range: %.6f .. %.6f lx\n", minLux, maxLux);
    Serial.printf("  ADC range: %lu .. %lu\n",
                  static_cast<unsigned long>(minAdc),
                  static_cast<unsigned long>(maxAdc));
    Serial.printf("  Counter gaps: %s%lu%s\n",
                  warnCountColor(counterGapCount),
                  static_cast<unsigned long>(counterGapCount),
                  LOG_COLOR_RESET);
  }
  Serial.println("  Health changes:");
  printHealthDiff(before, after);

  if (hasWarn) {
    Serial.println("  First warning:");
    printStatus(firstWarn);
    if (warnCount > 1U) {
      Serial.println("  Last warning:");
      printStatus(lastWarn);
    }
  }
  if (hasFail) {
    Serial.println("  First failure:");
    printStatus(firstFail);
    if (failCount > 1U) {
      Serial.println("  Last failure:");
      printStatus(lastFail);
    }
  }
}

void runStressMix(int32_t count) {
  struct OpStats {
    const char* name;
    uint32_t ok;
    uint32_t warn;
    uint32_t fail;
  };

  OpStats stats[] = {
    {"readBlocking", 0U, 0U, 0U},
    {"readBurst", 0U, 0U, 0U},
    {"readSlot0", 0U, 0U, 0U},
    {"readConfig", 0U, 0U, 0U},
    {"readIntCfg", 0U, 0U, 0U},
    {"readDeviceId", 0U, 0U, 0U},
    {"readFlagsRaw", 0U, 0U, 0U},
    {"probe", 0U, 0U, 0U},
  };

  uint32_t okCount = 0;
  uint32_t warnCount = 0;
  uint32_t failCount = 0;
  uint32_t probeSideEffectCount = 0;
  bool hasWarn = false;
  bool hasFail = false;
  bool haveSample = false;
  bool havePrevCounter = false;
  float minLux = 0.0f;
  float maxLux = 0.0f;
  uint32_t counterGapCount = 0;
  uint8_t prevCounter = 0U;
  OPT4001::Status firstWarn = OPT4001::Status::Ok();
  OPT4001::Status lastWarn = OPT4001::Status::Ok();
  OPT4001::Status firstFail = OPT4001::Status::Ok();
  OPT4001::Status lastFail = OPT4001::Status::Ok();
  HealthSnapshot<OPT4001::OPT4001> before;
  before.capture(device);
  const uint32_t startMs = millis();
  const size_t opCount = sizeof(stats) / sizeof(stats[0]);

  auto record = [&](size_t idx, const OPT4001::Status& st) {
    if (st.ok()) {
      stats[idx].ok++;
      okCount++;
      return;
    }
    if (sampleStatusWarn(st)) {
      stats[idx].warn++;
      warnCount++;
      if (!hasWarn) {
        firstWarn = st;
        hasWarn = true;
      }
      lastWarn = st;
      return;
    }
    stats[idx].fail++;
    failCount++;
    if (!hasFail) {
      firstFail = st;
      hasFail = true;
    }
    lastFail = st;
  };

  auto recordSampleStats = [&](const OPT4001::Sample& sample) {
    if (!haveSample) {
      haveSample = true;
      minLux = sample.lux;
      maxLux = sample.lux;
    } else {
      if (sample.lux < minLux) minLux = sample.lux;
      if (sample.lux > maxLux) maxLux = sample.lux;
    }

    if (havePrevCounter && device.sampleCounterDelta(prevCounter, sample.counter) != 1U) {
      counterGapCount++;
    }
    prevCounter = sample.counter;
    havePrevCounter = true;
  };

  for (int32_t i = 0; i < count; ++i) {
    const size_t op = static_cast<size_t>(i) % opCount;
    OPT4001::Status st = OPT4001::Status::Ok();

    switch (op) {
      case 0: {
        OPT4001::Sample sample;
        st = device.readBlocking(sample, BLOCKING_READ_TIMEOUT_MS);
        if (sampleStatusHasData(st)) {
          recordSampleStats(sample);
        }
        break;
      }
      case 1: {
        OPT4001::BurstFrame frame;
        st = device.readBurst(frame);
        if (sampleStatusHasData(st)) {
          recordSampleStats(frame.newest);
        }
        break;
      }
      case 2: {
        OPT4001::Sample sample;
        st = device.readSampleSlot(0, sample);
        if (sampleStatusHasData(st)) {
          recordSampleStats(sample);
        }
        break;
      }
      case 3: {
        OPT4001::ConfigurationInfo info;
        st = device.readConfiguration(info);
        break;
      }
      case 4: {
        OPT4001::IntConfigurationInfo info;
        st = device.readIntConfiguration(info);
        break;
      }
      case 5: {
        OPT4001::DeviceIdInfo info;
        st = device.readDeviceId(info);
        break;
      }
      case 6: {
        uint16_t raw = 0;
        st = device.readFlagsRaw(raw);
        break;
      }
      case 7: {
        HealthSnapshot<OPT4001::OPT4001> probeBefore;
        probeBefore.capture(device);
        st = device.probe();
        HealthSnapshot<OPT4001::OPT4001> probeAfter;
        probeAfter.capture(device);
        if (probeBefore.state != probeAfter.state ||
            probeBefore.online != probeAfter.online ||
            probeBefore.consecutiveFailures != probeAfter.consecutiveFailures ||
            probeBefore.totalFailures != probeAfter.totalFailures ||
            probeBefore.totalSuccess != probeAfter.totalSuccess) {
          probeSideEffectCount++;
          LOGV(verboseMode, "[%ld] probe changed tracked health unexpectedly",
               static_cast<long>(i + 1));
        }
        break;
      }
      default:
        break;
    }

    record(op, st);
    if (!st.ok() && verboseMode) {
      LOGV(verboseMode, "[%ld] %s -> %s",
           static_cast<long>(i + 1),
           stats[op].name,
           errToStr(st.code));
    }
    printStressProgress(static_cast<uint32_t>(i + 1),
                        static_cast<uint32_t>(count),
                        okCount,
                        warnCount,
                        failCount);
  }

  const uint32_t elapsedMs = millis() - startMs;
  const uint32_t successLike = okCount + warnCount;
  const float pct = (count > 0)
                        ? (100.0f * static_cast<float>(successLike) / static_cast<float>(count))
                        : 0.0f;
  HealthSnapshot<OPT4001::OPT4001> after;
  after.capture(device);

  Serial.println("=== stress_mix summary ===");
  Serial.printf("  Total: %sok=%lu%s %swarn=%lu%s %sfail=%lu%s (%s%.2f%%%s success-like)\n",
                goodIfNonZeroColor(okCount),
                static_cast<unsigned long>(okCount),
                LOG_COLOR_RESET,
                warnCountColor(warnCount),
                static_cast<unsigned long>(warnCount),
                LOG_COLOR_RESET,
                goodIfZeroColor(failCount),
                static_cast<unsigned long>(failCount),
                LOG_COLOR_RESET,
                successRateColor(pct),
                pct,
                LOG_COLOR_RESET);
  Serial.printf("  Duration: %lu ms\n", static_cast<unsigned long>(elapsedMs));
  if (elapsedMs > 0U) {
    Serial.printf("  Rate: %.2f ops/s\n",
                  (1000.0f * static_cast<float>(count)) / static_cast<float>(elapsedMs));
  }
  if (haveSample) {
    Serial.printf("  Sample lux range: %.6f .. %.6f lx\n", minLux, maxLux);
    Serial.printf("  Sample counter gaps: %s%lu%s\n",
                  warnCountColor(counterGapCount),
                  static_cast<unsigned long>(counterGapCount),
                  LOG_COLOR_RESET);
  }
  Serial.printf("  Probe side effects: %s%lu%s\n",
                goodIfZeroColor(probeSideEffectCount),
                static_cast<unsigned long>(probeSideEffectCount),
                LOG_COLOR_RESET);
  for (size_t i = 0; i < opCount; ++i) {
    Serial.printf("  %-12s %sok=%lu%s %swarn=%lu%s %sfail=%lu%s\n",
                  stats[i].name,
                  goodIfNonZeroColor(stats[i].ok),
                  static_cast<unsigned long>(stats[i].ok),
                  LOG_COLOR_RESET,
                  warnCountColor(stats[i].warn),
                  static_cast<unsigned long>(stats[i].warn),
                  LOG_COLOR_RESET,
                  goodIfZeroColor(stats[i].fail),
                  static_cast<unsigned long>(stats[i].fail),
                  LOG_COLOR_RESET);
  }
  Serial.println("  Health changes:");
  printHealthDiff(before, after);

  if (hasWarn) {
    Serial.println("  First warning:");
    printStatus(firstWarn);
    if (warnCount > 1U) {
      Serial.println("  Last warning:");
      printStatus(lastWarn);
    }
  }
  if (hasFail) {
    Serial.println("  First failure:");
    printStatus(firstFail);
    if (failCount > 1U) {
      Serial.println("  Last failure:");
      printStatus(lastFail);
    }
  }
}

void runSelfTest() {
  struct TestStats {
    uint32_t pass = 0;
    uint32_t fail = 0;
    uint32_t skip = 0;
  } stats;

  enum class SelftestOutcome : uint8_t { PASS, FAIL, SKIP };
  auto report = [&](const char* name, SelftestOutcome outcome, const char* note) {
    const bool passed = (outcome == SelftestOutcome::PASS);
    const bool skipped = (outcome == SelftestOutcome::SKIP);
    const char* color = skipped ? LOG_COLOR_YELLOW : LOG_COLOR_RESULT(passed);
    const char* tag = skipped ? "SKIP" : (passed ? "PASS" : "FAIL");
    Serial.printf("  [%s%s%s] %s", color, tag, LOG_COLOR_RESET, name);
    if (note && note[0]) {
      Serial.printf(" - %s", note);
    }
    Serial.println();
    if (skipped) {
      stats.skip++;
    } else if (passed) {
      stats.pass++;
    } else {
      stats.fail++;
    }
  };
  auto reportCheck = [&](const char* name, bool passed, const char* note) {
    report(name, passed ? SelftestOutcome::PASS : SelftestOutcome::FAIL, note);
  };
  auto reportSkip = [&](const char* name, const char* note) {
    report(name, SelftestOutcome::SKIP, note);
  };

  Serial.println("=== OPT4001 selftest (safe commands) ===");

  const uint32_t succBefore = device.totalSuccess();
  const uint32_t failBefore = device.totalFailures();
  const uint8_t consBefore = device.consecutiveFailures();

  const OPT4001::Status pst = device.probe();
  if (pst.code == OPT4001::Err::NOT_INITIALIZED) {
    reportSkip("probe responds", "driver not initialized");
    reportSkip("remaining checks", "selftest aborted");
    Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                  goodIfNonZeroColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                  goodIfZeroColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                  skipCountColor(stats.skip), static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
    return;
  }

  const bool probeHealthUnchanged =
      device.totalSuccess() == succBefore &&
      device.totalFailures() == failBefore &&
      device.consecutiveFailures() == consBefore;
  reportCheck("probe responds", pst.ok(), pst.ok() ? "" : errToStr(pst.code));
  reportCheck("probe no-health-side-effects", probeHealthUnchanged, "");

  uint16_t didRaw = 0;
  OPT4001::Status st = device.readDeviceId(didRaw);
  reportCheck("readDeviceId(raw)", st.ok(), st.ok() ? "" : errToStr(st.code));
  if (st.ok()) {
    reportCheck("device id raw expected", didRaw == OPT4001::cmd::DEVICE_ID_RESET, "");
  }

  OPT4001::DeviceIdInfo did;
  st = device.readDeviceId(did);
  reportCheck("readDeviceId(decoded)", st.ok(), st.ok() ? "" : errToStr(st.code));
  if (st.ok()) {
    reportCheck("device id matches", did.matchesExpected, "");
  }

  uint16_t cfgRaw = 0;
  st = device.readConfiguration(cfgRaw);
  reportCheck("readConfiguration(raw)", st.ok(), st.ok() ? "" : errToStr(st.code));

  OPT4001::ConfigurationInfo cfgInfo;
  st = device.readConfiguration(cfgInfo);
  reportCheck("readConfiguration(decoded)", st.ok(), st.ok() ? "" : errToStr(st.code));
  if (st.ok()) {
    reportCheck("configuration decode valid", cfgInfo.valid, "");
  }

  uint16_t intCfgRaw = 0;
  st = device.readIntConfiguration(intCfgRaw);
  reportCheck("readIntConfiguration(raw)", st.ok(), st.ok() ? "" : errToStr(st.code));

  OPT4001::IntConfigurationInfo intInfo;
  st = device.readIntConfiguration(intInfo);
  reportCheck("readIntConfiguration(decoded)", st.ok(), st.ok() ? "" : errToStr(st.code));
  if (st.ok()) {
    reportCheck("intcfg decode valid", intInfo.valid, "");
    reportCheck("intcfg fixed pattern valid", intInfo.fixedPatternValid, "");
  }

  uint8_t regBlock[8] = {};
  st = device.readRegisters(OPT4001::cmd::REG_RESULT, regBlock, sizeof(regBlock));
  reportCheck("readRegisters(0x00,8)", st.ok(), st.ok() ? "" : errToStr(st.code));

  OPT4001::Threshold low;
  OPT4001::Threshold high;
  st = device.getThresholds(low, high);
  reportCheck("getThresholds", st.ok(), st.ok() ? "" : errToStr(st.code));

  float lowLux = 0.0f;
  float highLux = 0.0f;
  st = device.getThresholdsLux(lowLux, highLux);
  reportCheck("getThresholdsLux", st.ok(), st.ok() ? "" : errToStr(st.code));
  reportCheck("scale helpers sane",
              device.getCurrentFullScaleLux() > 0.0f &&
                  device.getCurrentResolutionLux() > 0.0f &&
                  device.getEffectiveBits() > 0U,
              "");

  OPT4001::Sample sample;
  st = device.readBlocking(sample, BLOCKING_READ_TIMEOUT_MS);
  reportCheck("readBlocking", sampleStatusHasData(st), sampleStatusHasData(st) ? "" : errToStr(st.code));
  if (sampleStatusHasData(st)) {
    reportCheck("sample lux non-negative", sample.lux >= 0.0f, "");
    const float helperLux = device.adcCodesToLux(sample.adcCodes);
    const float rawLux = device.rawToLux(sample.exponent, sample.mantissa);
    const float tol = (sample.lux * 0.000001f) + 0.000001f;
    const float adcDiff = (helperLux >= sample.lux) ? (helperLux - sample.lux) : (sample.lux - helperLux);
    const float rawDiff = (rawLux >= sample.lux) ? (rawLux - sample.lux) : (sample.lux - rawLux);
    reportCheck("adcCodesToLux matches sample", adcDiff <= tol, "");
    reportCheck("rawToLux matches sample", rawDiff <= tol, "");

    OPT4001::Threshold encodedThreshold;
    OPT4001::Status helperSt = device.luxToThreshold(sample.lux, encodedThreshold);
    reportCheck("luxToThreshold", helperSt.ok(), helperSt.ok() ? "" : errToStr(helperSt.code));
    if (helperSt.ok()) {
      reportCheck("threshold helper packs 12-bit result", encodedThreshold.result <= 0x0FFFu, "");
      reportCheck("threshold roundtrip sane", device.thresholdToLux(encodedThreshold) >= 0.0f, "");
    }
  }

  OPT4001::Sample cached;
  st = device.getLastSample(cached);
  reportCheck("getLastSample", st.ok(), st.ok() ? "" : errToStr(st.code));
  if (st.ok()) {
    reportCheck("sample timestamp set", device.sampleTimestampMs() > 0U, "");
    reportCheck("sample age sane", device.sampleAgeMs(millis()) < 60000U, "");
  }

  OPT4001::Sample slot0;
  st = device.readSampleSlot(0, slot0);
  reportCheck("readSampleSlot(0)", sampleStatusHasData(st), sampleStatusHasData(st) ? "" : errToStr(st.code));

  OPT4001::BurstFrame frame;
  st = device.readBurst(frame);
  reportCheck("readBurst", sampleStatusHasData(st), sampleStatusHasData(st) ? "" : errToStr(st.code));
  if (sampleStatusHasData(st)) {
    reportCheck("burst counter delta sane",
                device.sampleCounterDelta(frame.fifo0.counter, frame.newest.counter) <
                    OPT4001::cmd::SAMPLE_COUNT_MODULO,
                "");
  }

  uint16_t flagsRaw = 0;
  st = device.readFlagsRaw(flagsRaw);
  reportCheck("readFlagsRaw", st.ok(), st.ok() ? "" : errToStr(st.code));

  st = device.clearConversionReadyFlag();
  reportCheck("clearConversionReadyFlag", st.ok(), st.ok() ? "" : errToStr(st.code));

  st = device.clearFlags();
  reportCheck("clearFlags", st.ok(), st.ok() ? "" : errToStr(st.code));

  st = device.recover();
  reportCheck("recover", st.ok(), st.ok() ? "" : errToStr(st.code));
  reportCheck("isOnline", device.isOnline(), "");

  Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                goodIfNonZeroColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                goodIfZeroColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                skipCountColor(stats.skip), static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
}

void printHelp() {
  auto section = [](const char* title) {
    Serial.printf("\n%s[%s]%s\n", LOG_COLOR_GREEN, title, LOG_COLOR_RESET);
  };
  auto item = [](const char* cmd, const char* desc) {
    Serial.printf("  %s%-32s%s - %s\n", LOG_COLOR_CYAN, cmd, LOG_COLOR_RESET, desc);
  };

  Serial.println();
  Serial.printf("%s=== OPT4001 CLI Help ===%s\n", LOG_COLOR_CYAN, LOG_COLOR_RESET);

  section("Common");
  item("help / ?", "Show this help");
  item("version / ver", "Print firmware and library version info");
  item("scan", "Scan I2C bus");
  item("init", "Initialize/reinitialize device");
  item("end", "Shut down driver (returns to UNINIT)");
  item("addr [0x44|0x45|0x46]", "Show or set target I2C address");
  item("pkg [pico|sot]", "Show or set package variant");

  section("Data");
  item("read", "Blocking one-shot read");
  item("read force", "Blocking forced-auto read");
  item("read N", "Run N blocking reads");
  item("readblocking [force]", "Explicit blocking alias");
  item("start [force]", "Start one-shot conversion");
  item("poll / drdy", "Check conversion ready");
  item("readburst [force]", "Read RESULT plus FIFO history");
  item("slot <0..3>", "Read one history slot (0=newest)");
  item("sample / sampleage", "Cached sample and age");
  item("lux / mlux / ulux", "Read scaled lux helpers");
  item("adc2lux <codes>", "Convert linearized ADC codes to lux");
  item("raw2lux <exp> <mant>", "Convert raw exponent(0..8)/mantissa fields to lux");
  item("scale / timing", "Show package scaling and timing helpers");

  section("Configuration");
  item("cfg / settings", "Show live config and cached settings");
  item("snapshot", "Show cached settings only");
  item("id / identify", "Read and decode device ID");
  item("config", "Read and decode CONFIGURATION");
  item("config write <hex>", "Write full CONFIGURATION");
  item("intcfg", "Read and decode INT_CONFIGURATION");
  item("intcfg write <hex>", "Write full INT_CONFIGURATION");
  item("mode [power|cont]", "Set or show stable mode");
  item("range [0..8|auto]", "Set or show range");
  item("ctime [0..11]", "Set or show conversion time");
  item("qwake [0|1]", "Set or show quick wake");
  item("crc [0|1]", "Set or show host-side CRC verification");
  item("burst [0|1]", "Set or show I2C burst mode");
  item("threshold [low high]", "Read or set thresholds in lux");
  item("threshold raw <low> <high>", "Set thresholds from raw 16-bit register values");
  item("thcalc <lux>", "Calculate threshold register fields for lux");
  item("thdecode <raw16>", "Decode packed threshold register value");
  item("int latch|pol|faults|dir|cfg ...", "Interrupt configuration");

  section("Registers");
  item("status / flags", "Read and decode FLAGS (clear-on-read)");
  item("status_raw / flags raw", "Read raw FLAGS register");
  item("flags readyclear", "Clear ready flag only by write");
  item("flags clear", "Clear sticky flags via read path");
  item("dump", "Dump key registers");
  item("reg <addr>", "Read 16-bit register");
  item("regs <start> <len>", "Read raw register bytes");
  item("wreg <addr> <val>", "Write 16-bit register");

  section("Diagnostics");
  item("drv / health", "Show driver state and health");
  item("state", "Show compact one-line health summary");
  item("diag", "Print consolidated diagnostic report");
  item("online", "Show online/offline state");
  item("probe", "Probe device (no health tracking)");
  item("recover", "Manual recovery attempt");
  item("reset", "General-call reset (bus-wide)");
  item("resetreapply", "General-call reset + re-apply");
  item("healthmon [0|1] [interval]", "Toggle periodic health monitor output");
  item("verbose [0|1]", "Enable/disable verbose output");
  item("stress [N]", "Run blocking read stress");
  item("stress_mix [N]", "Run mixed-operation stress");
  item("selftest", "Run safe command self-test report");
}

void processCommand(const String& cmdLine) {
  String cmd = cmdLine;
  cmd.trim();
  if (cmd.length() == 0) {
    return;
  }

  if (cmd == "help" || cmd == "?") { printHelp(); return; }
  if (cmd == "version" || cmd == "ver") { printVersionInfo(); return; }
  if (cmd == "scan") { bus_diag::scan(); return; }
  if (cmd == "init") {
    LOGI("Initializing OPT4001...");
    device.end();
    OPT4001::Status st = device.begin(makeDefaultConfig());
    printStatus(st);
    if (st.ok()) {
      printDriverHealth();
    }
    return;
  }
  if (cmd == "end") {
    LOGI("Shutting down driver...");
    device.end();
    LOGI("Driver state: UNINIT");
    return;
  }
  if (cmd == "addr") { printAddressInfo(); return; }
  if (cmd.startsWith("addr ")) {
    uint32_t addr = 0;
    if (!parseU32(cmd.substring(5), addr) || addr > 0x7Fu) {
      LOGW("Usage: addr [0x44|0x45|0x46]");
      return;
    }
    OPT4001::Config cfg = device.isInitialized() ? device.getConfig() : makeDefaultConfig();
    cfg.i2cAddress = static_cast<uint8_t>(addr);
    (void)rebeginWithConfig(cfg);
    return;
  }
  if (cmd == "pkg") { printAddressInfo(); return; }
  if (cmd.startsWith("pkg ")) {
    String token = cmd.substring(4);
    token.trim();
    OPT4001::PackageVariant variant =
        (token == "pico" || token == "picostar") ? OPT4001::PackageVariant::PICOSTAR :
        (token == "sot" || token == "sot_5x3") ? OPT4001::PackageVariant::SOT_5X3 :
        static_cast<OPT4001::PackageVariant>(0xFF);
    if (variant != OPT4001::PackageVariant::PICOSTAR &&
        variant != OPT4001::PackageVariant::SOT_5X3) {
      LOGW("Usage: pkg [pico|sot]");
      return;
    }
    OPT4001::Config cfg = device.isInitialized() ? device.getConfig() : makeDefaultConfig();
    cfg.packageVariant = variant;
    (void)rebeginWithConfig(cfg);
    return;
  }
  if (cmd == "drv" || cmd == "health") { printDriverHealth(); return; }
  if (cmd == "state") { printHealthView(device); return; }
  if (cmd == "diag") { printDiagnosticReport(); return; }
  if (cmd == "online") {
    const bool online = device.isOnline();
    Serial.printf("  Online: %s%s%s\n",
                  online ? LOG_COLOR_GREEN : LOG_COLOR_RED,
                  log_bool_str(online),
                  LOG_COLOR_RESET);
    return;
  }
  if (cmd == "probe") {
    LOGI("Probing device (no health tracking)...");
    HealthSnapshot<OPT4001::OPT4001> before;
    before.capture(device);
    OPT4001::Status st = device.probe();
    printStatus(st);
    HealthSnapshot<OPT4001::OPT4001> after;
    after.capture(device);
    Serial.println("  Health changes:");
    printHealthDiff(before, after);
    return;
  }
  if (cmd == "recover") {
    LOGI("Attempting recovery...");
    HealthSnapshot<OPT4001::OPT4001> before;
    before.capture(device);
    OPT4001::Status st = device.recover();
    printStatus(st);
    HealthSnapshot<OPT4001::OPT4001> after;
    after.capture(device);
    Serial.println("  Health changes:");
    printHealthDiff(before, after);
    printDriverHealth();
    return;
  }
  if (cmd == "reset") {
    LOGW("Issuing general-call reset (bus-wide).");
    HealthSnapshot<OPT4001::OPT4001> before;
    before.capture(device);
    OPT4001::Status st = device.softReset();
    printStatus(st);
    HealthSnapshot<OPT4001::OPT4001> after;
    after.capture(device);
    Serial.println("  Health changes:");
    printHealthDiff(before, after);
    printHealthView(device);
    return;
  }
  if (cmd == "resetreapply") {
    LOGW("Issuing general-call reset + re-apply.");
    HealthSnapshot<OPT4001::OPT4001> before;
    before.capture(device);
    OPT4001::Status st = device.resetAndReapply();
    printStatus(st);
    HealthSnapshot<OPT4001::OPT4001> after;
    after.capture(device);
    Serial.println("  Health changes:");
    printHealthDiff(before, after);
    if (st.ok()) {
      printDriverHealth();
    }
    return;
  }
  if (cmd == "cfg" || cmd == "settings") { printLiveConfig(); printSnapshot(); return; }
  if (cmd == "snapshot") { printSnapshot(); return; }
  if (cmd == "id" || cmd == "identify") { printDeviceIdInfo(); return; }
  if (cmd == "config") { printConfigurationInfo(); return; }
  if (cmd.startsWith("config write ")) {
    uint32_t value = 0;
    if (!parseU32(cmd.substring(13), value) || value > 0xFFFFu) {
      LOGW("Usage: config write <0..0xFFFF>");
      return;
    }
    OPT4001::Status st = device.writeConfiguration(static_cast<uint16_t>(value));
    printStatus(st);
    if (st.ok()) {
      printConfigurationInfo();
    }
    return;
  }
  if (cmd == "intcfg") { printIntConfigurationInfo(); return; }
  if (cmd.startsWith("intcfg write ")) {
    uint32_t value = 0;
    if (!parseU32(cmd.substring(13), value) || value > 0xFFFFu) {
      LOGW("Usage: intcfg write <0..0xFFFF>");
      return;
    }
    OPT4001::Status st = device.writeIntConfiguration(static_cast<uint16_t>(value));
    printStatus(st);
    if (st.ok()) {
      printIntConfigurationInfo();
    }
    return;
  }
  if (cmd == "status" || cmd == "flags") { printFlagsDecoded(); return; }
  if (cmd == "status_raw" || cmd == "flags raw") { printFlagsRawOnly(); return; }
  if (cmd == "flags readyclear") { printStatus(device.clearConversionReadyFlag()); return; }
  if (cmd == "flags clear") { printStatus(device.clearFlags()); return; }
  if (cmd == "dump") { printRegisterDump(); return; }
  if (cmd == "read") { (void)blockingReadAndPrint(OPT4001::Mode::ONE_SHOT); return; }
  if (cmd == "read force") { (void)blockingReadAndPrint(OPT4001::Mode::ONE_SHOT_FORCED_AUTO); return; }
  if (cmd == "readblocking") { (void)blockingReadAndPrint(OPT4001::Mode::ONE_SHOT); return; }
  if (cmd == "readblocking force") { (void)blockingReadAndPrint(OPT4001::Mode::ONE_SHOT_FORCED_AUTO); return; }
  if (cmd.startsWith("read ")) {
    int32_t count = 0;
    if (!parseI32(cmd.substring(5), count) || count <= 0 || count > 10000) {
      LOGW("Invalid count (1-10000)");
      return;
    }
    for (int32_t i = 0; i < count; ++i) {
      if (!blockingReadAndPrint(OPT4001::Mode::ONE_SHOT)) {
        break;
      }
    }
    return;
  }
  if (cmd == "start") { printStatus(device.startConversion()); return; }
  if (cmd == "start force") { printStatus(device.startConversion(OPT4001::Mode::ONE_SHOT_FORCED_AUTO)); return; }
  if (cmd == "poll" || cmd == "drdy") {
    const bool ready = device.conversionReady();
    LOGI("Conversion ready: %s%s%s", yesNoColor(ready), ready ? "YES" : "NO", LOG_COLOR_RESET);
    return;
  }
  if (cmd == "readburst" || cmd == "readburst force") {
    const OPT4001::Mode mode =
        cmd.endsWith("force") ? OPT4001::Mode::ONE_SHOT_FORCED_AUTO : OPT4001::Mode::ONE_SHOT;
    if (!primePowerDownSample(mode)) {
      return;
    }
    OPT4001::BurstFrame frame;
    OPT4001::Status st = device.readBurst(frame);
    if (!sampleStatusHasData(st)) {
      printStatus(st);
      return;
    }
    printBurstFrame(frame);
    if (sampleStatusWarn(st)) {
      printStatus(st);
    }
    return;
  }
  if (cmd.startsWith("slot ")) {
    uint32_t slot = 0;
    if (!parseU32(cmd.substring(5), slot) || slot > 3U) {
      LOGW("Usage: slot <0..3>");
      return;
    }
    if (!primePowerDownSample(OPT4001::Mode::ONE_SHOT)) {
      return;
    }
    OPT4001::Sample sample;
    OPT4001::Status st = device.readSampleSlot(static_cast<uint8_t>(slot), sample);
    if (!sampleStatusHasData(st)) {
      printStatus(st);
      return;
    }
    printSample(sample);
    if (sampleStatusWarn(st)) {
      printStatus(st);
    }
    return;
  }
  if (cmd == "sample") {
    OPT4001::Sample sample;
    OPT4001::Status st = device.getLastSample(sample);
    if (!st.ok()) {
      printStatus(st);
    } else {
      printSample(sample);
    }
    return;
  }
  if (cmd == "sampleage") { printSampleAge(); return; }
  if (cmd == "lux") {
    float lux = 0.0f;
    OPT4001::Status st = device.readBlockingLux(lux, BLOCKING_READ_TIMEOUT_MS);
    if (!sampleStatusHasData(st)) {
      printStatus(st);
      return;
    }
    Serial.printf("  Lux: %.6f lx\n", lux);
    if (sampleStatusWarn(st)) {
      printStatus(st);
    }
    return;
  }
  if (cmd == "mlux") {
    if (!primePowerDownSample(OPT4001::Mode::ONE_SHOT)) {
      return;
    }
    uint32_t milliLux = 0;
    OPT4001::Status st = device.readMilliLux(milliLux);
    if (!sampleStatusHasData(st)) {
      printStatus(st);
      return;
    }
    Serial.printf("  Milli-lux: %lu mlux\n", static_cast<unsigned long>(milliLux));
    if (sampleStatusWarn(st)) {
      printStatus(st);
    }
    return;
  }
  if (cmd == "ulux") {
    if (!primePowerDownSample(OPT4001::Mode::ONE_SHOT)) {
      return;
    }
    uint64_t microLux = 0;
    OPT4001::Status st = device.readMicroLux(microLux);
    if (!sampleStatusHasData(st)) {
      printStatus(st);
      return;
    }
    Serial.printf("  Micro-lux: %llu ulux\n", static_cast<unsigned long long>(microLux));
    if (sampleStatusWarn(st)) {
      printStatus(st);
    }
    return;
  }
  if (cmd.startsWith("adc2lux ")) {
    uint32_t adcCodes = 0;
    if (!parseU32(cmd.substring(8), adcCodes)) {
      LOGW("Usage: adc2lux <adcCodes>");
      return;
    }
    printAdcToLux(adcCodes);
    return;
  }
  if (cmd.startsWith("raw2lux ")) {
    String args = cmd.substring(8);
    args.trim();
    const int split = args.indexOf(' ');
    uint32_t exponent = 0;
    uint32_t mantissa = 0;
    if (split < 0 ||
        !parseU32(args.substring(0, split), exponent) ||
        !parseU32(args.substring(split + 1), mantissa) ||
        exponent > 8U ||
        mantissa > 0xFFFFFu) {
      LOGW("Usage: raw2lux <exp0..8> <mant0..0xFFFFF>");
      return;
    }
    printRawFieldsToLux(static_cast<uint8_t>(exponent), mantissa);
    return;
  }
  if (cmd == "scale" || cmd == "timing") { printScale(); return; }
  if (cmd == "mode") { Serial.printf("  Mode: %s\n", modeToStr(device.getMode())); return; }
  if (cmd.startsWith("mode ")) {
    String token = cmd.substring(5);
    token.trim();
    OPT4001::Status st =
        (token == "cont" || token == "continuous")
            ? device.setMode(OPT4001::Mode::CONTINUOUS)
            : (token == "power" || token == "pd")
                  ? device.setMode(OPT4001::Mode::POWER_DOWN)
                  : OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "Usage: mode [power|cont]");
    printStatus(st);
    return;
  }
  if (cmd == "range") {
    Serial.printf("  Range: %s full-scale=%.3f lx resolution=%.9f lx\n",
                  rangeToStr(device.getRange()),
                  device.getCurrentFullScaleLux(),
                  device.getCurrentResolutionLux());
    return;
  }
  if (cmd.startsWith("range ")) {
    String token = cmd.substring(6);
    token.trim();
    OPT4001::Range range = OPT4001::Range::AUTO;
    if (token != "auto") {
      int32_t value = 0;
      if (!parseI32(token, value) || value < 0 || value > 8) {
        LOGW("Usage: range [0..8|auto]");
        return;
      }
      range = static_cast<OPT4001::Range>(value);
    }
    printStatus(device.setRange(range));
    return;
  }
  if (cmd == "ctime") {
    Serial.printf("  Conversion time: %s (%lu us, bits=%u)\n",
                  conversionTimeToStr(device.getConversionTime()),
                  static_cast<unsigned long>(device.getConversionTimeUs()),
                  static_cast<unsigned>(device.getEffectiveBits()));
    return;
  }
  if (cmd.startsWith("ctime ")) {
    int32_t value = 0;
    if (!parseI32(cmd.substring(6), value) || value < 0 || value > 11) {
      LOGW("Usage: ctime [0..11]");
      return;
    }
    printStatus(device.setConversionTime(static_cast<OPT4001::ConversionTime>(value)));
    return;
  }
  if (cmd == "qwake") {
    Serial.printf("  Quick wake: %s%s%s\n",
                  onOffColor(device.getQuickWake()),
                  log_bool_str(device.getQuickWake()),
                  LOG_COLOR_RESET);
    return;
  }
  if (cmd.startsWith("qwake ")) {
    bool value = false;
    if (!parseBool01(cmd.substring(6), value)) {
      LOGW("Usage: qwake [0|1]");
      return;
    }
    printStatus(device.setQuickWake(value));
    return;
  }
  if (cmd == "crc") {
    Serial.printf("  Verify CRC: %s%s%s\n",
                  onOffColor(device.getVerifyCrc()),
                  log_bool_str(device.getVerifyCrc()),
                  LOG_COLOR_RESET);
    return;
  }
  if (cmd.startsWith("crc ")) {
    bool value = false;
    if (!parseBool01(cmd.substring(4), value)) {
      LOGW("Usage: crc [0|1]");
      return;
    }
    printStatus(device.setVerifyCrc(value));
    return;
  }
  if (cmd == "burst") {
    Serial.printf("  Burst mode: %s%s%s\n",
                  onOffColor(device.getBurstMode()),
                  log_bool_str(device.getBurstMode()),
                  LOG_COLOR_RESET);
    return;
  }
  if (cmd.startsWith("burst ")) {
    bool value = false;
    if (!parseBool01(cmd.substring(6), value)) {
      LOGW("Usage: burst [0|1]");
      return;
    }
    printStatus(device.setBurstMode(value));
    return;
  }
  if (cmd == "threshold" || cmd == "threshold lux") { printThresholdLux(); return; }
  if (cmd.startsWith("threshold raw ")) {
    String args = cmd.substring(14);
    args.trim();
    const int split = args.indexOf(' ');
    uint32_t lowRaw = 0;
    uint32_t highRaw = 0;
    if (split < 0 ||
        !parseU32(args.substring(0, split), lowRaw) ||
        !parseU32(args.substring(split + 1), highRaw) ||
        lowRaw > 0xFFFFu || highRaw > 0xFFFFu) {
      LOGW("Usage: threshold raw <low16> <high16>");
      return;
    }
    const OPT4001::Threshold low = unpackThreshold(static_cast<uint16_t>(lowRaw));
    const OPT4001::Threshold high = unpackThreshold(static_cast<uint16_t>(highRaw));
    printStatus(device.setThresholds(low, high));
    return;
  }
  if (cmd.startsWith("threshold ")) {
    String args = cmd.substring(10);
    args.trim();
    const int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: threshold <lowLux> <highLux>");
      return;
    }
    float low = 0.0f;
    float high = 0.0f;
    if (!parseF32(args.substring(0, split), low) ||
        !parseF32(args.substring(split + 1), high)) {
      LOGW("Usage: threshold <lowLux> <highLux>");
      return;
    }
    printStatus(device.setThresholdsLux(low, high));
    return;
  }
  if (cmd.startsWith("thcalc ")) {
    float lux = 0.0f;
    if (!parseF32(cmd.substring(7), lux)) {
      LOGW("Usage: thcalc <lux>");
      return;
    }
    printThresholdEncodingFromLux(lux);
    return;
  }
  if (cmd.startsWith("thdecode ")) {
    uint32_t raw = 0;
    if (!parseU32(cmd.substring(9), raw) || raw > 0xFFFFu) {
      LOGW("Usage: thdecode <0..0xFFFF>");
      return;
    }
    printThresholdDecodingFromRaw(static_cast<uint16_t>(raw));
    return;
  }
  if (cmd.startsWith("int latch ")) {
    bool value = false;
    if (!parseBool01(cmd.substring(10), value)) {
      LOGW("Usage: int latch [0|1]");
      return;
    }
    printStatus(device.setInterruptLatch(value ? OPT4001::InterruptLatch::LATCHED
                                               : OPT4001::InterruptLatch::TRANSPARENT));
    return;
  }
  if (cmd.startsWith("int pol ")) {
    String token = cmd.substring(8);
    token.trim();
    OPT4001::Status st =
        (token == "low")
            ? device.setInterruptPolarity(OPT4001::InterruptPolarity::ACTIVE_LOW)
            : (token == "high")
                  ? device.setInterruptPolarity(OPT4001::InterruptPolarity::ACTIVE_HIGH)
                  : OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "Usage: int pol [low|high]");
    printStatus(st);
    return;
  }
  if (cmd.startsWith("int faults ")) {
    String token = cmd.substring(11);
    token.trim();
    OPT4001::FaultCount faults =
        (token == "1") ? OPT4001::FaultCount::FAULTS_1 :
        (token == "2") ? OPT4001::FaultCount::FAULTS_2 :
        (token == "4") ? OPT4001::FaultCount::FAULTS_4 :
        (token == "8") ? OPT4001::FaultCount::FAULTS_8 :
        static_cast<OPT4001::FaultCount>(0xFF);
    if (faults != OPT4001::FaultCount::FAULTS_1 &&
        faults != OPT4001::FaultCount::FAULTS_2 &&
        faults != OPT4001::FaultCount::FAULTS_4 &&
        faults != OPT4001::FaultCount::FAULTS_8) {
      LOGW("Usage: int faults [1|2|4|8]");
      return;
    }
    printStatus(device.setFaultCount(faults));
    return;
  }
  if (cmd.startsWith("int dir ")) {
    String token = cmd.substring(8);
    token.trim();
    OPT4001::IntDirection direction =
        (token == "in" || token == "input") ? OPT4001::IntDirection::PIN_INPUT :
        (token == "out" || token == "output") ? OPT4001::IntDirection::PIN_OUTPUT :
        static_cast<OPT4001::IntDirection>(0xFF);
    if (direction != OPT4001::IntDirection::PIN_INPUT &&
        direction != OPT4001::IntDirection::PIN_OUTPUT) {
      LOGW("Usage: int dir [in|out]");
      return;
    }
    printStatus(device.setIntDirection(direction));
    return;
  }
  if (cmd.startsWith("int cfg ")) {
    String token = cmd.substring(8);
    token.trim();
    OPT4001::IntConfig config =
        (token == "threshold" || token == "thresh") ? OPT4001::IntConfig::THRESHOLD :
        (token == "conv" || token == "every") ? OPT4001::IntConfig::EVERY_CONVERSION :
        (token == "fifo" || token == "full") ? OPT4001::IntConfig::FIFO_FULL :
        static_cast<OPT4001::IntConfig>(0xFF);
    if (config != OPT4001::IntConfig::THRESHOLD &&
        config != OPT4001::IntConfig::EVERY_CONVERSION &&
        config != OPT4001::IntConfig::FIFO_FULL) {
      LOGW("Usage: int cfg [threshold|conv|fifo]");
      return;
    }
    printStatus(device.setIntConfig(config));
    return;
  }
  if (cmd == "int") { printIntConfigurationInfo(); printThresholdLux(); return; }
  if (cmd.startsWith("reg ")) {
    uint32_t addr = 0;
    if (!parseU32(cmd.substring(4), addr) || addr > 0xFFu) {
      LOGW("Usage: reg <addr>");
      return;
    }
    uint16_t value = 0;
    OPT4001::Status st = device.readRegister16(static_cast<uint8_t>(addr), value);
    if (!st.ok()) {
      printStatus(st);
      return;
    }
    Serial.printf("  Reg 0x%02lX = 0x%04X (%u)\n",
                  static_cast<unsigned long>(addr),
                  value,
                  value);
    return;
  }
  if (cmd.startsWith("regs ")) {
    String args = cmd.substring(5);
    args.trim();
    const int split = args.indexOf(' ');
    uint32_t start = 0;
    uint32_t len = 0;
    if (split < 0 ||
        !parseU32(args.substring(0, split), start) ||
        !parseU32(args.substring(split + 1), len) ||
        start > 0xFFu || len == 0U || len > 64U) {
      LOGW("Usage: regs <start> <lenBytes>");
      return;
    }
    uint8_t buf[64] = {};
    OPT4001::Status st = device.readRegisters(static_cast<uint8_t>(start), buf, static_cast<size_t>(len));
    if (!st.ok()) {
      printStatus(st);
      return;
    }
    for (uint32_t i = 0; i < len; ++i) {
      Serial.printf("  [0x%02lX] = 0x%02X\n",
                    static_cast<unsigned long>(start + i),
                    buf[i]);
    }
    return;
  }
  if (cmd.startsWith("wreg ")) {
    String args = cmd.substring(5);
    args.trim();
    const int split = args.indexOf(' ');
    uint32_t addr = 0;
    uint32_t value = 0;
    if (split < 0 ||
        !parseU32(args.substring(0, split), addr) ||
        !parseU32(args.substring(split + 1), value) ||
        addr > 0xFFu || value > 0xFFFFu) {
      LOGW("Usage: wreg <addr> <val>");
      return;
    }
    printStatus(device.writeRegister16(static_cast<uint8_t>(addr), static_cast<uint16_t>(value)));
    return;
  }
  if (cmd == "healthmon") {
    printHealthMonitorState();
    return;
  }
  if (cmd.startsWith("healthmon ")) {
    String args = cmd.substring(10);
    args.trim();
    const int split = args.indexOf(' ');
    bool enable = false;
    uint32_t intervalMs = healthMonitorIntervalMs;
    if (split < 0) {
      if (!parseBool01(args, enable)) {
        LOGW("Usage: healthmon [0|1] [intervalMs]");
        return;
      }
    } else {
      if (!parseBool01(args.substring(0, split), enable) ||
          !parseU32(args.substring(split + 1), intervalMs)) {
        LOGW("Usage: healthmon [0|1] [intervalMs]");
        return;
      }
    }
    healthMonitorEnabled = enable;
    healthMonitorIntervalMs = intervalMs;
    if (healthMonitorEnabled) {
      healthMonitor.begin(healthMonitorIntervalMs);
      healthMonitor.tick(device, true);
    }
    printHealthMonitorState();
    return;
  }
  if (cmd == "verbose") {
    LOGI("Verbose mode: %s%s%s", onOffColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);
    return;
  }
  if (cmd.startsWith("verbose ")) {
    bool value = false;
    if (!parseBool01(cmd.substring(8), value)) {
      LOGW("Usage: verbose [0|1]");
      return;
    }
    verboseMode = value;
    LOGI("Verbose mode: %s%s%s", onOffColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);
    return;
  }
  if (cmd == "selftest") { runSelfTest(); return; }
  if (cmd == "stress_mix") { runStressMix(50); return; }
  if (cmd.startsWith("stress_mix ")) {
    int32_t count = 0;
    if (!parseI32(cmd.substring(11), count) || count <= 0 || count > 100000) {
      LOGW("Invalid count (1-100000)");
      return;
    }
    runStressMix(count);
    return;
  }
  if (cmd == "stress") { runStress(10); return; }
  if (cmd.startsWith("stress ")) {
    int32_t count = 0;
    if (!parseI32(cmd.substring(7), count) || count <= 0 || count > 100000) {
      LOGW("Invalid count (1-100000)");
      return;
    }
    runStress(count);
    return;
  }
  LOGW("Unknown command: %s", cmd.c_str());
}

void setup() {
  board::initSerial();
  delay(100);

  LOGI("=== OPT4001 Bring-up Example ===");

  if (!board::initI2c()) {
    LOGE("Failed to initialize I2C");
    return;
  }
  LOGI("I2C initialized (SDA=%d, SCL=%d)", board::I2C_SDA, board::I2C_SCL);

  board::initIntPin();

  bus_diag::scan();

  OPT4001::Status st = device.begin(makeDefaultConfig());
  if (!st.ok()) {
    LOGE("Failed to initialize device");
    printStatus(st);
    LOGI("Type 'init' to retry initialization");
  } else {
    LOGI("Device initialized successfully");
    printDriverHealth();
  }

  Serial.println("\nType 'help' for commands");
  Serial.print("> ");
}

void loop() {
  device.tick(millis());
  if (healthMonitorEnabled) {
    healthMonitor.tick(device);
  }
  static String inputBuffer;
  static constexpr size_t kMaxInputLen = 128;
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
        Serial.print("> ");
      }
    } else if (inputBuffer.length() < kMaxInputLen) {
      inputBuffer += c;
    }
  }
}
