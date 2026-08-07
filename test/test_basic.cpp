/// @file test_basic.cpp
/// @brief Native contract tests for OPT4001 lifecycle and measurement behavior.

#include <unity.h>

#include <cmath>
#include <limits>
#include <cstring>
#include <type_traits>

#include "Arduino.h"
#include "Wire.h"

SerialClass Serial;
TwoWire Wire;

#define private public
#include "OPT4001/OPT4001.h"
#undef private

using namespace OPT4001;

namespace {

static_assert(std::is_default_constructible<OPT4001::OPT4001>::value,
              "OPT4001 must remain default constructible");
static_assert(!std::is_copy_constructible<OPT4001::OPT4001>::value,
              "OPT4001 copy construction must be disabled");
static_assert(!std::is_copy_assignable<OPT4001::OPT4001>::value,
              "OPT4001 copy assignment must be disabled");
static_assert(!std::is_move_constructible<OPT4001::OPT4001>::value,
              "OPT4001 move construction must be disabled");
static_assert(!std::is_move_assignable<OPT4001::OPT4001>::value,
              "OPT4001 move assignment must be disabled");

struct FakeBus {
  Status writeStatus = Status::Ok();
  Status readStatus = Status::Ok();
  uint16_t registers[0x20]{};
  uint32_t nowMs = 1234;
  uint32_t writeCalls = 0;
  uint32_t readCalls = 0;
  uint32_t failWriteCall = 0;
  uint32_t oneShotReadyAtMs = UINT32_MAX;
  uint32_t continuousReadyAtMs = UINT32_MAX;
  bool autoCompleteConversions = true;
  bool gpioLevel = true;

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
    autoCompleteConversions = true;
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
  if (!bus.autoCompleteConversions) {
    return;
  }

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
  if (bus->failWriteCall != 0U && bus->writeCalls == bus->failWriteCall) {
    bus->failWriteCall = 0;
    return Status::Error(Err::I2C_ERROR, "forced indexed write error", -22);
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

  const uint16_t endReg = static_cast<uint16_t>(reg) +
                          static_cast<uint16_t>((rxLen == 0U ? 0U : rxLen - 1U) / 2U);
  if (rxLen > 0U && reg <= cmd::REG_FLAGS && endReg >= cmd::REG_FLAGS) {
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

void fakeAdvancingYield(void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->nowMs++;
}

bool fakeGpioRead(int, void* user) {
  return static_cast<FakeBus*>(user)->gpioLevel;
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

uint8_t bitValue(uint32_t value, uint8_t bit) {
  return static_cast<uint8_t>((value >> bit) & 0x1U);
}

uint8_t xorBitList(uint32_t value, const uint8_t* bits, size_t count) {
  uint8_t parity = 0;
  for (size_t i = 0; i < count; ++i) {
    parity ^= bitValue(value, bits[i]);
  }
  return parity;
}

uint8_t datasheetCrc(uint8_t exponent, uint32_t mantissa, uint8_t counter) {
  uint8_t x0 = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    x0 ^= bitValue(exponent, i);
    x0 ^= bitValue(counter, i);
  }
  for (uint8_t i = 0; i < 20; ++i) {
    x0 ^= bitValue(mantissa, i);
  }

  const uint8_t c1Bits[] = {1, 3};
  const uint8_t r1Bits[] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19};
  const uint8_t e1Bits[] = {1, 3};
  const uint8_t r2Bits[] = {3, 7, 11, 15, 19};
  const uint8_t r3Bits[] = {3, 11, 19};
  const uint8_t x1 = static_cast<uint8_t>(xorBitList(counter, c1Bits, 2) ^
                                          xorBitList(mantissa, r1Bits, 10) ^
                                          xorBitList(exponent, e1Bits, 2));
  const uint8_t x2 = static_cast<uint8_t>(bitValue(counter, 3) ^
                                          xorBitList(mantissa, r2Bits, 5) ^
                                          bitValue(exponent, 3));
  const uint8_t x3 = xorBitList(mantissa, r3Bits, 3);
  return static_cast<uint8_t>((x3 << 3) | (x2 << 2) | (x1 << 1) | x0);
}

uint16_t sampleResultReg(uint8_t exponent, uint32_t mantissa) {
  return static_cast<uint16_t>(((static_cast<uint16_t>(exponent) << 12) & 0xF000) |
                               ((mantissa >> 8) & 0x0FFF));
}

uint16_t sampleLsbCrcReg(uint32_t mantissa, uint8_t counter, uint8_t crc) {
  return static_cast<uint16_t>(((mantissa & 0xFFU) << 8) |
                               ((counter & 0x0FU) << 4) |
                               (crc & 0x0FU));
}

void seedSample(FakeBus& bus, uint8_t msbReg, uint8_t exponent,
                uint32_t mantissa, uint8_t counter, bool goodCrc = true) {
  uint8_t crc = datasheetCrc(exponent, mantissa, counter);
  if (!goodCrc) {
    crc ^= 0x1U;
  }
  bus.registers[msbReg] = sampleResultReg(exponent, mantissa);
  bus.registers[static_cast<uint8_t>(msbReg + 1U)] =
      sampleLsbCrcReg(mantissa, counter, crc);
}

uint16_t packThresholdForTest(const Threshold& threshold) {
  return static_cast<uint16_t>(
      ((static_cast<uint16_t>(threshold.exponent) << cmd::BIT_THRESHOLD_EXPONENT) &
       cmd::MASK_THRESHOLD_EXPONENT) |
      (threshold.result & cmd::MASK_THRESHOLD_RESULT));
}

void assertStatusSame(const Status& expected, const Status& actual) {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.code),
                          static_cast<uint8_t>(actual.code));
  TEST_ASSERT_EQUAL_INT32(expected.detail, actual.detail);
  TEST_ASSERT_EQUAL_STRING(expected.msg, actual.msg);
}

void assertHardwareConfigDirty(OPT4001::OPT4001& dev, bool expected) {
  TEST_ASSERT_EQUAL(expected, dev.hardwareConfigDirty());

  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_EQUAL(expected, snap.hardwareConfigDirty);
}

void assertHardwareConfigDirtyError(OPT4001::OPT4001& dev,
                                    const Status& expected) {
  assertStatusSame(expected, dev.hardwareConfigDirtyError());
}

void assertSampleFields(const OPT4001::OPT4001& dev, const Sample& sample,
                        uint8_t exponent, uint32_t mantissa, uint8_t counter,
                        bool crcGood) {
  uint8_t expectedCrc = datasheetCrc(exponent, mantissa, counter);
  if (!crcGood) {
    expectedCrc ^= 0x1U;
  }
  const uint32_t adcCodes = mantissa << exponent;

  TEST_ASSERT_EQUAL_UINT16(sampleResultReg(exponent, mantissa), sample.resultReg);
  TEST_ASSERT_EQUAL_UINT16(sampleLsbCrcReg(mantissa, counter, expectedCrc),
                           sample.resultLsbCrcReg);
  TEST_ASSERT_EQUAL_UINT8(exponent, sample.exponent);
  TEST_ASSERT_EQUAL_UINT32(mantissa, sample.mantissa);
  TEST_ASSERT_EQUAL_UINT32(adcCodes, sample.adcCodes);
  TEST_ASSERT_EQUAL_UINT8(counter, sample.counter);
  TEST_ASSERT_EQUAL_UINT8(expectedCrc, sample.crc);
  TEST_ASSERT_EQUAL(crcGood, sample.crcValid);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, dev.adcCodesToLux(adcCodes), sample.lux);
}

void assertBurstFrameFields(const OPT4001::OPT4001& dev, const BurstFrame& frame,
                            bool newestCrcGood, bool fifo0CrcGood,
                            bool fifo1CrcGood, bool fifo2CrcGood) {
  assertSampleFields(dev, frame.newest, 0, 0x11111, 9, newestCrcGood);
  assertSampleFields(dev, frame.fifo0, 1, 0x22222, 8, fifo0CrcGood);
  assertSampleFields(dev, frame.fifo1, 2, 0x33333, 7, fifo1CrcGood);
  assertSampleFields(dev, frame.fifo2, 3, 0x44444, 6, fifo2CrcGood);
}

Status readSeededBurst(bool newestCrcGood, bool fifo0CrcGood,
                       bool fifo1CrcGood, bool fifo2CrcGood,
                       BurstFrame& frame, OPT4001::OPT4001& dev) {
  FakeBus* bus = static_cast<FakeBus*>(dev._config.i2cUser);
  seedSample(*bus, cmd::REG_RESULT, 0, 0x11111, 9, newestCrcGood);
  seedSample(*bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 8, fifo0CrcGood);
  seedSample(*bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 7, fifo1CrcGood);
  seedSample(*bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 6, fifo2CrcGood);
  bus->nowMs += 150;
  dev.tick(bus->nowMs);
  return dev.readBurst(frame);
}

Status makeThresholdPairDirty(OPT4001::OPT4001& dev, FakeBus& bus,
                              const Threshold& low,
                              const Threshold& high) {
  bus.failWriteCall = bus.writeCalls + 2U;
  return dev.setThresholds(low, high);
}

void assertApplyConfigFailureDirtyState(uint8_t relativeWrite,
                                        bool expectedDirty) {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  assertHardwareConfigDirty(dev, false);

  const Threshold low{0, static_cast<uint16_t>(0x0010U + relativeWrite)};
  const Threshold high{0, static_cast<uint16_t>(0x0080U + relativeWrite)};
  bus.failWriteCall = bus.writeCalls + relativeWrite;
  Status st = dev.enableThresholdInterrupt(low, high);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-22, st.detail);
  assertHardwareConfigDirty(dev, expectedDirty);
  if (expectedDirty) {
    assertHardwareConfigDirtyError(dev, st);
  }
}

void markConversionReady(FakeBus& bus) {
  bus.registers[cmd::REG_FLAGS] =
      static_cast<uint16_t>(bus.registers[cmd::REG_FLAGS] |
                            cmd::MASK_CONVERSION_READY_FLAG);
}

