/// @file test_basic.cpp
/// @brief Native contract tests for OPT4001 lifecycle and measurement behavior.

#include <unity.h>

#include <cstring>

#include "Arduino.h"
#include "Wire.h"

SerialClass Serial;
TwoWire Wire;

#define private public
#include "OPT4001/OPT4001.h"
#undef private

using namespace OPT4001;

namespace {

struct FakeBus {
  Status writeStatus = Status::Ok();
  Status readStatus = Status::Ok();
  uint16_t registers[0x20]{};
  uint32_t nowMs = 1234;
  uint32_t writeCalls = 0;
  uint32_t readCalls = 0;
  uint32_t oneShotReadyAtMs = UINT32_MAX;
  uint32_t continuousReadyAtMs = UINT32_MAX;

  FakeBus() {
    resetToDefaults();
  }

  void resetToDefaults() {
    memset(registers, 0, sizeof(registers));
    registers[cmd::REG_THRESHOLD_L] = cmd::THRESHOLD_L_RESET;
    registers[cmd::REG_THRESHOLD_H] = cmd::THRESHOLD_H_RESET;
    registers[cmd::REG_CONFIGURATION] = cmd::CONFIGURATION_RESET;
    registers[cmd::REG_INT_CONFIGURATION] = cmd::INT_CONFIGURATION_RESET;
    registers[cmd::REG_FLAGS] = cmd::FLAGS_RESET;
    registers[cmd::REG_DEVICE_ID] = cmd::DEVICE_ID_RESET;
    oneShotReadyAtMs = UINT32_MAX;
    continuousReadyAtMs = UINT32_MAX;
  }
};

uint32_t conversionTimeMsFromRegister(uint16_t configReg) {
  const uint8_t idx =
      static_cast<uint8_t>((configReg & cmd::MASK_CONVERSION_TIME) >> cmd::BIT_CONVERSION_TIME);
  return cmd::CONVERSION_TIME_MS_CEIL[idx];
}

uint32_t oneShotBudgetMsFromRegister(uint16_t configReg, Mode mode) {
  uint32_t budgetMs = conversionTimeMsFromRegister(configReg);
  if ((configReg & cmd::MASK_QWAKE) == 0) {
    budgetMs += 1U;
  }
  if (mode == Mode::ONE_SHOT_FORCED_AUTO) {
    budgetMs += 1U;
  }
  return budgetMs;
}

void maybeCompleteConversion(FakeBus& bus) {
  if (bus.oneShotReadyAtMs != UINT32_MAX &&
      static_cast<int32_t>(bus.nowMs - bus.oneShotReadyAtMs) >= 0) {
    bus.registers[cmd::REG_CONFIGURATION] =
        static_cast<uint16_t>(bus.registers[cmd::REG_CONFIGURATION] & ~cmd::MASK_MODE);
    bus.registers[cmd::REG_FLAGS] |= cmd::MASK_CONVERSION_READY_FLAG;
    bus.oneShotReadyAtMs = UINT32_MAX;
  }

  if (bus.continuousReadyAtMs != UINT32_MAX &&
      static_cast<int32_t>(bus.nowMs - bus.continuousReadyAtMs) >= 0) {
    bus.registers[cmd::REG_FLAGS] |= cmd::MASK_CONVERSION_READY_FLAG;
    bus.continuousReadyAtMs = UINT32_MAX;
  }
}

Status fakeWrite(uint8_t, const uint8_t* data, size_t len, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->writeCalls++;
  if (!bus->writeStatus.ok()) {
    return bus->writeStatus;
  }
  if (data == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake write");
  }

  if (len == 1 && data[0] == cmd::GENERAL_CALL_RESET) {
    bus->resetToDefaults();
    return Status::Ok();
  }

  if (len >= 3) {
    const uint8_t reg = data[0];
    const uint16_t value = static_cast<uint16_t>((data[1] << 8) | data[2]);
    if (reg != cmd::REG_FLAGS) {
      bus->registers[reg] = value;
    }

    if (reg == cmd::REG_CONFIGURATION) {
      const Mode mode =
          static_cast<Mode>((value & cmd::MASK_MODE) >> cmd::BIT_MODE);
      bus->registers[cmd::REG_FLAGS] &= static_cast<uint16_t>(~cmd::MASK_CONVERSION_READY_FLAG);
      bus->oneShotReadyAtMs = UINT32_MAX;
      bus->continuousReadyAtMs = UINT32_MAX;
      if (mode == Mode::CONTINUOUS) {
        bus->continuousReadyAtMs = bus->nowMs + conversionTimeMsFromRegister(value);
      } else if (mode == Mode::ONE_SHOT || mode == Mode::ONE_SHOT_FORCED_AUTO) {
        bus->oneShotReadyAtMs = bus->nowMs + oneShotBudgetMsFromRegister(value, mode);
      }
    } else if (reg == cmd::REG_FLAGS && value != 0) {
      bus->registers[cmd::REG_FLAGS] &=
          static_cast<uint16_t>(~cmd::MASK_CONVERSION_READY_FLAG);
    } else if (reg == cmd::REG_FLAGS) {
      bus->registers[cmd::REG_FLAGS] = value;
    }
  }

  return Status::Ok();
}

Status fakeWriteRead(uint8_t, const uint8_t* txData, size_t txLen, uint8_t* rxData,
                     size_t rxLen, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->readCalls++;
  if (!bus->readStatus.ok()) {
    return bus->readStatus;
  }
  if (txData == nullptr || txLen == 0 || (rxLen > 0 && rxData == nullptr)) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake read");
  }

