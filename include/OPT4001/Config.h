/// @file Config.h
/// @brief Configuration structure for OPT4001 driver.
#pragma once

#include <cstddef>
#include <cstdint>

#include "OPT4001/Status.h"

namespace OPT4001 {

/// I2C write callback signature.
using I2cWriteFn = Status (*)(uint8_t addr, const uint8_t* data, size_t len,
                              uint32_t timeoutMs, void* user);

/// I2C write-then-read callback signature.
using I2cWriteReadFn = Status (*)(uint8_t addr, const uint8_t* txData, size_t txLen,
                                  uint8_t* rxData, size_t rxLen, uint32_t timeoutMs,
                                  void* user);

/// Optional GPIO read callback for the device INT pin.
using GpioReadFn = bool (*)(int pin, void* user);

/// Millisecond timestamp callback.
using NowMsFn = uint32_t (*)(void* user);

/// Cooperative yield callback.
using YieldFn = void (*)(void* user);

/// Package variant. This controls lux scaling and address validation.
enum class PackageVariant : uint8_t {
  PICOSTAR = 0,  ///< 4-pin PicoStar package, fixed address 0x45.
  SOT_5X3  = 1   ///< 8-pin SOT-5X3 package, address-selectable, exposes INT.
};

/// Full-scale range selection.
enum class Range : uint8_t {
  RANGE_0 = 0,
  RANGE_1 = 1,
  RANGE_2 = 2,
  RANGE_3 = 3,
  RANGE_4 = 4,
  RANGE_5 = 5,
  RANGE_6 = 6,
  RANGE_7 = 7,
  RANGE_8 = 8,
  AUTO    = 12  ///< Automatic full-scale range selection.
};

/// Conversion time selection.
enum class ConversionTime : uint8_t {
  US_600  = 0,
  MS_1    = 1,
  MS_1_8  = 2,
  MS_3_4  = 3,
  MS_6_5  = 4,
  MS_12_7 = 5,
  MS_25   = 6,
  MS_50   = 7,
  MS_100  = 8,
  MS_200  = 9,
  MS_400  = 10,
  MS_800  = 11
};

/// Operating mode field from register 0x0A.
enum class Mode : uint8_t {
  POWER_DOWN           = 0,  ///< Lowest-power standby mode.
  ONE_SHOT_FORCED_AUTO = 1,  ///< One-shot conversion with forced auto-range reset.
  ONE_SHOT             = 2,  ///< One-shot conversion using prior range history.
  CONTINUOUS           = 3   ///< Continuous conversion mode.
};

/// Interrupt latch behavior.
enum class InterruptLatch : uint8_t {
  TRANSPARENT = 0,  ///< Transparent hysteresis behavior.
  LATCHED     = 1   ///< Latched window behavior.
};

/// Interrupt polarity.
enum class InterruptPolarity : uint8_t {
  ACTIVE_LOW  = 0,
  ACTIVE_HIGH = 1
};

/// Number of consecutive faults required before asserting threshold flags.
enum class FaultCount : uint8_t {
  FAULTS_1 = 0,
  FAULTS_2 = 1,
  FAULTS_4 = 2,
  FAULTS_8 = 3
};

/// INT pin direction.
enum class IntDirection : uint8_t {
  PIN_INPUT  = 0,  ///< Hardware one-shot trigger input.
  PIN_OUTPUT = 1   ///< Open-drain interrupt output.
};

/// INT pin output function.
enum class IntConfig : uint8_t {
  THRESHOLD        = 0,  ///< Threshold / SMBus alert behavior.
  EVERY_CONVERSION = 1,  ///< ~1 us pulse after each conversion.
  RESERVED         = 2,  ///< Reserved; do not use.
  FIFO_FULL        = 3   ///< ~1 us pulse every four conversions.
};

/// Threshold register representation.
struct Threshold {
  uint8_t exponent = 0;  ///< Register bits [15:12].
  uint16_t result = 0;   ///< Register bits [11:0].

  constexpr Threshold() = default;
  constexpr Threshold(uint8_t exponentIn, uint16_t resultIn)
      : exponent(exponentIn), result(resultIn) {}
};

/// Configuration for OPT4001 driver.
struct Config {
  // === I2C Transport (required) ===
  I2cWriteFn i2cWrite = nullptr;
  I2cWriteReadFn i2cWriteRead = nullptr;
  void* i2cUser = nullptr;

  // === Timing Hooks (optional) ===
  NowMsFn nowMs = nullptr;
  YieldFn cooperativeYield = nullptr;
  void* timeUser = nullptr;

  // === Device Settings ===
  PackageVariant packageVariant = PackageVariant::SOT_5X3;
  uint8_t i2cAddress = 0x45;  ///< 0x45 fixed for PicoStar, 0x44-0x46 for SOT-5X3.
  uint32_t i2cTimeoutMs = 50;
  bool verifyCrc = true;

  // === Measurement Settings ===
  bool quickWake = false;
  Range range = Range::AUTO;
  ConversionTime conversionTime = ConversionTime::MS_100;
  Mode mode = Mode::POWER_DOWN;  ///< Only POWER_DOWN or CONTINUOUS are valid for begin().

  // === Interrupt / Threshold Settings ===
  InterruptLatch interruptLatch = InterruptLatch::LATCHED;
  InterruptPolarity interruptPolarity = InterruptPolarity::ACTIVE_LOW;
  FaultCount faultCount = FaultCount::FAULTS_1;
  IntDirection intDirection = IntDirection::PIN_OUTPUT;
  IntConfig intConfig = IntConfig::THRESHOLD;
  bool burstMode = true;
  Threshold lowThreshold{};
  Threshold highThreshold{0x0B, 0x0FFF};

  // === Optional INT Pin Hook ===
  int intPin = -1;
  GpioReadFn gpioRead = nullptr;
  void* gpioUser = nullptr;

  // === Health Tracking ===
  uint8_t offlineThreshold = 5;
};

}  // namespace OPT4001