void forceHardwareMode(FakeBus& bus, Mode mode) {
  bus.registers[cmd::REG_CONFIGURATION] =
      static_cast<uint16_t>(
          (bus.registers[cmd::REG_CONFIGURATION] &
           static_cast<uint16_t>(~cmd::MASK_MODE)) |
          ((static_cast<uint16_t>(mode) << cmd::BIT_MODE) & cmd::MASK_MODE));
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

void test_error_names_are_complete_and_stable() {
  TEST_ASSERT_EQUAL_STRING("OK", errorName(Err::OK));
  TEST_ASSERT_EQUAL_STRING("MEASUREMENT_NOT_READY",
                           errorName(Err::CONVERSION_NOT_READY));
  TEST_ASSERT_EQUAL_STRING("OFFLINE", toString(Err::OFFLINE));
  TEST_ASSERT_EQUAL_STRING("UNKNOWN_ERROR", errorName(static_cast<Err>(0xFF)));
  TEST_ASSERT_EQUAL_STRING("READY", driverStateName(DriverState::READY));
  TEST_ASSERT_EQUAL_STRING("OFFLINE", toString(DriverState::OFFLINE));
  TEST_ASSERT_EQUAL_STRING("UNKNOWN_STATE",
                           driverStateName(static_cast<DriverState>(0xFF)));
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
  TEST_ASSERT_FALSE(dev.hasSample());
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

void test_package_address_matrix() {
  struct AddressCase {
    PackageVariant packageVariant;
    uint8_t address;
    bool valid;
  };
  const AddressCase cases[] = {
    {PackageVariant::PICOSTAR, 0x44, false},
    {PackageVariant::PICOSTAR, 0x45, true},
    {PackageVariant::PICOSTAR, 0x46, false},
    {PackageVariant::PICOSTAR, 0x47, false},
    {PackageVariant::SOT_5X3, 0x43, false},
    {PackageVariant::SOT_5X3, 0x44, true},
    {PackageVariant::SOT_5X3, 0x45, true},
    {PackageVariant::SOT_5X3, 0x46, true},
    {PackageVariant::SOT_5X3, 0x47, false},
  };

  for (const AddressCase& c : cases) {
    FakeBus bus;
    OPT4001::OPT4001 dev;
    Config cfg = makeConfig(bus);
    cfg.packageVariant = c.packageVariant;
    cfg.i2cAddress = c.address;
    Status st = dev.begin(cfg);
    if (c.valid) {
      TEST_ASSERT_TRUE(st.ok());
      TEST_ASSERT_TRUE(dev.isInitialized());
      TEST_ASSERT_EQUAL_UINT32(1u, bus.readCalls);
    } else {
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                              static_cast<uint8_t>(st.code));
      TEST_ASSERT_FALSE(dev.isInitialized());
      TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
      TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
    }
  }

  const PackageVariant invalidPackages[] = {
    static_cast<PackageVariant>(2),
    static_cast<PackageVariant>(255),
  };
  for (PackageVariant packageVariant : invalidPackages) {
    FakeBus bus;
    OPT4001::OPT4001 dev;
    Config cfg = makeConfig(bus);
    cfg.packageVariant = packageVariant;
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_FALSE(dev.isInitialized());
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
}

void test_invalid_begin_after_success_resets_default_runtime() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  cfg.mode = Mode::CONTINUOUS;
  cfg.quickWake = true;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  Config bad = makeConfig(bus);
  bad.i2cTimeoutMs = 0;
  Status st = dev.begin(bad);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastOkMs());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastErrorMs());
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  const Config& stored = dev.getConfig();
  TEST_ASSERT_NULL(stored.i2cWrite);
  TEST_ASSERT_NULL(stored.i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(cmd::I2C_ADDR_DEFAULT, stored.i2cAddress);
  TEST_ASSERT_EQUAL_UINT32(50u, stored.i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(5u, stored.offlineThreshold);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::POWER_DOWN),
                          static_cast<uint8_t>(stored.mode));
  TEST_ASSERT_FALSE(stored.quickWake);

  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.initialized);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(snap.state));
  TEST_ASSERT_EQUAL_UINT8(5u, snap.offlineThreshold);
  TEST_ASSERT_FALSE(snap.hasNowMsHook);
  TEST_ASSERT_FALSE(snap.sampleAvailable);
  TEST_ASSERT_FALSE(snap.conversionStarted);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, snap.lastLux);
}

void test_begin_normalizes_offline_threshold_on_stored_copy() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 0;

  Status st = dev.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, cfg.offlineThreshold);
  TEST_ASSERT_EQUAL_UINT8(1u, dev.getConfig().offlineThreshold);

  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_EQUAL_UINT8(1u, snap.offlineThreshold);
}

void test_get_settings_is_cache_only_without_bus_io() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.quickWake = true;
  cfg.verifyCrc = false;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "blocked read", -50);
  bus.writeStatus = Status::Error(Err::I2C_BUS, "blocked write", -51);

  SettingsSnapshot snap;
  Status st = dev.getSettings(snap);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_TRUE(snap.initialized);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(snap.state));
  TEST_ASSERT_EQUAL_HEX8(cmd::I2C_ADDR_DEFAULT, snap.i2cAddress);
  TEST_ASSERT_TRUE(snap.quickWake);
  TEST_ASSERT_FALSE(snap.verifyCrc);
  TEST_ASSERT_FALSE(snap.hardwareConfigDirty);
  TEST_ASSERT_TRUE(snap.hardwareConfigDirtyError.ok());
}

void test_begin_success_sets_ready_without_health_counts() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Status st = dev.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_GREATER_THAN_UINT32(0u, bus.readCalls + bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastOkMs());
}

void test_update_health_ignores_in_progress() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.nowMs = 2222u;
  const Status st = dev._updateHealth(Status{Err::IN_PROGRESS, 0, "pending"});
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastOkMs());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastErrorMs());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
}

void test_probe_accepts_valid_full_device_id() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  dev._config = makeConfig(bus);
  bus.registers[cmd::REG_DEVICE_ID] = cmd::DEVICE_ID_RESET;

  Status st = dev.probe();

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1U, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCalls);
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(0U, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0U, dev.totalFailures());
}

void test_probe_rejects_matching_didh_with_nonzero_high_id_bits() {
  const uint16_t badIds[] = {
      static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0x1000U),
      static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0x2000U),
      static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0x4000U),
      static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0x8000U),
      static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0xF000U),
  };

  for (uint16_t badId : badIds) {
    FakeBus bus;
    OPT4001::OPT4001 dev;
    dev._config = makeConfig(bus);
    bus.registers[cmd::REG_DEVICE_ID] = badId;

    Status st = dev.probe();

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_ID_MISMATCH),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(badId), st.detail);
    TEST_ASSERT_EQUAL_UINT32(1U, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0U, dev.totalSuccess());
    TEST_ASSERT_EQUAL_UINT32(0U, dev.totalFailures());
    TEST_ASSERT_FALSE(dev.isInitialized());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                            static_cast<uint8_t>(dev.state()));
  }
}

void test_begin_rejects_matching_didh_with_nonzero_high_id_bits() {
  const uint16_t badIds[] = {
      static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0x1000U),
      static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0x4000U),
  };

  for (uint16_t badId : badIds) {
    FakeBus bus;
    OPT4001::OPT4001 dev;
    bus.registers[cmd::REG_DEVICE_ID] = badId;

    Status st = dev.begin(makeConfig(bus));

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_ID_MISMATCH),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(badId), st.detail);
    TEST_ASSERT_EQUAL_UINT32(1U, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCalls);
    TEST_ASSERT_FALSE(dev.isInitialized());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                            static_cast<uint8_t>(dev.state()));
    TEST_ASSERT_EQUAL_UINT32(0U, dev.totalSuccess());
    TEST_ASSERT_EQUAL_UINT32(0U, dev.totalFailures());
  }
}

void test_recover_rejects_matching_didh_with_nonzero_high_id_bits() {
  const uint16_t badIds[] = {
      static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0x1000U),
      static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0x4000U),
  };

  for (uint16_t badId : badIds) {
    FakeBus bus;
    OPT4001::OPT4001 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

    bus.nowMs = 2468U;
    bus.registers[cmd::REG_DEVICE_ID] = badId;
    Status st = dev.recover();

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_ID_MISMATCH),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(badId), st.detail);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                            static_cast<uint8_t>(dev.state()));
    TEST_ASSERT_EQUAL_UINT32(1U, dev.totalFailures());
    TEST_ASSERT_EQUAL_UINT8(1U, dev.consecutiveFailures());
    TEST_ASSERT_EQUAL_UINT32(2468U, dev.lastErrorMs());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_ID_MISMATCH),
                            static_cast<uint8_t>(dev.lastError().code));
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(badId), dev.lastError().detail);
  }
}

void test_probe_preserves_transport_errors_and_detail() {
  struct ProbeMapCase {
    Err transportErr;
    int32_t detail;
  };

  const ProbeMapCase cases[] = {
      {Err::I2C_NACK_ADDR, -101},
      {Err::I2C_NACK_DATA, -102},
      {Err::I2C_TIMEOUT, -103},
      {Err::I2C_BUS, -104},
      {Err::I2C_ERROR, -105},
  };

  for (const ProbeMapCase& c : cases) {
    FakeBus bus;
    OPT4001::OPT4001 dev;
    dev._config = makeConfig(bus);
    bus.readStatus = Status::Error(c.transportErr, "transport error", c.detail);

    Status st = dev.probe();

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(c.transportErr),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(c.detail, st.detail);
    TEST_ASSERT_EQUAL_STRING("transport error", st.msg);
    TEST_ASSERT_EQUAL_UINT32(1U, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCalls);
    TEST_ASSERT_EQUAL_UINT32(0U, dev.totalSuccess());
    TEST_ASSERT_EQUAL_UINT32(0U, dev.totalFailures());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                            static_cast<uint8_t>(dev.state()));
  }
}

void test_probe_success_and_failure_do_not_update_health_or_state() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 2;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.nowMs = 3000U;
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "tracked timeout", -201);
  Flags flags;
  Status st = dev.readFlags(flags);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  const uint32_t degradedSuccess = dev.totalSuccess();
  const uint32_t degradedFailures = dev.totalFailures();
  const uint8_t degradedConsecutive = dev.consecutiveFailures();
  const uint32_t degradedLastOk = dev.lastOkMs();
  const uint32_t degradedLastError = dev.lastErrorMs();

  bus.readStatus = Status::Ok();
  st = dev.probe();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(degradedSuccess, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(degradedFailures, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(degradedConsecutive, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(degradedLastOk, dev.lastOkMs());
  TEST_ASSERT_EQUAL_UINT32(degradedLastError, dev.lastErrorMs());

  bus.readStatus = Status::Error(Err::I2C_BUS, "raw bus error", -202);
  st = dev.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-202, st.detail);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(degradedSuccess, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(degradedFailures, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(degradedConsecutive, dev.consecutiveFailures());

  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "tracked timeout", -203);
  st = dev.readFlags(flags);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  const uint32_t offlineSuccess = dev.totalSuccess();
  const uint32_t offlineFailures = dev.totalFailures();
  const uint8_t offlineConsecutive = dev.consecutiveFailures();

  bus.readStatus = Status::Ok();
  st = dev.probe();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(offlineSuccess, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(offlineFailures, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(offlineConsecutive, dev.consecutiveFailures());
}

void test_probe_failure_does_not_update_health() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t beforeSuccess = dev.totalSuccess();
  const uint32_t beforeFailures = dev.totalFailures();

  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced timeout", -7);
  Status st = dev.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-7, st.detail);
  TEST_ASSERT_EQUAL_UINT32(beforeSuccess, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(beforeFailures, dev.totalFailures());
}

void test_probe_id_mismatch_does_not_update_health() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t beforeSuccess = dev.totalSuccess();
  const uint32_t beforeFailures = dev.totalFailures();
  bus.registers[cmd::REG_DEVICE_ID] = 0x0000;

  Status st = dev.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_ID_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(beforeSuccess, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(beforeFailures, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
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

void test_recover_id_mismatch_updates_health() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.nowMs = 2222;
  bus.registers[cmd::REG_DEVICE_ID] = 0x0000;
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_ID_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(2222u, dev.lastErrorMs());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_ID_MISMATCH),
                          static_cast<uint8_t>(dev.lastError().code));
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

void test_read_blocking_times_out_with_stalled_clock() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Sample sample;
  Status st = dev.readBlocking(sample, Mode::ONE_SHOT, 5);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT), static_cast<uint8_t>(st.code));
  SettingsSnapshot timedOut;
  TEST_ASSERT_TRUE(dev.getSettings(timedOut).ok());
  TEST_ASSERT_TRUE(timedOut.conversionStarted);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::ONE_SHOT),
                          static_cast<uint8_t>(timedOut.pendingMode));

  FakeBus continuousBus;
  OPT4001::OPT4001 continuousDev;
  Config cfg = makeConfig(continuousBus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(continuousDev.begin(cfg).ok());

  st = continuousDev.readBlocking(sample, 5);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT), static_cast<uint8_t>(st.code));
}