  maybeCompleteConversion(*bus);

  uint8_t reg = txData[0];
  for (size_t i = 0; i < rxLen; ++i) {
    const uint8_t currentReg = static_cast<uint8_t>(reg + (i / 2U));
    const uint16_t value = bus->registers[currentReg];
    rxData[i] = ((i & 1U) == 0U) ? static_cast<uint8_t>(value >> 8)
                                 : static_cast<uint8_t>(value & 0xFF);
  }

  if (reg == cmd::REG_FLAGS && rxLen >= 2) {
    bus->registers[cmd::REG_FLAGS] &=
        static_cast<uint16_t>(~(cmd::MASK_CONVERSION_READY_FLAG |
                                cmd::MASK_FLAG_H |
                                cmd::MASK_FLAG_L));
  }

  return Status::Ok();
}

uint32_t fakeNowMs(void* user) {
  return static_cast<FakeBus*>(user)->nowMs;
}

void fakeYield(void*) {}

bool fakeGpioRead(int, void*) {
  return true;
}

Config makeConfig(FakeBus& bus) {
  Config cfg;
  cfg.i2cWrite = fakeWrite;
  cfg.i2cWriteRead = fakeWriteRead;
  cfg.i2cUser = &bus;
  cfg.nowMs = fakeNowMs;
  cfg.timeUser = &bus;
  cfg.cooperativeYield = fakeYield;
  cfg.packageVariant = PackageVariant::SOT_5X3;
  cfg.i2cAddress = cmd::I2C_ADDR_DEFAULT;
  cfg.i2cTimeoutMs = 10;
  cfg.offlineThreshold = 3;
  return cfg;
}

uint8_t computeCrc(uint8_t exponent, uint32_t mantissa, uint8_t counter) {
  OPT4001::OPT4001 helper;
  return helper._computeCrcNibble(exponent, mantissa, counter);
}

void seedSample(FakeBus& bus, uint8_t msbReg, uint8_t exponent,
                uint32_t mantissa, uint8_t counter, bool goodCrc = true) {
  const uint16_t msb = static_cast<uint16_t>(((static_cast<uint16_t>(exponent) << 12) & 0xF000) |
                                             ((mantissa >> 8) & 0x0FFF));
  uint8_t crc = computeCrc(exponent, mantissa, counter);
  if (!goodCrc) {
    crc ^= 0x1U;
  }
  const uint16_t lsb =
      static_cast<uint16_t>(((mantissa & 0xFFU) << 8) |
                            ((counter & 0x0FU) << 4) |
                            (crc & 0x0FU));
  bus.registers[msbReg] = msb;
  bus.registers[static_cast<uint8_t>(msbReg + 1U)] = lsb;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_status_ok() {
  Status st = Status::Ok();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(static_cast<bool>(st));
  TEST_ASSERT_TRUE(st.is(Err::OK));
}

void test_status_error() {
  Status st = Status::Error(Err::I2C_ERROR, "Test", 42);
  TEST_ASSERT_FALSE(st.ok());
  TEST_ASSERT_TRUE(st.is(Err::I2C_ERROR));
  TEST_ASSERT_EQUAL_INT32(42, st.detail);
}

void test_status_in_progress() {
  Status st{Err::IN_PROGRESS, 0, "In progress"};
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_FALSE(st.ok());
}

void test_config_defaults() {
  Config cfg;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PackageVariant::SOT_5X3),
                          static_cast<uint8_t>(cfg.packageVariant));
  TEST_ASSERT_EQUAL_HEX8(0x45, cfg.i2cAddress);
  TEST_ASSERT_TRUE(cfg.verifyCrc);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::AUTO), static_cast<uint8_t>(cfg.range));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConversionTime::MS_100),
                          static_cast<uint8_t>(cfg.conversionTime));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::POWER_DOWN),
                          static_cast<uint8_t>(cfg.mode));
  TEST_ASSERT_TRUE(cfg.burstMode);
  TEST_ASSERT_EQUAL_UINT8(0x0B, cfg.highThreshold.exponent);
  TEST_ASSERT_EQUAL_UINT16(0x0FFF, cfg.highThreshold.result);
}

void test_get_last_sample_before_any_read() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Sample sample;
  Status st = dev.getLastSample(sample);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::MEASUREMENT_NOT_READY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, dev.sampleTimestampMs());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.sampleAgeMs(5000));
}

void test_begin_rejects_missing_callbacks() {
  OPT4001::OPT4001 dev;
  Config cfg;
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

void test_begin_rejects_one_shot_startup_mode() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::ONE_SHOT;
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG), static_cast<uint8_t>(st.code));
}

void test_begin_rejects_invalid_package_address_combo() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.packageVariant = PackageVariant::PICOSTAR;
  cfg.i2cAddress = cmd::I2C_ADDR_GND;
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  cfg = makeConfig(bus);
  cfg.packageVariant = PackageVariant::SOT_5X3;
  cfg.i2cAddress = 0x47;
  st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
}

void test_begin_success_sets_ready_and_counters() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Status st = dev.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(4u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.lastOkMs());
}

void test_probe_failure_does_not_update_health() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t beforeSuccess = dev.totalSuccess();
  const uint32_t beforeFailures = dev.totalFailures();

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -7);
  Status st = dev.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(beforeSuccess, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(beforeFailures, dev.totalFailures());
}

void test_recover_failure_updates_health() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -9);
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
}

void test_recover_success_returns_ready() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -9);
  (void)dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));

  bus.readStatus = Status::Ok();
  bus.nowMs = 4321;
  Status st = dev.recover();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(4321u, dev.lastOkMs());
}

void test_start_conversion_wraparound_reaches_ready() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  seedSample(bus, cmd::REG_RESULT, 1, 0x12345, 7);

  bus.nowMs = 0xFFFFFFF8u;
  Status st = dev.startConversion();
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_FALSE(dev.conversionReady());

  bus.nowMs = 120u;
  dev.tick(bus.nowMs);
  TEST_ASSERT_TRUE(dev.conversionReady());

  Sample sample;
  st = dev.readSample(sample);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(1u, sample.exponent);
  TEST_ASSERT_EQUAL_UINT32(0x12345u, sample.mantissa);
}

void test_read_sample_decodes_lux_and_crc() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 2, 0x34567, 9, true);
  bus.nowMs += 150;
  dev.tick(bus.nowMs);

  Sample sample;
  Status st = dev.readSample(sample);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(sample.crcValid);
  TEST_ASSERT_EQUAL_UINT8(2u, sample.exponent);
  TEST_ASSERT_EQUAL_UINT32(0x34567u, sample.mantissa);
  TEST_ASSERT_EQUAL_UINT32(0xD159Cu, sample.adcCodes);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, sample.adcCodes * dev.getLuxLsb(), sample.lux);
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.sampleTimestampMs());

  Sample cached;
  TEST_ASSERT_TRUE(dev.getLastSample(cached).ok());
  TEST_ASSERT_EQUAL_UINT32(sample.adcCodes, cached.adcCodes);

  bus.nowMs += 250;
  TEST_ASSERT_EQUAL_UINT32(250u, dev.sampleAgeMs(bus.nowMs));
}

void test_crc_mismatch_returns_error_when_enabled() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x23456, 3, false);
  bus.nowMs += 150;
  dev.tick(bus.nowMs);

  Sample sample;
  Status st = dev.readSample(sample);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR), static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(sample.crcValid);
}