void test_blocking_reads_accept_full_uint32_timeout_range() {
  FakeBus continuousBus;
  OPT4001::OPT4001 continuousDev;
  Config continuousCfg = makeConfig(continuousBus);
  continuousCfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(continuousDev.begin(continuousCfg).ok());
  seedSample(continuousBus, cmd::REG_RESULT, 0, 0x12345, 1, true);
  markConversionReady(continuousBus);

  Sample sample;
  Status st = continuousDev.readFreshBlocking(sample, UINT32_MAX);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0x12345U, sample.mantissa);

  FakeBus oneShotBus;
  OPT4001::OPT4001 oneShotDev;
  Config oneShotCfg = makeConfig(oneShotBus);
  oneShotCfg.conversionTime = ConversionTime::US_600;
  oneShotCfg.quickWake = true;
  oneShotCfg.cooperativeYield = fakeAdvancingYield;
  TEST_ASSERT_TRUE(oneShotDev.begin(oneShotCfg).ok());
  seedSample(oneShotBus, cmd::REG_RESULT, 0, 0x23456, 2, true);

  st = oneShotDev.readFreshBlocking(sample, Mode::ONE_SHOT, UINT32_MAX);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0x23456U, sample.mantissa);
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
  TEST_ASSERT_TRUE(dev.hasSample());
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.sampleTimestampMs());

  Sample cached;
  TEST_ASSERT_TRUE(dev.getLastSample(cached).ok());
  TEST_ASSERT_EQUAL_UINT32(sample.adcCodes, cached.adcCodes);

  bus.nowMs += 250;
  TEST_ASSERT_EQUAL_UINT32(250u, dev.sampleAgeMs(bus.nowMs));
}

void test_raw_lux_vectors_use_64_bit_intermediates() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  struct RawVector {
    uint8_t exponent;
    uint32_t mantissa;
    uint64_t adcCodes;
    float sotLux;
    float picoLux;
  };
  const RawVector vectors[] = {
    {0, 0x00000, 0ULL, 0.0f, 0.0f},
    {0, 0x00001, 1ULL, 0.0004375f, 0.0003125f},
    {2, 0x34567, 857500ULL, 375.15625f, 267.96875f},
    {8, 0x00001, 256ULL, 0.112f, 0.08f},
    {8, 0xFFFFF, 268435200ULL, 117440.4f, 83886.0f},
  };

  for (const RawVector& v : vectors) {
    uint64_t adcCodes = UINT64_MAX;
    float lux = -1.0f;
    const float tolerance = (v.adcCodes > 1000000ULL) ? 0.1f : 0.001f;
    TEST_ASSERT_TRUE(dev.rawToAdcCodes(v.exponent, v.mantissa, adcCodes).ok());
    TEST_ASSERT_EQUAL_UINT64(v.adcCodes, adcCodes);
    TEST_ASSERT_TRUE(dev.rawToLux(v.exponent, v.mantissa, lux).ok());
    TEST_ASSERT_FLOAT_WITHIN(tolerance, v.sotLux, lux);
    TEST_ASSERT_FLOAT_WITHIN(tolerance, v.sotLux,
                             dev.rawToLux(v.exponent, v.mantissa));

    TEST_ASSERT_TRUE(dev.setPackageVariant(PackageVariant::PICOSTAR).ok());
    TEST_ASSERT_TRUE(dev.rawToLux(v.exponent, v.mantissa, lux).ok());
    TEST_ASSERT_FLOAT_WITHIN(tolerance, v.picoLux, lux);
    TEST_ASSERT_TRUE(dev.setPackageVariant(PackageVariant::SOT_5X3).ok());
  }
}

void test_raw_lux_rejects_invalid_result_fields_without_shift_ub() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  struct InvalidRaw {
    uint8_t exponent;
    uint32_t mantissa;
  };
  const InvalidRaw invalid[] = {
    {9, 0x00001},
    {15, 0x00001},
    {31, 0x00001},
    {32, 0x00001},
    {0, 0x100000},
  };

  for (const InvalidRaw& v : invalid) {
    uint64_t adcCodes = UINT64_MAX;
    float lux = 1.0f;
    Status st = dev.rawToAdcCodes(v.exponent, v.mantissa, adcCodes);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT64(0ULL, adcCodes);

    st = dev.rawToLux(v.exponent, v.mantissa, lux);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_TRUE(std::isnan(lux));
    TEST_ASSERT_TRUE(std::isnan(dev.rawToLux(v.exponent, v.mantissa)));
  }
}

void test_decode_rejects_invalid_result_exponent_without_cache_update() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 1, 0x12345, 1, true);
  bus.nowMs += 150;
  dev.tick(bus.nowMs);
  Sample valid;
  TEST_ASSERT_TRUE(dev.readSample(valid).ok());
  TEST_ASSERT_TRUE(dev.hasSample());
  TEST_ASSERT_EQUAL_UINT32(0x12345u, valid.mantissa);

  seedSample(bus, cmd::REG_RESULT, 9, 0x00001, 2, true);
  markConversionReady(bus);
  Sample invalid;
  Status st = dev.readSample(invalid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(std::isnan(invalid.lux));

  Sample cached;
  TEST_ASSERT_TRUE(dev.getLastSample(cached).ok());
  TEST_ASSERT_EQUAL_UINT8(valid.exponent, cached.exponent);
  TEST_ASSERT_EQUAL_UINT32(valid.mantissa, cached.mantissa);
  TEST_ASSERT_EQUAL_UINT32(valid.adcCodes, cached.adcCodes);
}

void test_crc_vectors_use_datasheet_oracle() {
  OPT4001::OPT4001 dev;

  struct CrcVector {
    uint8_t exponent;
    uint32_t mantissa;
    uint8_t counter;
    uint8_t crc;
    uint16_t resultReg;
    uint16_t lsbCrcReg;
  };
  const CrcVector vectors[] = {
    {0, 0x00000, 0, 0x0, 0x0000, 0x0000},
    {0, 0x00001, 0, 0x1, 0x0000, 0x0101},
    {1, 0x12345, 6, 0x2, 0x1123, 0x4562},
    {2, 0x34567, 9, 0x7, 0x2345, 0x6797},
    {3, 0x44444, 4, 0x2, 0x3444, 0x4442},
    {8, 0xFFFFF, 15, 0xF, 0x8FFF, 0xFFFF},
    {8, 0x80000, 1, 0x9, 0x8800, 0x0019},
    {4, 0x00F0F, 10, 0x5, 0x400F, 0x0FA5},
    {7, 0xABCDE, 5, 0xE, 0x7ABC, 0xDE5E},
  };

  for (const CrcVector& v : vectors) {
    TEST_ASSERT_EQUAL_HEX8(v.crc, datasheetCrc(v.exponent, v.mantissa, v.counter));
    TEST_ASSERT_EQUAL_HEX16(v.resultReg, sampleResultReg(v.exponent, v.mantissa));
    TEST_ASSERT_EQUAL_HEX16(v.lsbCrcReg, sampleLsbCrcReg(v.mantissa, v.counter, v.crc));

    Sample sample;
    Status st = dev._decodeSampleRegisters(v.resultReg, v.lsbCrcReg, sample);
    TEST_ASSERT_TRUE(st.ok());
    TEST_ASSERT_TRUE(sample.crcValid);
    TEST_ASSERT_EQUAL_UINT8(v.exponent, sample.exponent);
    TEST_ASSERT_EQUAL_UINT32(v.mantissa, sample.mantissa);
    TEST_ASSERT_EQUAL_UINT8(v.counter, sample.counter);
    TEST_ASSERT_EQUAL_HEX8(v.crc, sample.crc);
  }
}

void test_crc_mismatch_preserves_received_crc_and_decode_fields() {
  OPT4001::OPT4001 dev;
  const uint8_t expectedCrc = datasheetCrc(0, 0x23456, 3);
  const uint8_t receivedCrc = static_cast<uint8_t>(expectedCrc ^ 0x1U);
  const uint16_t resultReg = sampleResultReg(0, 0x23456);
  const uint16_t lsbCrcReg = sampleLsbCrcReg(0x23456, 3, receivedCrc);

  Sample sample;
  Status st = dev._decodeSampleRegisters(resultReg, lsbCrcReg, sample);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(sample.crcValid);
  TEST_ASSERT_EQUAL_UINT8(receivedCrc, sample.crc);
  TEST_ASSERT_EQUAL_UINT8(0u, sample.exponent);
  TEST_ASSERT_EQUAL_UINT32(0x23456u, sample.mantissa);
  TEST_ASSERT_EQUAL_UINT8(3u, sample.counter);

  dev._config.verifyCrc = false;
  st = dev._decodeSampleRegisters(resultReg, lsbCrcReg, sample);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(sample.crcValid);
  TEST_ASSERT_EQUAL_UINT8(receivedCrc, sample.crc);
}

void test_zero_timestamp_sample_age_uses_valid_flag() {
  OPT4001::OPT4001 dev;
  TEST_ASSERT_EQUAL_UINT32(0u, dev.sampleAgeMs(123u));

  dev._lastSampleValid = true;
  dev._lastSampleTimestampMs = 0;
  TEST_ASSERT_EQUAL_UINT32(123u, dev.sampleAgeMs(123u));
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

void test_lux_helpers_preserve_outputs_on_crc_warning() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 1, 0x12345, 6, false);
  bus.nowMs += 150;
  dev.tick(bus.nowMs);

  const uint32_t adcCodes = 0x12345u << 1U;
  const float expectedLux = dev.adcCodesToLux(adcCodes);
  const uint64_t expectedMicroLux =
      (static_cast<uint64_t>(adcCodes) * cmd::MICRO_LUX_NUMERATOR_SOT_5X3 + 5ULL) / 10ULL;

  float lux = 0.0f;
  Status st = dev.readLux(lux);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR), static_cast<uint8_t>(st.code));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, expectedLux, lux);

  seedSample(bus, cmd::REG_RESULT, 1, 0x12345, 7, false);
  markConversionReady(bus);
  uint64_t microLux = 0;
  st = dev.readMicroLux(microLux);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT64(expectedMicroLux, microLux);

  seedSample(bus, cmd::REG_RESULT, 1, 0x12345, 8, false);
  markConversionReady(bus);
  uint32_t milliLux = 0;
  st = dev.readMilliLux(milliLux);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>((expectedMicroLux + 500ULL) / 1000ULL), milliLux);
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

void test_read_burst_all_four_slots_valid_populates_fields_and_counter_order() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  BurstFrame frame;
  Status st = readSeededBurst(true, true, true, true, frame, dev);
  TEST_ASSERT_TRUE(st.ok());
  assertBurstFrameFields(dev, frame, true, true, true, true);
}

void test_read_burst_newest_crc_error_populates_all_slots() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  BurstFrame frame;
  Status st = readSeededBurst(false, true, true, true, frame, dev);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR),
                          static_cast<uint8_t>(st.code));
  assertBurstFrameFields(dev, frame, false, true, true, true);
}

void test_read_burst_middle_fifo_crc_error_populates_all_slots() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  BurstFrame frame;
  Status st = readSeededBurst(true, true, false, true, frame, dev);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR),
                          static_cast<uint8_t>(st.code));
  assertBurstFrameFields(dev, frame, true, true, false, true);
}

void test_read_burst_last_fifo_crc_error_populates_all_slots() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  BurstFrame frame;
  Status st = readSeededBurst(true, true, true, false, frame, dev);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR),
                          static_cast<uint8_t>(st.code));
  assertBurstFrameFields(dev, frame, true, true, true, false);
}

void test_read_burst_multiple_crc_errors_populates_all_slots() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  BurstFrame frame;
  Status st = readSeededBurst(false, true, false, false, frame, dev);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR),
                          static_cast<uint8_t>(st.code));
  assertBurstFrameFields(dev, frame, false, true, false, false);
}

void test_read_burst_nonburst_path_aggregates_crc_and_populates_all_slots() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.burstMode = false;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  BurstFrame frame;
  Status st = readSeededBurst(true, false, true, false, frame, dev);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR),
                          static_cast<uint8_t>(st.code));
  assertBurstFrameFields(dev, frame, true, false, true, false);
}

void test_poll_read_burst_status_and_burst_share_budget_two() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 9, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 8, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 7, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 6, true);
  markConversionReady(bus);

  const uint32_t readsBefore = bus.readCalls;
  TEST_ASSERT_TRUE(dev.startReadBurst().inProgress());
  Status st = dev.poll(bus.nowMs, 2);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_TRUE(dev.lastPollStatus().ok());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 2U, bus.readCalls);

  BurstFrame frame;
  TEST_ASSERT_TRUE(dev.getLastBurst(frame).ok());
  assertBurstFrameFields(dev, frame, true, true, true, true);

  Sample cached;
  TEST_ASSERT_TRUE(dev.getLastSample(cached).ok());
  TEST_ASSERT_EQUAL_UINT32(frame.newest.mantissa, cached.mantissa);
}

void test_poll_read_sample_uses_burst_primitive_without_publishing_burst() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 9, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 8, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 7, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 6, true);
  markConversionReady(bus);

  const uint32_t readsBefore = bus.readCalls;
  TEST_ASSERT_TRUE(dev.startReadSample().inProgress());
  Status st = dev.poll(bus.nowMs, 2);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 2U, bus.readCalls);

  Sample sample;
  TEST_ASSERT_TRUE(dev.getLastSample(sample).ok());
  TEST_ASSERT_EQUAL_UINT32(0x11111u, sample.mantissa);

  BurstFrame frame;
  st = dev.getLastBurst(frame);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::MEASUREMENT_NOT_READY),
                          static_cast<uint8_t>(st.code));
}

void test_poll_read_burst_budget_one_splits_status_and_burst() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 1, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 2, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 3, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 4, true);
  markConversionReady(bus);

  const uint32_t readsBefore = bus.readCalls;
  TEST_ASSERT_TRUE(dev.startReadBurst().inProgress());

  Status st = dev.poll(bus.nowMs, 1);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 1U, bus.readCalls);

  st = dev.poll(bus.nowMs, 1);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 2U, bus.readCalls);
}

void test_poll_zero_budget_does_not_touch_i2c() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 9, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 8, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 7, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 6, true);
  markConversionReady(bus);

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  TEST_ASSERT_TRUE(dev.startReadBurst().inProgress());

  Status st = dev.poll(bus.nowMs, 0);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  st = dev.poll(bus.nowMs, 2);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 2U, bus.readCalls);
}

void test_poll_read_burst_delay_gate_consumes_no_instruction() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.conversionTime = ConversionTime::MS_100;
  bus.autoCompleteConversions = false;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t startMs = bus.nowMs;
  TEST_ASSERT_TRUE(dev.startReadBurst().inProgress());

  Status st = dev.poll(startMs + 99U, 2);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 1, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 2, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 3, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 4, true);
  markConversionReady(bus);
  bus.nowMs = startMs + 100U;
  st = dev.poll(bus.nowMs, 2);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 2U, bus.readCalls);
}

void test_poll_busy_blocks_tick_and_synchronous_i2c() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.conversionTime = ConversionTime::MS_100;
  bus.autoCompleteConversions = false;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t startMs = bus.nowMs;
  TEST_ASSERT_TRUE(dev.startReadBurst().inProgress());

  bus.nowMs = startMs + 100U;
  dev.tick(bus.nowMs);
  TEST_ASSERT_TRUE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  Flags flags;
  Status st = dev.readFlags(flags);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);

  st = dev.setPackageVariant(PackageVariant::PICOSTAR);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PackageVariant::SOT_5X3),
                          static_cast<uint8_t>(dev.getPackageVariant()));
  st = dev.setVerifyCrc(false);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.getVerifyCrc());
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 9, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 8, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 7, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 6, true);
  markConversionReady(bus);
  st = dev.poll(bus.nowMs, 2);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 2U, bus.readCalls);
}

void test_poll_read_burst_fifo_full_gate_uses_four_sample_cadence() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.conversionTime = ConversionTime::MS_100;
  cfg.intConfig = IntConfig::FIFO_FULL;
  bus.autoCompleteConversions = false;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t startMs = bus.nowMs;
  TEST_ASSERT_TRUE(dev.startReadBurst().inProgress());

  Status st = dev.poll(startMs + 399U, 2);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 9, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 8, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 7, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 6, true);
  markConversionReady(bus);
  st = dev.poll(startMs + 400U, 2);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 2U, bus.readCalls);
}

void test_poll_read_burst_one_shot_forced_auto_gate_uses_full_budget() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::POWER_DOWN;
  cfg.conversionTime = ConversionTime::US_600;
  cfg.quickWake = false;
  bus.autoCompleteConversions = false;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  Status st = dev.startConversion(Mode::ONE_SHOT_FORCED_AUTO);
  TEST_ASSERT_TRUE(st.inProgress());
  const uint32_t startMs = bus.nowMs;
  const uint32_t readyMs = startMs + dev.getOneShotBudgetMs(Mode::ONE_SHOT_FORCED_AUTO);
  const uint32_t readsBefore = bus.readCalls;

  TEST_ASSERT_TRUE(dev.startReadBurst().inProgress());
  st = dev.poll(readyMs - 1U, 2);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 9, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 8, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 7, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 6, true);
  markConversionReady(bus);
  st = dev.poll(readyMs, 2);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 2U, bus.readCalls);
}

void test_poll_config_apply_honors_instruction_budget() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t writesBefore = bus.writeCalls;
  Status st = dev.startConfigureMeasurement(Range::RANGE_2,
                                            ConversionTime::MS_25,
                                            Mode::CONTINUOUS,
                                            true);
  TEST_ASSERT_TRUE(st.inProgress());

  st = dev.poll(bus.nowMs, 2);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 2U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::AUTO),
                          static_cast<uint8_t>(dev.getRange()));

  st = dev.poll(bus.nowMs, 1);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 3U, bus.writeCalls);

  st = dev.poll(bus.nowMs, 1);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 4U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::RANGE_2),
                          static_cast<uint8_t>(dev.getRange()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConversionTime::MS_25),
                          static_cast<uint8_t>(dev.getConversionTime()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::CONTINUOUS),
                          static_cast<uint8_t>(dev.getMode()));
  TEST_ASSERT_TRUE(dev.getQuickWake());
  assertHardwareConfigDirty(dev, false);
}

void test_poll_config_apply_failure_stops_and_reports_status() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t writesBefore = bus.writeCalls;
  bus.failWriteCall = writesBefore + 3U;
  Status st = dev.startConfigureMeasurement(Range::RANGE_3,
                                            ConversionTime::MS_50,
                                            Mode::CONTINUOUS,
                                            true);
  TEST_ASSERT_TRUE(st.inProgress());

  st = dev.poll(bus.nowMs, 4);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-22, st.detail);
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 3U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::AUTO),
                          static_cast<uint8_t>(dev.getRange()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::POWER_DOWN),
                          static_cast<uint8_t>(dev.getMode()));
  assertHardwareConfigDirty(dev, true);
  assertHardwareConfigDirtyError(dev, st);
}

void test_poll_reset_and_reapply_failure_after_reset_stops_dirty_uninit() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(dev.setRange(Range::RANGE_4).ok());
  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 9, true);
  seedSample(bus, cmd::REG_FIFO0_MSB, 1, 0x22222, 8, true);
  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 7, true);
  seedSample(bus, cmd::REG_FIFO2_MSB, 3, 0x44444, 6, true);
  markConversionReady(bus);
  BurstFrame frame;
  TEST_ASSERT_TRUE(dev.readBurst(frame).ok());
  TEST_ASSERT_TRUE(dev.hasSample());
  TEST_ASSERT_TRUE(dev.getLastBurst(frame).ok());

  const uint32_t writesBefore = bus.writeCalls;
  bus.failWriteCall = writesBefore + 2U;
  Status st = dev.startResetAndReapply();
  TEST_ASSERT_TRUE(st.inProgress());

  st = dev.poll(bus.nowMs, 1);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(dev.pollBusy());
  TEST_ASSERT_TRUE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 1U, bus.writeCalls);

  st = dev.poll(bus.nowMs, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  const Status failureStatus = st;
  TEST_ASSERT_FALSE(dev.pollBusy());
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_FALSE(dev.hasSample());
  st = dev.getLastBurst(frame);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::MEASUREMENT_NOT_READY),
                          static_cast<uint8_t>(st.code));
  assertHardwareConfigDirty(dev, true);
  assertHardwareConfigDirtyError(dev, failureStatus);
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

void test_threshold_adc_vectors_use_64_bit_and_legacy_saturates() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  struct ThresholdVector {
    Threshold threshold;
    uint64_t adcCodes;
    uint32_t legacyCodes;
    float sotLux;
  };
  const ThresholdVector vectors[] = {
    {{0, 0x000}, 0ULL, 0U, 0.0f},
    {{0, 0x001}, 256ULL, 256U, 0.112f},
    {{0, 0x0FFF}, 1048320ULL, 1048320U, 458.64f},
    {{1, 0x0800}, 1048576ULL, 1048576U, 458.752f},
    {{11, 0x0FFF}, 2146959360ULL, 2146959360U, 939294.72f},
    {{12, 0x0FFF}, 4293918720ULL, 4293918720U, 1878589.44f},
    {{15, 0x0FFF}, 34351349760ULL, UINT32_MAX, 15028715.52f},
  };

  for (const ThresholdVector& v : vectors) {
    uint64_t adcCodes = UINT64_MAX;
    TEST_ASSERT_TRUE(dev.thresholdToAdcCodes(v.threshold, adcCodes).ok());
    TEST_ASSERT_EQUAL_UINT64(v.adcCodes, adcCodes);
    TEST_ASSERT_EQUAL_UINT32(v.legacyCodes, dev.thresholdToAdcCodes(v.threshold));
    TEST_ASSERT_FLOAT_WITHIN(4.0f, v.sotLux, dev.thresholdToLux(v.threshold));
  }

  Threshold invalidExp{16, 0x0001};
  uint64_t adcCodes = UINT64_MAX;
  Status st = dev.thresholdToAdcCodes(invalidExp, adcCodes);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT64(0ULL, adcCodes);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, dev.thresholdToAdcCodes(invalidExp));
  TEST_ASSERT_TRUE(std::isnan(dev.thresholdToLux(invalidExp)));

  Threshold invalidResult{0, 0x1000};
  st = dev.thresholdToAdcCodes(invalidResult, adcCodes);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
}

void test_threshold_interrupt_ordering_uses_64_bit_codes() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Threshold highMagnitudeLow{13, 0x0800};
  Threshold lowerMagnitudeHigh{12, 0x0FFF};
  Status st = dev.enableThresholdInterrupt(highMagnitudeLow, lowerMagnitudeHigh);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  Threshold low{12, 0x0FFF};
  Threshold high{13, 0x0800};
  st = dev.enableThresholdInterrupt(low, high);
  TEST_ASSERT_TRUE(st.ok());

  Threshold readLow;
  Threshold readHigh;
  TEST_ASSERT_TRUE(dev.getThresholds(readLow, readHigh).ok());
  TEST_ASSERT_EQUAL_UINT8(low.exponent, readLow.exponent);
  TEST_ASSERT_EQUAL_UINT16(low.result, readLow.result);
  TEST_ASSERT_EQUAL_UINT8(high.exponent, readHigh.exponent);
  TEST_ASSERT_EQUAL_UINT16(high.result, readHigh.result);
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