void test_crc_mismatch_allowed_when_verification_disabled() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.verifyCrc = false;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x23456, 3, false);
  bus.nowMs += 150;
  dev.tick(bus.nowMs);

  Sample sample;
  Status st = dev.readSample(sample);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(sample.crcValid);
}

void test_read_burst_decodes_fifo() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 1, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 2, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 3, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 4, true);
  bus.nowMs += 150;
  dev.tick(bus.nowMs);

  BurstFrame frame;
  Status st = dev.readBurst(frame);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0x11111u, frame.newest.mantissa);
  TEST_ASSERT_EQUAL_UINT32(0x22222u, frame.fifo0.mantissa);
  TEST_ASSERT_EQUAL_UINT32(0x33333u, frame.fifo1.mantissa);
  TEST_ASSERT_EQUAL_UINT32(0x44444u, frame.fifo2.mantissa);
}

void test_read_burst_nonburst_path_decodes_fifo() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.burstMode = false;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 1, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 2, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 3, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 4, true);
  bus.nowMs += 150;
  dev.tick(bus.nowMs);

  BurstFrame frame;
  Status st = dev.readBurst(frame);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0x11111u, frame.newest.mantissa);
  TEST_ASSERT_EQUAL_UINT32(0x22222u, frame.fifo0.mantissa);
  TEST_ASSERT_EQUAL_UINT32(0x33333u, frame.fifo1.mantissa);
  TEST_ASSERT_EQUAL_UINT32(0x44444u, frame.fifo2.mantissa);
}

void test_set_thresholds_lux_updates_threshold_registers() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.setThresholdsLux(10.0f, 1000.0f);
  TEST_ASSERT_TRUE(st.ok());

  Threshold low;
  Threshold high;
  TEST_ASSERT_TRUE(dev.getThresholds(low, high).ok());
  TEST_ASSERT_GREATER_THAN_UINT32(0u, dev.thresholdToAdcCodes(low));
  TEST_ASSERT_TRUE(dev.thresholdToAdcCodes(high) > dev.thresholdToAdcCodes(low));
}

void test_threshold_lux_helpers_roundtrip() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(dev.setThresholdsLux(10.0f, 1000.0f).ok());

  float lowLux = 0.0f;
  float highLux = 0.0f;
  TEST_ASSERT_TRUE(dev.getThresholdsLux(lowLux, highLux).ok());
  TEST_ASSERT_TRUE(lowLux > 0.0f);
  TEST_ASSERT_TRUE(highLux > lowLux);

  Threshold low;
  Threshold high;
  TEST_ASSERT_TRUE(dev.getThresholds(low, high).ok());
  TEST_ASSERT_FLOAT_WITHIN(0.5f, dev.thresholdToLux(low), lowLux);
  TEST_ASSERT_FLOAT_WITHIN(5.0f, dev.thresholdToLux(high), highLux);
}

void test_read_flags_parses_and_clears_ready_flag() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.registers[cmd::REG_FLAGS] = static_cast<uint16_t>(cmd::MASK_CONVERSION_READY_FLAG |
                                                        cmd::MASK_FLAG_H);
  Flags flags;
  Status st = dev.readFlags(flags);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(flags.conversionReady);
  TEST_ASSERT_TRUE(flags.highThreshold);
  TEST_ASSERT_EQUAL_UINT16(0u, bus.registers[cmd::REG_FLAGS] & cmd::MASK_CONVERSION_READY_FLAG);
}

void test_clear_conversion_ready_flag_preserves_window_flags() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.registers[cmd::REG_FLAGS] = static_cast<uint16_t>(cmd::MASK_CONVERSION_READY_FLAG |
                                                        cmd::MASK_FLAG_H |
                                                        cmd::MASK_FLAG_L);
  TEST_ASSERT_TRUE(dev.clearConversionReadyFlag().ok());
  TEST_ASSERT_EQUAL_UINT16(0u, bus.registers[cmd::REG_FLAGS] & cmd::MASK_CONVERSION_READY_FLAG);
  TEST_ASSERT_NOT_EQUAL(0u, bus.registers[cmd::REG_FLAGS] & cmd::MASK_FLAG_H);
  TEST_ASSERT_NOT_EQUAL(0u, bus.registers[cmd::REG_FLAGS] & cmd::MASK_FLAG_L);
}