void test_read_int_pin_asserted_uses_configured_polarity() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.intPin = 7;
  cfg.gpioRead = fakeGpioRead;
  cfg.gpioUser = &bus;
  cfg.interruptPolarity = InterruptPolarity::ACTIVE_LOW;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bool asserted = true;
  bus.gpioLevel = true;
  TEST_ASSERT_TRUE(dev.readIntPinAsserted(asserted).ok());
  TEST_ASSERT_FALSE(asserted);

  bus.gpioLevel = false;
  TEST_ASSERT_TRUE(dev.readIntPinAsserted(asserted).ok());
  TEST_ASSERT_TRUE(asserted);

  TEST_ASSERT_TRUE(dev.setInterruptPolarity(InterruptPolarity::ACTIVE_HIGH).ok());
  bus.gpioLevel = true;
  TEST_ASSERT_TRUE(dev.readIntPinAsserted(asserted).ok());
  TEST_ASSERT_TRUE(asserted);
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
  TEST_ASSERT_EQUAL_UINT8(cmd::DIDL_EXPECTED, did.didl);
  TEST_ASSERT_TRUE(did.reservedBitsClear);
  TEST_ASSERT_TRUE(did.matchesExpected);

  DeviceIdInfo badDidl;
  dev.decodeDeviceId(static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0x1000U), badDidl);
  TEST_ASSERT_EQUAL_HEX16(cmd::DIDH_EXPECTED, badDidl.didh);
  TEST_ASSERT_EQUAL_UINT8(1u, badDidl.didl);
  TEST_ASSERT_TRUE(badDidl.reservedBitsClear);
  TEST_ASSERT_FALSE(badDidl.matchesExpected);

  DeviceIdInfo badReserved;
  dev.decodeDeviceId(static_cast<uint16_t>(cmd::DIDH_EXPECTED | 0x4000U), badReserved);
  TEST_ASSERT_EQUAL_HEX16(cmd::DIDH_EXPECTED, badReserved.didh);
  TEST_ASSERT_EQUAL_UINT8(cmd::DIDL_EXPECTED, badReserved.didl);
  TEST_ASSERT_FALSE(badReserved.reservedBitsClear);
  TEST_ASSERT_FALSE(badReserved.matchesExpected);

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

void test_picostar_rejects_impossible_int_hook_configuration() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.packageVariant = PackageVariant::PICOSTAR;
  cfg.intPin = 4;
  cfg.gpioRead = fakeGpioRead;
  cfg.gpioUser = &bus;

  Status st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(0U, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCalls);
}

void test_runtime_package_switch_rejects_picostar_with_int_hook() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.intPin = 4;
  cfg.gpioRead = fakeGpioRead;
  cfg.gpioUser = &bus;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  const Status st = dev.setPackageVariant(PackageVariant::PICOSTAR);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PackageVariant::SOT_5X3),
                          static_cast<uint8_t>(dev.getPackageVariant()));
}

void test_generic_raw_flags_access_synchronizes_readiness_without_dirtying_config() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.registers[cmd::REG_FLAGS] = cmd::MASK_CONVERSION_READY_FLAG;
  dev._sampleAvailable = true;
  dev._conversionReady = true;
  uint16_t raw = 0;
  TEST_ASSERT_TRUE(dev.readRegister16(cmd::REG_FLAGS, raw).ok());
  TEST_ASSERT_EQUAL_HEX16(cmd::MASK_CONVERSION_READY_FLAG, raw);
  TEST_ASSERT_FALSE(dev._sampleAvailable);
  TEST_ASSERT_FALSE(dev._conversionReady);

  bus.registers[cmd::REG_FLAGS] = cmd::MASK_CONVERSION_READY_FLAG;
  dev._sampleAvailable = true;
  dev._conversionReady = true;
  uint8_t bytes[2] = {};
  TEST_ASSERT_TRUE(dev.readRegisters(cmd::REG_FLAGS, bytes, sizeof(bytes)).ok());
  TEST_ASSERT_FALSE(dev._sampleAvailable);
  TEST_ASSERT_FALSE(dev._conversionReady);

  dev._sampleAvailable = true;
  dev._conversionReady = true;
  TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_FLAGS, 0x0001).ok());
  TEST_ASSERT_FALSE(dev._sampleAvailable);
  TEST_ASSERT_FALSE(dev._conversionReady);
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());

  dev._sampleAvailable = true;
  dev._conversionReady = true;
  TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_FLAGS, 0x0000).ok());
  TEST_ASSERT_TRUE(dev._sampleAvailable);
  TEST_ASSERT_TRUE(dev._conversionReady);
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
}

void test_burst_enabled_single_sample_uses_one_coherent_result_transaction() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  seedSample(bus, cmd::REG_RESULT, 2, 0x34567, 3, true);
  const uint32_t readsBefore = bus.readCalls;

  Sample sample;
  Status st = dev.readLatestSample(sample);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0x34567u, sample.mantissa);
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 1U, bus.readCalls);
}

void test_nonburst_register_block_uses_bounded_per_register_reads() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.burstMode = false;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.registers[cmd::REG_RESULT] = 0x1234;
  bus.registers[cmd::REG_RESULT_LSB_CRC] = 0x5678;
  bus.registers[cmd::REG_FIFO0_MSB] = 0x9ABC;
  uint8_t bytes[5] = {};
  const uint32_t readsBefore = bus.readCalls;

  Status st = dev.readRegisters(cmd::REG_RESULT, bytes, sizeof(bytes));

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 3U, bus.readCalls);
  TEST_ASSERT_EQUAL_HEX8(0x12, bytes[0]);
  TEST_ASSERT_EQUAL_HEX8(0x34, bytes[1]);
  TEST_ASSERT_EQUAL_HEX8(0x56, bytes[2]);
  TEST_ASSERT_EQUAL_HEX8(0x78, bytes[3]);
  TEST_ASSERT_EQUAL_HEX8(0x9A, bytes[4]);
}

void test_register_block_rejects_size_t_span_overflow_before_transport() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  uint8_t byte = 0;
  const uint32_t readsBefore = bus.readCalls;
  const size_t wrappedSpanLength = static_cast<size_t>(UINT16_MAX) * 2U + 3U;
  Status st = dev.readRegisters(cmd::REG_RESULT, &byte, wrappedSpanLength);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
}

void test_fifo_shadow_slots_do_not_require_fresh_evidence() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  seedSample(bus, cmd::REG_FIFO1_MSB, 2, 0x33333, 3, true);
  bus.registers[cmd::REG_FLAGS] = cmd::MASK_FLAG_H;
  const uint32_t readsBefore = bus.readCalls;

  Sample slot;
  Status st = dev.readSampleSlot(2, slot);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0x33333u, slot.mantissa);
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 1U, bus.readCalls);
  TEST_ASSERT_TRUE((bus.registers[cmd::REG_FLAGS] & cmd::MASK_FLAG_H) != 0U);
}

void test_int_fresh_evidence_does_not_clear_flags() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.intPin = 4;
  cfg.gpioRead = fakeGpioRead;
  cfg.gpioUser = &bus;
  cfg.intDirection = IntDirection::PIN_OUTPUT;
  cfg.intConfig = IntConfig::EVERY_CONVERSION;
  cfg.interruptPolarity = InterruptPolarity::ACTIVE_LOW;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.gpioLevel = false;
  seedSample(bus, cmd::REG_RESULT, 0, 0x45678, 4, true);
  bus.registers[cmd::REG_FLAGS] =
      static_cast<uint16_t>(cmd::MASK_FLAG_H | cmd::MASK_FLAG_L);
  const uint32_t readsBefore = bus.readCalls;

  Sample sample;
  Status st = dev.readSample(sample);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0x45678u, sample.mantissa);
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 1U, bus.readCalls);
  TEST_ASSERT_TRUE((bus.registers[cmd::REG_FLAGS] & cmd::MASK_FLAG_H) != 0U);
  TEST_ASSERT_TRUE((bus.registers[cmd::REG_FLAGS] & cmd::MASK_FLAG_L) != 0U);
}

void test_counter_fresh_evidence_does_not_clear_flags() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x12345, 4, true);
  markConversionReady(bus);
  Sample sample;
  Status st = dev.readSample(sample);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(4u, sample.counter);

  seedSample(bus, cmd::REG_RESULT, 0, 0x23456, 5, true);
  bus.nowMs += dev.getConversionTimeMs();
  bus.registers[cmd::REG_FLAGS] =
      static_cast<uint16_t>(cmd::MASK_FLAG_H | cmd::MASK_FLAG_L);

  st = dev.readSample(sample);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(5u, sample.counter);
  TEST_ASSERT_EQUAL_UINT32(0x23456u, sample.mantissa);
  TEST_ASSERT_TRUE((bus.registers[cmd::REG_FLAGS] & cmd::MASK_FLAG_H) != 0U);
  TEST_ASSERT_TRUE((bus.registers[cmd::REG_FLAGS] & cmd::MASK_FLAG_L) != 0U);
}

void test_try_read_helpers_report_not_ready_without_error() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  Sample sample;
  bool didRead = true;
  Status st = dev.tryReadSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(didRead);

  float lux = -1.0f;
  didRead = true;
  st = dev.tryReadLux(lux, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(didRead);
  TEST_ASSERT_FLOAT_WITHIN(0.0f, -1.0f, lux);

  seedSample(bus, cmd::REG_RESULT, 0, 0x23456, 5, true);
  bus.nowMs += 150;
  dev.tick(bus.nowMs);

  st = dev.tryReadSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);
  TEST_ASSERT_EQUAL_UINT32(0x23456u, sample.mantissa);

  seedSample(bus, cmd::REG_RESULT, 0, 0x23456, 6, true);
  markConversionReady(bus);
  didRead = false;
  lux = 0.0f;
  st = dev.tryReadLux(lux, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);
  TEST_ASSERT_TRUE(lux > 0.0f);
}

void test_fresh_continuous_first_sample_is_fresh() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 1, 0x13579, 2, true);
  markConversionReady(bus);

  Sample sample;
  bool didRead = false;
  Status st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);
  TEST_ASSERT_EQUAL_UINT8(2u, sample.counter);
  TEST_ASSERT_EQUAL_UINT32(0x13579u, sample.mantissa);
}

void test_fresh_repeated_same_counter_is_not_fresh() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 1, 0x24680, 3, true);
  markConversionReady(bus);

  Sample sample;
  bool didRead = false;
  Status st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);

  didRead = true;
  st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(didRead);
}

void test_fresh_counter_wrap_15_to_0_is_fresh() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x11111, 15, true);
  markConversionReady(bus);

  Sample sample;
  bool didRead = false;
  Status st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);
  TEST_ASSERT_EQUAL_UINT8(15u, sample.counter);

  seedSample(bus, cmd::REG_RESULT, 0, 0x22222, 0, true);
  markConversionReady(bus);

  didRead = false;
  st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);
  TEST_ASSERT_EQUAL_UINT8(0u, sample.counter);
  TEST_ASSERT_EQUAL_UINT32(0x22222u, sample.mantissa);
}

void test_fresh_same_lux_changed_counter_is_fresh() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 2, 0x12345, 4, true);
  markConversionReady(bus);

  Sample first;
  bool didRead = false;
  Status st = dev.tryReadFreshSample(first, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);

  seedSample(bus, cmd::REG_RESULT, 2, 0x12345, 5, true);
  markConversionReady(bus);

  Sample second;
  didRead = false;
  st = dev.tryReadFreshSample(second, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);
  TEST_ASSERT_EQUAL_UINT8(5u, second.counter);
  TEST_ASSERT_EQUAL_UINT32(first.adcCodes, second.adcCodes);
}

void test_fresh_one_shot_elapsed_without_flag_is_not_ready() {
  FakeBus bus;
  bus.autoCompleteConversions = false;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.conversionTime = ConversionTime::US_600;
  cfg.quickWake = true;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x34567, 6, true);
  Status st = dev.startConversion(Mode::ONE_SHOT);
  TEST_ASSERT_TRUE(st.inProgress());

  bus.nowMs += dev.getOneShotBudgetMs(Mode::ONE_SHOT);
  forceHardwareMode(bus, Mode::POWER_DOWN);
  bus.registers[cmd::REG_FLAGS] =
      static_cast<uint16_t>(bus.registers[cmd::REG_FLAGS] &
                            static_cast<uint16_t>(~cmd::MASK_CONVERSION_READY_FLAG));

  Sample sample;
  bool didRead = true;
  st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(didRead);
}

void test_fresh_one_shot_flag_set_returns_sample() {
  FakeBus bus;
  bus.autoCompleteConversions = false;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.conversionTime = ConversionTime::US_600;
  cfg.quickWake = true;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x45678, 7, true);
  Status st = dev.startConversion(Mode::ONE_SHOT);
  TEST_ASSERT_TRUE(st.inProgress());

  bus.nowMs += dev.getOneShotBudgetMs(Mode::ONE_SHOT);
  forceHardwareMode(bus, Mode::POWER_DOWN);
  markConversionReady(bus);

  Sample sample;
  bool didRead = false;
  st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);
  TEST_ASSERT_EQUAL_UINT8(7u, sample.counter);
  TEST_ASSERT_EQUAL_UINT32(0x45678u, sample.mantissa);
}

void test_fresh_forced_auto_range_is_not_read_early() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.conversionTime = ConversionTime::US_600;
  cfg.quickWake = true;
  cfg.cooperativeYield = fakeAdvancingYield;
  cfg.timeUser = &bus;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  const uint32_t nominalBudgetMs = dev.getOneShotBudgetMs(Mode::ONE_SHOT);
  const uint32_t forcedAutoBudgetMs = dev.getOneShotBudgetMs(Mode::ONE_SHOT_FORCED_AUTO);
  TEST_ASSERT_TRUE(forcedAutoBudgetMs > nominalBudgetMs);

  seedSample(bus, cmd::REG_RESULT, 0, 0x56789, 8, true);

  Sample sample;
  Status st = dev.readFreshBlocking(sample, Mode::ONE_SHOT_FORCED_AUTO, nominalBudgetMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
}

void test_fresh_read_advances_readiness() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 1, 0x6789A, 9, true);
  markConversionReady(bus);

  Sample sample;
  bool didRead = false;
  Status st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);

  didRead = true;
  st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(didRead);
}

void test_fresh_one_shot_to_continuous_clears_stale_readiness() {
  FakeBus bus;
  bus.autoCompleteConversions = false;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.conversionTime = ConversionTime::US_600;
  cfg.quickWake = true;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x789AB, 10, true);
  Status st = dev.startConversion(Mode::ONE_SHOT);
  TEST_ASSERT_TRUE(st.inProgress());
  bus.nowMs += dev.getOneShotBudgetMs(Mode::ONE_SHOT);
  forceHardwareMode(bus, Mode::POWER_DOWN);
  markConversionReady(bus);

  Flags flags;
  TEST_ASSERT_TRUE(dev.readFlags(flags).ok());
  TEST_ASSERT_TRUE(flags.conversionReady);

  TEST_ASSERT_TRUE(dev.setMode(Mode::CONTINUOUS).ok());

  Sample sample;
  bool didRead = true;
  st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(didRead);

  seedSample(bus, cmd::REG_RESULT, 0, 0x789AC, 11, true);
  markConversionReady(bus);
  didRead = false;
  st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(didRead);
  TEST_ASSERT_EQUAL_UINT8(11u, sample.counter);
}

void test_fresh_blocking_requires_now_ms() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.nowMs = nullptr;
  cfg.timeUser = nullptr;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  const uint32_t writesBefore = bus.writeCalls;
  Sample sample;
  Status st = dev.readFreshBlocking(sample, Mode::ONE_SHOT, 5);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
}

void test_fresh_crc_error_on_fresh_sample() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  seedSample(bus, cmd::REG_RESULT, 0, 0x23456, 12, false);
  markConversionReady(bus);

  Sample sample;
  bool didRead = false;
  Status st = dev.tryReadFreshSample(sample, didRead);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CRC_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(didRead);
  TEST_ASSERT_FALSE(sample.crcValid);
  TEST_ASSERT_EQUAL_UINT8(12u, sample.counter);
}

void test_try_read_propagates_readiness_i2c_error() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.startConversion();
  TEST_ASSERT_TRUE(st.inProgress());
  bus.nowMs += dev.getOneShotBudgetMs(Mode::ONE_SHOT);
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced readiness timeout", -31);

  Sample sample;
  bool didRead = true;
  st = dev.tryReadSample(sample, didRead);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(didRead);
}

void test_read_blocking_propagates_readiness_i2c_error() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.conversionTime = ConversionTime::US_600;
  cfg.quickWake = true;
  cfg.cooperativeYield = fakeAdvancingYield;
  cfg.timeUser = &bus;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced readiness timeout", -32);

  Sample sample;
  Status st = dev.readBlocking(sample, Mode::ONE_SHOT, 50);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
}

void test_offline_blocks_normal_operation_without_bus_io() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced offline timeout", -33);
  Flags flags;
  Status st = dev.readFlags(flags);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  bus.readStatus = Status::Ok();
  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;

  st = dev.readFlags(flags);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OFFLINE),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
}

void test_failed_recover_from_offline_reasserts_latch_after_apply_failure() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 3;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced offline timeout", -34);
  Flags flags;
  for (uint8_t i = 0; i < cfg.offlineThreshold; ++i) {
    Status st = dev.readFlags(flags);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                            static_cast<uint8_t>(st.code));
  }
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_TRUE(dev.consecutiveFailures() >= cfg.offlineThreshold);

  bus.readStatus = Status::Ok();
  bus.failWriteCall = bus.writeCalls + 1U;
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_TRUE(dev.consecutiveFailures() >= cfg.offlineThreshold);
  TEST_ASSERT_FALSE(dev._allowOfflineI2c);

  const uint32_t readsAfterRecover = bus.readCalls;
  const uint32_t writesAfterRecover = bus.writeCalls;
  st = dev.readFlags(flags);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OFFLINE),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(readsAfterRecover, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesAfterRecover, bus.writeCalls);
}

void test_apply_config_fail_positions_track_partial_hardware_dirty() {
  assertApplyConfigFailureDirtyState(1, false);
  assertApplyConfigFailureDirtyState(2, true);
  assertApplyConfigFailureDirtyState(3, true);
  assertApplyConfigFailureDirtyState(4, true);
}

void test_threshold_high_failure_marks_dirty_and_preserves_status() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  assertHardwareConfigDirty(dev, false);

  const Threshold low{0, 0x0010};
  const Threshold high{0, 0x0020};
  Status st = makeThresholdPairDirty(dev, bus, low, high);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-22, st.detail);
  TEST_ASSERT_EQUAL_UINT16(packThresholdForTest(low), bus.registers[cmd::REG_THRESHOLD_L]);
  TEST_ASSERT_EQUAL_UINT16(cmd::THRESHOLD_H_RESET, bus.registers[cmd::REG_THRESHOLD_H]);
  assertHardwareConfigDirty(dev, true);
  assertHardwareConfigDirtyError(dev, st);
}

void test_dirty_state_survives_unrelated_read_and_preserves_error() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const Threshold low{0, 0x0030};
  const Threshold high{0, 0x0040};
  Status dirtySt = makeThresholdPairDirty(dev, bus, low, high);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(dirtySt.code));
  assertHardwareConfigDirty(dev, true);

  uint16_t deviceId = 0;
  TEST_ASSERT_TRUE(dev.readDeviceId(deviceId).ok());
  TEST_ASSERT_EQUAL_UINT16(cmd::DEVICE_ID_RESET, deviceId);
  assertHardwareConfigDirty(dev, true);
  assertHardwareConfigDirtyError(dev, dirtySt);
}

void test_successful_recover_clears_hardware_config_dirty_state() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const Threshold low{0, 0x0050};
  const Threshold high{0, 0x0060};
  Status dirtySt = makeThresholdPairDirty(dev, bus, low, high);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(dirtySt.code));
  assertHardwareConfigDirty(dev, true);

  Status st = dev.recover();
  TEST_ASSERT_TRUE(st.ok());
  assertHardwareConfigDirty(dev, false);
  TEST_ASSERT_TRUE(dev.hardwareConfigDirtyError().ok());
}

void test_failed_recover_leaves_hardware_config_dirty_state_set() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const Threshold low{0, 0x0070};
  const Threshold high{0, 0x0080};
  Status dirtySt = makeThresholdPairDirty(dev, bus, low, high);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(dirtySt.code));
  assertHardwareConfigDirty(dev, true);

  bus.failWriteCall = bus.writeCalls + 2U;
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-22, st.detail);
  assertHardwareConfigDirty(dev, true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(dev.hardwareConfigDirtyError().code));
}

void test_reset_and_reapply_failure_after_reset_marks_dirty() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  assertHardwareConfigDirty(dev, false);

  bus.failWriteCall = bus.writeCalls + 2U;
  Status st = dev.resetAndReapply();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  assertHardwareConfigDirty(dev, true);
  assertHardwareConfigDirtyError(dev, st);
}

void test_successful_reset_and_reapply_clears_hardware_config_dirty_state() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const Threshold low{0, 0x0090};
  const Threshold high{0, 0x00A0};
  Status dirtySt = makeThresholdPairDirty(dev, bus, low, high);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(dirtySt.code));
  assertHardwareConfigDirty(dev, true);

  Status st = dev.resetAndReapply();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.isInitialized());
  assertHardwareConfigDirty(dev, false);
  TEST_ASSERT_TRUE(dev.hardwareConfigDirtyError().ok());
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
  TEST_ASSERT_TRUE(std::isnan(
      dev.getRangeFullScaleLux(static_cast<Range>(9))));
  TEST_ASSERT_TRUE(std::isnan(
      dev.getRangeResolutionLux(static_cast<Range>(9), ConversionTime::MS_100)));
  TEST_ASSERT_TRUE(std::isnan(
      dev.getRangeResolutionLux(Range::RANGE_0,
                                static_cast<ConversionTime>(12))));
  Sample sample{};
  sample.exponent = 3U;
  TEST_ASSERT_TRUE(dev.getSampleFullScaleLux(sample) > 0.0f);
  TEST_ASSERT_TRUE(dev.getSampleResolutionLux(sample) > 0.0f);
  sample.exponent = 9U;
  TEST_ASSERT_TRUE(std::isnan(dev.getSampleFullScaleLux(sample)));
  TEST_ASSERT_TRUE(std::isnan(dev.getSampleResolutionLux(sample)));
  TEST_ASSERT_EQUAL_UINT32(0U, dev.getOneShotBudgetUs(Mode::POWER_DOWN));
  TEST_ASSERT_EQUAL_UINT32(0U, dev.getOneShotBudgetUs(Mode::CONTINUOUS));
  TEST_ASSERT_EQUAL_UINT32(0U, dev.getOneShotBudgetMs(static_cast<Mode>(0xFF)));

  TEST_ASSERT_TRUE(dev.setPackageVariant(PackageVariant::PICOSTAR).ok());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 328.0f, dev.getRangeFullScaleLux(Range::RANGE_0));
}

void test_all_conversion_time_vectors_and_invalid_values() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const ConversionTime times[] = {
    ConversionTime::US_600,
    ConversionTime::MS_1,
    ConversionTime::MS_1_8,
    ConversionTime::MS_3_4,
    ConversionTime::MS_6_5,
    ConversionTime::MS_12_7,
    ConversionTime::MS_25,
    ConversionTime::MS_50,
    ConversionTime::MS_100,
    ConversionTime::MS_200,
    ConversionTime::MS_400,
    ConversionTime::MS_800,
  };
  const uint32_t expectedUs[] = {
    600U, 1000U, 1800U, 3400U, 6500U, 12700U,
    25000U, 50000U, 100000U, 200000U, 400000U, 800000U,
  };
  const uint32_t expectedMs[] = {
    1U, 1U, 2U, 4U, 7U, 13U, 25U, 50U, 100U, 200U, 400U, 800U,
  };
  const uint8_t expectedBits[] = {
    9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U,
  };

  for (size_t i = 0; i < 12; ++i) {
    TEST_ASSERT_TRUE(dev.setConversionTime(times[i]).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(times[i]),
                            static_cast<uint8_t>(dev.getConversionTime()));
    TEST_ASSERT_EQUAL_UINT32(expectedUs[i], dev.getConversionTimeUs());
    TEST_ASSERT_EQUAL_UINT32(expectedMs[i], dev.getConversionTimeMs());
    TEST_ASSERT_EQUAL_UINT8(expectedBits[i], dev.getEffectiveBits());
    TEST_ASSERT_EQUAL_UINT8(expectedBits[i], dev.getEffectiveBits(times[i]));
  }

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConversionTime::MS_800),
                          static_cast<uint8_t>(dev.getConversionTime()));
  Status st = dev.setConversionTime(static_cast<ConversionTime>(12));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.setConversionTime(static_cast<ConversionTime>(255));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConversionTime::MS_800),
                          static_cast<uint8_t>(dev.getConversionTime()));
}