void test_clear_flags_uses_clear_on_read_semantics() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.registers[cmd::REG_FLAGS] = static_cast<uint16_t>(cmd::MASK_CONVERSION_READY_FLAG |
                                                        cmd::MASK_FLAG_H |
                                                        cmd::MASK_FLAG_L);
  TEST_ASSERT_TRUE(dev.clearFlags().ok());
  TEST_ASSERT_EQUAL_UINT16(0u, bus.registers[cmd::REG_FLAGS] &
                                  static_cast<uint16_t>(cmd::MASK_CONVERSION_READY_FLAG |
                                                        cmd::MASK_FLAG_H |
                                                        cmd::MASK_FLAG_L));
}

void test_write_int_configuration_rejects_bad_fixed_pattern() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.writeIntConfiguration(0x0011);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));
}

void test_read_device_id_returns_raw_register_value() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  uint16_t did = 0;
  Status st = dev.readDeviceId(did);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX16(cmd::DEVICE_ID_RESET, did);
}

void test_set_verify_crc_updates_cached_setting() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  TEST_ASSERT_TRUE(dev.getVerifyCrc());
  TEST_ASSERT_TRUE(dev.setVerifyCrc(false).ok());
  TEST_ASSERT_FALSE(dev.getVerifyCrc());
}

void test_decoded_register_helpers() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  TEST_ASSERT_TRUE(dev.setQuickWake(true).ok());
  TEST_ASSERT_TRUE(dev.setRange(Range::RANGE_3).ok());
  TEST_ASSERT_TRUE(dev.setConversionTime(ConversionTime::MS_25).ok());
  TEST_ASSERT_TRUE(dev.setFaultCount(FaultCount::FAULTS_4).ok());
  TEST_ASSERT_TRUE(dev.setIntDirection(IntDirection::PIN_OUTPUT).ok());
  TEST_ASSERT_TRUE(dev.setIntConfig(IntConfig::FIFO_FULL).ok());
  TEST_ASSERT_TRUE(dev.setBurstMode(false).ok());

  DeviceIdInfo did;
  TEST_ASSERT_TRUE(dev.readDeviceId(did).ok());
  TEST_ASSERT_EQUAL_HEX16(cmd::DEVICE_ID_RESET, did.raw);
  TEST_ASSERT_EQUAL_HEX16(cmd::DIDH_EXPECTED, did.didh);
  TEST_ASSERT_EQUAL_UINT8(0u, did.didl);
  TEST_ASSERT_TRUE(did.matchesExpected);

  ConfigurationInfo cfgInfo;
  TEST_ASSERT_TRUE(dev.readConfiguration(cfgInfo).ok());
  TEST_ASSERT_TRUE(cfgInfo.valid);
  TEST_ASSERT_TRUE(cfgInfo.quickWake);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::RANGE_3),
                          static_cast<uint8_t>(cfgInfo.range));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConversionTime::MS_25),
                          static_cast<uint8_t>(cfgInfo.conversionTime));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FaultCount::FAULTS_4),
                          static_cast<uint8_t>(cfgInfo.faultCount));

  IntConfigurationInfo intInfo;
  TEST_ASSERT_TRUE(dev.readIntConfiguration(intInfo).ok());
  TEST_ASSERT_TRUE(intInfo.valid);
  TEST_ASSERT_TRUE(intInfo.fixedPatternValid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(IntConfig::FIFO_FULL),
                          static_cast<uint8_t>(intInfo.intConfig));
  TEST_ASSERT_FALSE(intInfo.burstMode);
}

void test_read_register_block_and_sample_slot_helpers() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 1, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 2, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 3, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 4, true);
  bus.nowMs += 150;
  dev.tick(bus.nowMs);

  uint8_t raw[16] = {};
  TEST_ASSERT_TRUE(dev.readRegisters(cmd::REG_RESULT, raw, sizeof(raw)).ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(bus.registers[cmd::REG_RESULT] >> 8), raw[0]);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(bus.registers[cmd::REG_FIFO2_LSB_CRC] & 0xFF), raw[15]);

  Sample slot;
  TEST_ASSERT_TRUE(dev.readSampleSlot(2, slot).ok());
  TEST_ASSERT_EQUAL_UINT32(0x33333u, slot.mantissa);
}