void test_range_vectors_and_invalid_values() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const Range validRanges[] = {
    Range::RANGE_0,
    Range::RANGE_1,
    Range::RANGE_2,
    Range::RANGE_3,
    Range::RANGE_4,
    Range::RANGE_5,
    Range::RANGE_6,
    Range::RANGE_7,
    Range::RANGE_8,
    Range::AUTO,
  };

  for (Range range : validRanges) {
    TEST_ASSERT_TRUE(dev.setRange(range).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(range), static_cast<uint8_t>(dev.getRange()));
    TEST_ASSERT_TRUE(dev.getCurrentFullScaleLux() > 0.0f);
    TEST_ASSERT_TRUE(dev.getCurrentResolutionLux() > 0.0f);
  }

  const uint8_t invalidRanges[] = {9U, 10U, 11U, 13U, 255U};
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::AUTO), static_cast<uint8_t>(dev.getRange()));
  for (uint8_t value : invalidRanges) {
    Status st = dev.setRange(static_cast<Range>(value));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::AUTO), static_cast<uint8_t>(dev.getRange()));
  }

  Status st = dev.configureMeasurement(static_cast<Range>(9),
                                       ConversionTime::MS_100,
                                       Mode::CONTINUOUS);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.configureMeasurement(Range::RANGE_2,
                                static_cast<ConversionTime>(12),
                                Mode::CONTINUOUS);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::AUTO), static_cast<uint8_t>(dev.getRange()));
}

void test_configuration_and_interrupt_convenience_helpers() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.configureMeasurement(Range::RANGE_2,
                                       ConversionTime::MS_25,
                                       Mode::CONTINUOUS,
                                       true);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.getQuickWake());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::RANGE_2),
                          static_cast<uint8_t>(dev.getRange()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConversionTime::MS_25),
                          static_cast<uint8_t>(dev.getConversionTime()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::CONTINUOUS),
                          static_cast<uint8_t>(dev.getMode()));

  uint16_t cfgReg = 0;
  TEST_ASSERT_TRUE(dev.readConfiguration(cfgReg).ok());
  TEST_ASSERT_NOT_EQUAL(0u, cfgReg & cmd::MASK_QWAKE);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(Range::RANGE_2),
                           static_cast<uint16_t>((cfgReg & cmd::MASK_RANGE) >> cmd::BIT_RANGE));
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(ConversionTime::MS_25),
                           static_cast<uint16_t>((cfgReg & cmd::MASK_CONVERSION_TIME) >>
                                                 cmd::BIT_CONVERSION_TIME));
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(Mode::CONTINUOUS),
                           static_cast<uint16_t>((cfgReg & cmd::MASK_MODE) >> cmd::BIT_MODE));

  TEST_ASSERT_TRUE(dev.enableConversionReadyInterrupt().ok());
  uint16_t intCfgReg = 0;
  TEST_ASSERT_TRUE(dev.readIntConfiguration(intCfgReg).ok());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(IntDirection::PIN_OUTPUT),
                           static_cast<uint16_t>((intCfgReg & cmd::MASK_INT_DIR) >>
                                                 cmd::BIT_INT_DIR));
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(IntConfig::EVERY_CONVERSION),
                           static_cast<uint16_t>((intCfgReg & cmd::MASK_INT_CFG) >>
                                                 cmd::BIT_INT_CFG));

  TEST_ASSERT_TRUE(dev.enableFifoFullInterrupt().ok());
  TEST_ASSERT_TRUE(dev.readIntConfiguration(intCfgReg).ok());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(IntConfig::FIFO_FULL),
                           static_cast<uint16_t>((intCfgReg & cmd::MASK_INT_CFG) >>
                                                 cmd::BIT_INT_CFG));

  Threshold low{1, 0x0020};
  Threshold high{2, 0x0100};
  TEST_ASSERT_TRUE(dev.enableThresholdInterrupt(low, high).ok());
  TEST_ASSERT_TRUE(dev.readIntConfiguration(intCfgReg).ok());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(IntConfig::THRESHOLD),
                           static_cast<uint16_t>((intCfgReg & cmd::MASK_INT_CFG) >>
                                                 cmd::BIT_INT_CFG));

  Threshold lowRead;
  Threshold highRead;
  TEST_ASSERT_TRUE(dev.getThresholds(lowRead, highRead).ok());
  TEST_ASSERT_EQUAL_UINT8(low.exponent, lowRead.exponent);
  TEST_ASSERT_EQUAL_UINT16(low.result, lowRead.result);
  TEST_ASSERT_EQUAL_UINT8(high.exponent, highRead.exponent);
  TEST_ASSERT_EQUAL_UINT16(high.result, highRead.result);

  st = dev.enableThresholdInterruptLux(5.0f, 50.0f);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.getThresholds(lowRead, highRead).ok());
  TEST_ASSERT_TRUE(dev.thresholdToLux(highRead) > dev.thresholdToLux(lowRead));

  st = dev.enableThresholdInterruptLux(50.0f, 5.0f);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  TEST_ASSERT_TRUE(dev.restoreDefaultThresholds().ok());
  TEST_ASSERT_TRUE(dev.getThresholds(lowRead, highRead).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>((cmd::THRESHOLD_L_RESET & cmd::MASK_THRESHOLD_EXPONENT) >>
                                               cmd::BIT_THRESHOLD_EXPONENT),
                          lowRead.exponent);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(cmd::THRESHOLD_L_RESET & cmd::MASK_THRESHOLD_RESULT),
                           lowRead.result);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>((cmd::THRESHOLD_H_RESET & cmd::MASK_THRESHOLD_EXPONENT) >>
                                               cmd::BIT_THRESHOLD_EXPONENT),
                          highRead.exponent);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(cmd::THRESHOLD_H_RESET & cmd::MASK_THRESHOLD_RESULT),
                           highRead.result);
}

void test_picostar_rejects_int_output_presets_without_bus_io() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.packageVariant = PackageVariant::PICOSTAR;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  const uint32_t writesBefore = bus.writeCalls;

  Status st = dev.enableConversionReadyInterrupt();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  st = dev.enableFifoFullInterrupt();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  st = dev.enableThresholdInterrupt(Threshold{0, 1}, Threshold{0, 2});
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
}

void test_cached_configuration_rolls_back_after_i2c_failure() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write error", -12);
  Status st = dev.setRange(Range::RANGE_3);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::AUTO), static_cast<uint8_t>(dev.getRange()));

  bus.writeStatus = Status::Ok();
  bus.failWriteCall = bus.writeCalls + 2U;
  Threshold low{1, 0x0010};
  Threshold high{1, 0x0020};
  st = dev.setThresholds(low, high);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR), static_cast<uint8_t>(st.code));

  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(dev.driverState()),
                          static_cast<uint8_t>(snap.state));
  TEST_ASSERT_FALSE(snap.hasSample);
  TEST_ASSERT_EQUAL_UINT8(0u, snap.lowThreshold.exponent);
  TEST_ASSERT_EQUAL_UINT16(0u, snap.lowThreshold.result);
  TEST_ASSERT_EQUAL_UINT8(0x0Bu, snap.highThreshold.exponent);
  TEST_ASSERT_EQUAL_UINT16(0x0FFFu, snap.highThreshold.result);

  bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write error", -13);
  st = dev.configureMeasurement(Range::RANGE_2, ConversionTime::MS_25, Mode::CONTINUOUS, true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Range::AUTO), static_cast<uint8_t>(dev.getRange()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConversionTime::MS_100),
                          static_cast<uint8_t>(dev.getConversionTime()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::POWER_DOWN),
                          static_cast<uint8_t>(dev.getMode()));
  TEST_ASSERT_FALSE(dev.getQuickWake());
}

void test_successful_raw_config_write_marks_hardware_config_dirty() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  assertHardwareConfigDirty(dev, false);

  Status st = dev.writeRegister16(cmd::REG_RESULT, 0x1234);
  TEST_ASSERT_TRUE(st.ok());
  assertHardwareConfigDirty(dev, false);

  st = dev.writeRegister16(cmd::REG_CONFIGURATION, cmd::CONFIGURATION_RESET);
  TEST_ASSERT_TRUE(st.ok());
  assertHardwareConfigDirty(dev, true);
  TEST_ASSERT_TRUE(dev.hardwareConfigDirtyError().ok());

  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.hardwareConfigDirty);
  TEST_ASSERT_TRUE(snap.hardwareConfigDirtyError.ok());

  st = dev.recover();
  TEST_ASSERT_TRUE(st.ok());
  assertHardwareConfigDirty(dev, false);
}

void test_raw_register_access_rejects_invalid_bounds_without_bus_io() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  uint16_t value = 0;
  Status st = dev.readRegister16(0x0D, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev.writeRegister16(0x0D, 0x1234);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  uint8_t raw[4] = {};
  st = dev.readRegisters(cmd::REG_FLAGS, raw, sizeof(raw));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
}

void test_raw_register_access_before_begin_is_guarded_without_bus_io() {
  FakeBus bus;
  OPT4001::OPT4001 dev;

  uint16_t value = 0xBEEF;
  Status st = dev.readRegister16(cmd::REG_DEVICE_ID, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_HEX16(0xBEEF, value);

  st = dev.writeRegister16(cmd::REG_CONFIGURATION, cmd::CONFIGURATION_RESET);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));

  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
}

void test_raw_register_access_after_failed_begin_is_guarded_without_bus_io() {
  FakeBus bus;
  OPT4001::OPT4001 dev;

  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced probe timeout", -40);
  Status st = dev.begin(makeConfig(bus));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));

  bus.readStatus = Status::Ok();
  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;

  uint16_t value = 0xCAFE;
  st = dev.readRegister16(cmd::REG_DEVICE_ID, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_HEX16(0xCAFE, value);

  st = dev.writeRegister16(cmd::REG_CONFIGURATION, cmd::CONFIGURATION_RESET);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));

  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
}

void test_raw_register_access_after_end_is_guarded_without_bus_io() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  dev.end();
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;

  uint16_t value = 0x1234;
  Status st = dev.readRegister16(cmd::REG_DEVICE_ID, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_HEX16(0x1234, value);

  st = dev.writeRegister16(cmd::REG_CONFIGURATION, cmd::CONFIGURATION_RESET);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));

  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
}

void test_raw_register_access_offline_matches_normal_operation_without_bus_io() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced offline timeout", -41);
  Flags flags;
  Status st = dev.readFlags(flags);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  bus.readStatus = Status::Ok();
  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;

  st = dev.readFlags(flags);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OFFLINE), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);

  uint16_t value = 0;
  st = dev.readRegister16(cmd::REG_DEVICE_ID, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OFFLINE), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);

  st = dev.writeRegister16(cmd::REG_CONFIGURATION, cmd::CONFIGURATION_RESET);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OFFLINE), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);

  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
}

void test_probe_without_begin_uses_raw_transport_when_cached_config_present() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  dev._config = makeConfig(bus);

  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));

  Status st = dev.probe();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1U, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(0U, dev.totalSuccess());
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

void test_lux_to_threshold_rejects_non_finite_inputs() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Threshold threshold;
  Status st = dev.luxToThreshold(std::numeric_limits<float>::quiet_NaN(), threshold);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev.luxToThreshold(std::numeric_limits<float>::infinity(), threshold);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev.luxToThreshold(-0.001f, threshold);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  const Threshold maxThreshold{15, 0x0FFF};
  st = dev.luxToThreshold(dev.thresholdToLux(maxThreshold) + 1000.0f, threshold);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev.setThresholdsLux(1.0f, std::numeric_limits<float>::infinity());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));
}

void test_lux_to_threshold_rounding_and_out_of_range_policy() {
  FakeBus bus;
  OPT4001::OPT4001 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Threshold threshold;
  Status st = dev.luxToThreshold(10.0f, threshold);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, threshold.exponent);
  TEST_ASSERT_EQUAL_HEX16(0x0059, threshold.result);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.968f, dev.thresholdToLux(threshold));

  st = dev.luxToThreshold(100.0f, threshold);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, threshold.exponent);
  TEST_ASSERT_EQUAL_HEX16(0x037D, threshold.result);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.016f, dev.thresholdToLux(threshold));

  st = dev.luxToThreshold(0.060f, threshold);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, threshold.exponent);
  TEST_ASSERT_EQUAL_HEX16(0x0001, threshold.result);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.112f, dev.thresholdToLux(threshold));

  st = dev.luxToThreshold(0.05589f, threshold);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, threshold.exponent);
  TEST_ASSERT_EQUAL_HEX16(0x0000, threshold.result);

  // At an exponent boundary, the saturated finer encoding can be closer than
  // the first non-saturated coarser encoding.
  st = dev.luxToThreshold(458.675f, threshold);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, threshold.exponent);
  TEST_ASSERT_EQUAL_HEX16(0x0FFF, threshold.result);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 458.64f, dev.thresholdToLux(threshold));

  TEST_ASSERT_TRUE(dev.setPackageVariant(PackageVariant::PICOSTAR).ok());
  st = dev.luxToThreshold(10.0f, threshold);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, threshold.exponent);
  TEST_ASSERT_EQUAL_HEX16(0x007D, threshold.result);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, dev.thresholdToLux(threshold));

  st = dev.luxToThreshold(100.0f, threshold);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, threshold.exponent);
  TEST_ASSERT_EQUAL_HEX16(0x04E2, threshold.result);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, dev.thresholdToLux(threshold));

  TEST_ASSERT_TRUE(dev.setPackageVariant(PackageVariant::SOT_5X3).ok());
  const Threshold maxThreshold{15, cmd::THRESHOLD_RESULT_MAX};
  const float maxLux = dev.thresholdToLux(maxThreshold);
  st = dev.luxToThreshold(maxLux - 16.0f, threshold);
  TEST_ASSERT_TRUE(st.ok());
  uint64_t adcCodes = 0;
  TEST_ASSERT_TRUE(dev.thresholdToAdcCodes(threshold, adcCodes).ok());
  TEST_ASSERT_TRUE(adcCodes <= 34351349760ULL);

  st = dev.luxToThreshold(maxLux + 16.0f, threshold);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = dev.luxToThreshold(-0.001f, threshold);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
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
  RUN_TEST(test_error_names_are_complete_and_stable);
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_get_last_sample_before_any_read);
  RUN_TEST(test_begin_rejects_missing_callbacks);
  RUN_TEST(test_begin_rejects_one_shot_startup_mode);
  RUN_TEST(test_begin_rejects_invalid_package_address_combo);
  RUN_TEST(test_picostar_rejects_impossible_int_hook_configuration);
  RUN_TEST(test_runtime_package_switch_rejects_picostar_with_int_hook);
  RUN_TEST(test_package_address_matrix);
  RUN_TEST(test_invalid_begin_after_success_resets_default_runtime);
  RUN_TEST(test_begin_normalizes_offline_threshold_on_stored_copy);
  RUN_TEST(test_get_settings_is_cache_only_without_bus_io);
  RUN_TEST(test_begin_success_sets_ready_without_health_counts);
  RUN_TEST(test_update_health_ignores_in_progress);
  RUN_TEST(test_probe_accepts_valid_full_device_id);
  RUN_TEST(test_probe_rejects_matching_didh_with_nonzero_high_id_bits);
  RUN_TEST(test_begin_rejects_matching_didh_with_nonzero_high_id_bits);
  RUN_TEST(test_recover_rejects_matching_didh_with_nonzero_high_id_bits);
  RUN_TEST(test_probe_preserves_transport_errors_and_detail);
  RUN_TEST(test_probe_success_and_failure_do_not_update_health_or_state);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_probe_id_mismatch_does_not_update_health);
  RUN_TEST(test_recover_failure_updates_health);
  RUN_TEST(test_recover_id_mismatch_updates_health);
  RUN_TEST(test_recover_success_returns_ready);
  RUN_TEST(test_start_conversion_wraparound_reaches_ready);
  RUN_TEST(test_read_blocking_times_out_with_stalled_clock);
  RUN_TEST(test_blocking_reads_accept_full_uint32_timeout_range);
  RUN_TEST(test_read_sample_decodes_lux_and_crc);
  RUN_TEST(test_raw_lux_vectors_use_64_bit_intermediates);
  RUN_TEST(test_raw_lux_rejects_invalid_result_fields_without_shift_ub);
  RUN_TEST(test_decode_rejects_invalid_result_exponent_without_cache_update);
  RUN_TEST(test_crc_vectors_use_datasheet_oracle);
  RUN_TEST(test_crc_mismatch_preserves_received_crc_and_decode_fields);
  RUN_TEST(test_zero_timestamp_sample_age_uses_valid_flag);
  RUN_TEST(test_crc_mismatch_returns_error_when_enabled);
  RUN_TEST(test_crc_mismatch_allowed_when_verification_disabled);
  RUN_TEST(test_lux_helpers_preserve_outputs_on_crc_warning);
  RUN_TEST(test_read_burst_decodes_fifo);
  RUN_TEST(test_read_burst_nonburst_path_decodes_fifo);
  RUN_TEST(test_read_burst_all_four_slots_valid_populates_fields_and_counter_order);
  RUN_TEST(test_read_burst_newest_crc_error_populates_all_slots);
  RUN_TEST(test_read_burst_middle_fifo_crc_error_populates_all_slots);
  RUN_TEST(test_read_burst_last_fifo_crc_error_populates_all_slots);
  RUN_TEST(test_read_burst_multiple_crc_errors_populates_all_slots);
  RUN_TEST(test_read_burst_nonburst_path_aggregates_crc_and_populates_all_slots);
  RUN_TEST(test_poll_read_burst_status_and_burst_share_budget_two);
  RUN_TEST(test_poll_read_sample_uses_burst_primitive_without_publishing_burst);
  RUN_TEST(test_poll_read_burst_budget_one_splits_status_and_burst);
  RUN_TEST(test_poll_zero_budget_does_not_touch_i2c);
  RUN_TEST(test_poll_read_burst_delay_gate_consumes_no_instruction);
  RUN_TEST(test_poll_busy_blocks_tick_and_synchronous_i2c);
  RUN_TEST(test_poll_read_burst_fifo_full_gate_uses_four_sample_cadence);
  RUN_TEST(test_poll_read_burst_one_shot_forced_auto_gate_uses_full_budget);
  RUN_TEST(test_poll_config_apply_honors_instruction_budget);
  RUN_TEST(test_poll_config_apply_failure_stops_and_reports_status);
  RUN_TEST(test_poll_reset_and_reapply_failure_after_reset_stops_dirty_uninit);
  RUN_TEST(test_set_thresholds_lux_updates_threshold_registers);
  RUN_TEST(test_threshold_lux_helpers_roundtrip);
  RUN_TEST(test_threshold_adc_vectors_use_64_bit_and_legacy_saturates);
  RUN_TEST(test_threshold_interrupt_ordering_uses_64_bit_codes);
  RUN_TEST(test_read_flags_parses_and_clears_ready_flag);
  RUN_TEST(test_clear_conversion_ready_flag_preserves_window_flags);
  RUN_TEST(test_clear_flags_uses_clear_on_read_semantics);
  RUN_TEST(test_generic_raw_flags_access_synchronizes_readiness_without_dirtying_config);
  RUN_TEST(test_read_int_pin_asserted_uses_configured_polarity);
  RUN_TEST(test_write_int_configuration_rejects_bad_fixed_pattern);
  RUN_TEST(test_read_device_id_returns_raw_register_value);
  RUN_TEST(test_set_verify_crc_updates_cached_setting);
  RUN_TEST(test_decoded_register_helpers);
  RUN_TEST(test_read_register_block_and_sample_slot_helpers);
  RUN_TEST(test_burst_enabled_single_sample_uses_one_coherent_result_transaction);
  RUN_TEST(test_nonburst_register_block_uses_bounded_per_register_reads);
  RUN_TEST(test_register_block_rejects_size_t_span_overflow_before_transport);
  RUN_TEST(test_fifo_shadow_slots_do_not_require_fresh_evidence);
  RUN_TEST(test_int_fresh_evidence_does_not_clear_flags);
  RUN_TEST(test_counter_fresh_evidence_does_not_clear_flags);
  RUN_TEST(test_try_read_helpers_report_not_ready_without_error);
  RUN_TEST(test_fresh_continuous_first_sample_is_fresh);
  RUN_TEST(test_fresh_repeated_same_counter_is_not_fresh);
  RUN_TEST(test_fresh_counter_wrap_15_to_0_is_fresh);
  RUN_TEST(test_fresh_same_lux_changed_counter_is_fresh);
  RUN_TEST(test_fresh_one_shot_elapsed_without_flag_is_not_ready);
  RUN_TEST(test_fresh_one_shot_flag_set_returns_sample);
  RUN_TEST(test_fresh_forced_auto_range_is_not_read_early);
  RUN_TEST(test_fresh_read_advances_readiness);
  RUN_TEST(test_fresh_one_shot_to_continuous_clears_stale_readiness);
  RUN_TEST(test_fresh_blocking_requires_now_ms);
  RUN_TEST(test_fresh_crc_error_on_fresh_sample);
  RUN_TEST(test_try_read_propagates_readiness_i2c_error);
  RUN_TEST(test_read_blocking_propagates_readiness_i2c_error);
  RUN_TEST(test_offline_blocks_normal_operation_without_bus_io);
  RUN_TEST(test_failed_recover_from_offline_reasserts_latch_after_apply_failure);
  RUN_TEST(test_apply_config_fail_positions_track_partial_hardware_dirty);
  RUN_TEST(test_threshold_high_failure_marks_dirty_and_preserves_status);
  RUN_TEST(test_dirty_state_survives_unrelated_read_and_preserves_error);
  RUN_TEST(test_successful_recover_clears_hardware_config_dirty_state);
  RUN_TEST(test_failed_recover_leaves_hardware_config_dirty_state_set);
  RUN_TEST(test_reset_and_reapply_failure_after_reset_marks_dirty);
  RUN_TEST(test_successful_reset_and_reapply_clears_hardware_config_dirty_state);
  RUN_TEST(test_scale_and_counter_helpers);
  RUN_TEST(test_all_conversion_time_vectors_and_invalid_values);
  RUN_TEST(test_range_vectors_and_invalid_values);
  RUN_TEST(test_configuration_and_interrupt_convenience_helpers);
  RUN_TEST(test_picostar_rejects_int_output_presets_without_bus_io);
  RUN_TEST(test_cached_configuration_rolls_back_after_i2c_failure);
  RUN_TEST(test_successful_raw_config_write_marks_hardware_config_dirty);
  RUN_TEST(test_raw_register_access_rejects_invalid_bounds_without_bus_io);
  RUN_TEST(test_raw_register_access_before_begin_is_guarded_without_bus_io);
  RUN_TEST(test_raw_register_access_after_failed_begin_is_guarded_without_bus_io);
  RUN_TEST(test_raw_register_access_after_end_is_guarded_without_bus_io);
  RUN_TEST(test_raw_register_access_offline_matches_normal_operation_without_bus_io);
  RUN_TEST(test_probe_without_begin_uses_raw_transport_when_cached_config_present);
  RUN_TEST(test_lux_to_threshold_rejects_non_finite_inputs);
  RUN_TEST(test_lux_to_threshold_rounding_and_out_of_range_policy);
  RUN_TEST(test_soft_reset_moves_driver_to_uninit);
  RUN_TEST(test_reset_and_reapply_restores_ready_and_config);
  RUN_TEST(test_raw_transport_rejects_invalid_buffers);
  return UNITY_END();
}