void test_scale_and_counter_helpers() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 459.0f, dev.getRangeFullScaleLux(Range::RANGE_0));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 117441.0f, dev.getCurrentFullScaleLux());
  TEST_ASSERT_EQUAL_UINT8(20u, dev.getEffectiveBits(ConversionTime::MS_800));
  TEST_ASSERT_EQUAL_UINT8(17u, dev.getEffectiveBits());
  TEST_ASSERT_TRUE(dev.getCurrentResolutionLux() > 0.0f);
  TEST_ASSERT_EQUAL_UINT8(2u, dev.sampleCounterDelta(15, 1));

  TEST_ASSERT_TRUE(dev.setPackageVariant(PackageVariant::PICOSTAR).ok());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 328.0f, dev.getRangeFullScaleLux(Range::RANGE_0));
}

void test_soft_reset_moves_driver_to_uninit() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  TEST_ASSERT_TRUE(dev.softReset().ok());
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

void test_reset_and_reapply_restores_ready_and_config() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  TEST_ASSERT_TRUE(dev.setQuickWake(true).ok());
  TEST_ASSERT_TRUE(dev.setRange(Range::RANGE_3).ok());
  TEST_ASSERT_TRUE(dev.setFaultCount(FaultCount::FAULTS_4).ok());
  TEST_ASSERT_TRUE(dev.setBurstMode(false).ok());

  Status st = dev.resetAndReapply();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));

  uint16_t cfg = 0;
  uint16_t intCfg = 0;
  TEST_ASSERT_TRUE(dev.readConfiguration(cfg).ok());
  TEST_ASSERT_TRUE((cfg & cmd::MASK_QWAKE) != 0);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(Range::RANGE_3),
                           static_cast<uint16_t>((cfg & cmd::MASK_RANGE) >> cmd::BIT_RANGE));
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(FaultCount::FAULTS_4),
                           static_cast<uint16_t>(cfg & cmd::MASK_FAULT_COUNT));

  TEST_ASSERT_TRUE(dev.readIntConfiguration(intCfg).ok());
  TEST_ASSERT_EQUAL_UINT16(0u, intCfg & cmd::MASK_I2C_BURST);
}

void test_raw_transport_rejects_invalid_buffers() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  uint8_t byte = 0;
  uint8_t rx = 0;

  Status st = dev._i2cWriteRaw(nullptr, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteRaw(&byte, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(nullptr, 1, &rx, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(&byte, 0, &rx, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(&byte, 1, nullptr, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(&byte, 1, &rx, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_status_ok);
  RUN_TEST(test_status_error);
  RUN_TEST(test_status_in_progress);
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_get_last_sample_before_any_read);
  RUN_TEST(test_begin_rejects_missing_callbacks);
  RUN_TEST(test_begin_rejects_one_shot_startup_mode);
  RUN_TEST(test_begin_rejects_invalid_package_address_combo);
  RUN_TEST(test_begin_success_sets_ready_and_counters);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_recover_failure_updates_health);
  RUN_TEST(test_recover_success_returns_ready);
  RUN_TEST(test_start_conversion_wraparound_reaches_ready);
  RUN_TEST(test_read_sample_decodes_lux_and_crc);
  RUN_TEST(test_crc_mismatch_returns_error_when_enabled);
  RUN_TEST(test_crc_mismatch_allowed_when_verification_disabled);
  RUN_TEST(test_read_burst_decodes_fifo);
  RUN_TEST(test_read_burst_nonburst_path_decodes_fifo);
  RUN_TEST(test_set_thresholds_lux_updates_threshold_registers);
  RUN_TEST(test_threshold_lux_helpers_roundtrip);
  RUN_TEST(test_read_flags_parses_and_clears_ready_flag);
  RUN_TEST(test_clear_conversion_ready_flag_preserves_window_flags);
  RUN_TEST(test_clear_flags_uses_clear_on_read_semantics);
  RUN_TEST(test_write_int_configuration_rejects_bad_fixed_pattern);
  RUN_TEST(test_read_device_id_returns_raw_register_value);
  RUN_TEST(test_set_verify_crc_updates_cached_setting);
  RUN_TEST(test_decoded_register_helpers);
  RUN_TEST(test_read_register_block_and_sample_slot_helpers);
  RUN_TEST(test_scale_and_counter_helpers);
  RUN_TEST(test_soft_reset_moves_driver_to_uninit);
  RUN_TEST(test_reset_and_reapply_restores_ready_and_config);
  RUN_TEST(test_raw_transport_rejects_invalid_buffers);
  return UNITY_END();
}
