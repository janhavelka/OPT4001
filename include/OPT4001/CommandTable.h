/// @file CommandTable.h
/// @brief Register addresses and bit definitions for OPT4001.
#pragma once

#include <cstdint>

namespace OPT4001 {
namespace cmd {

// ============================================================================
// I2C Addresses
// ============================================================================

static constexpr uint8_t I2C_ADDR_GND = 0x44;
static constexpr uint8_t I2C_ADDR_DEFAULT = 0x45;
static constexpr uint8_t I2C_ADDR_SDA = 0x46;

// ============================================================================
// Register Addresses
// ============================================================================

static constexpr uint8_t REG_RESULT = 0x00;
static constexpr uint8_t REG_RESULT_LSB_CRC = 0x01;
static constexpr uint8_t REG_FIFO0_MSB = 0x02;
static constexpr uint8_t REG_FIFO0_LSB_CRC = 0x03;
static constexpr uint8_t REG_FIFO1_MSB = 0x04;
static constexpr uint8_t REG_FIFO1_LSB_CRC = 0x05;
static constexpr uint8_t REG_FIFO2_MSB = 0x06;
static constexpr uint8_t REG_FIFO2_LSB_CRC = 0x07;
static constexpr uint8_t REG_THRESHOLD_L = 0x08;
static constexpr uint8_t REG_THRESHOLD_H = 0x09;
static constexpr uint8_t REG_CONFIGURATION = 0x0A;
static constexpr uint8_t REG_INT_CONFIGURATION = 0x0B;
static constexpr uint8_t REG_FLAGS = 0x0C;
static constexpr uint8_t REG_DEVICE_ID = 0x11;

// ============================================================================
// Reset Values
// ============================================================================

static constexpr uint16_t RESULT_RESET = 0x0000;
static constexpr uint16_t RESULT_LSB_CRC_RESET = 0x0000;
static constexpr uint16_t THRESHOLD_L_RESET = 0x0000;
static constexpr uint16_t THRESHOLD_H_RESET = 0xBFFF;
static constexpr uint16_t CONFIGURATION_RESET = 0x3208;
static constexpr uint16_t INT_CONFIGURATION_RESET = 0x8011;
static constexpr uint16_t FLAGS_RESET = 0x0000;
static constexpr uint16_t DEVICE_ID_RESET = 0x0121;

// ============================================================================
// RESULT / FIFO Format
// ============================================================================

static constexpr uint16_t MASK_EXPONENT = 0xF000;
static constexpr uint16_t MASK_RESULT_MSB = 0x0FFF;
static constexpr uint16_t MASK_RESULT_LSB = 0xFF00;
static constexpr uint16_t MASK_COUNTER = 0x00F0;
static constexpr uint16_t MASK_CRC = 0x000F;

static constexpr uint8_t BIT_EXPONENT = 12;
static constexpr uint8_t BIT_RESULT_LSB = 8;
static constexpr uint8_t BIT_COUNTER = 4;
static constexpr uint8_t BIT_CRC = 0;

// ============================================================================
// Threshold Register Format
// ============================================================================

static constexpr uint16_t MASK_THRESHOLD_EXPONENT = 0xF000;
static constexpr uint16_t MASK_THRESHOLD_RESULT = 0x0FFF;
static constexpr uint8_t BIT_THRESHOLD_EXPONENT = 12;

static constexpr uint8_t THRESHOLD_EXPONENT_MAX = 0x0F;
static constexpr uint16_t THRESHOLD_RESULT_MAX = 0x0FFF;

// ============================================================================
// Configuration Register 0x0A
// ============================================================================

static constexpr uint16_t MASK_QWAKE = 0x8000;
static constexpr uint16_t MASK_CONFIGURATION_RESERVED = 0x4000;
static constexpr uint16_t MASK_RANGE = 0x3C00;
static constexpr uint16_t MASK_CONVERSION_TIME = 0x03C0;
static constexpr uint16_t MASK_MODE = 0x0030;
static constexpr uint16_t MASK_LATCH = 0x0008;
static constexpr uint16_t MASK_INT_POL = 0x0004;
static constexpr uint16_t MASK_FAULT_COUNT = 0x0003;

static constexpr uint8_t BIT_QWAKE = 15;
static constexpr uint8_t BIT_RANGE = 10;
static constexpr uint8_t BIT_CONVERSION_TIME = 6;
static constexpr uint8_t BIT_MODE = 4;
static constexpr uint8_t BIT_LATCH = 3;
static constexpr uint8_t BIT_INT_POL = 2;
static constexpr uint8_t BIT_FAULT_COUNT = 0;

// ============================================================================
// Interrupt Configuration Register 0x0B
// ============================================================================

static constexpr uint16_t MASK_INTCFG_FIXED = 0xFFE0;
static constexpr uint16_t INTCFG_FIXED_BITS = 0x8000;  ///< bits[15:5] == 0x400 field value
static constexpr uint16_t MASK_INT_DIR = 0x0010;
static constexpr uint16_t MASK_INT_CFG = 0x000C;
static constexpr uint16_t MASK_INTCFG_RESERVED = 0x0002;
static constexpr uint16_t MASK_I2C_BURST = 0x0001;

static constexpr uint8_t BIT_INT_DIR = 4;
static constexpr uint8_t BIT_INT_CFG = 2;
static constexpr uint8_t BIT_I2C_BURST = 0;

// ============================================================================
// Flags Register 0x0C
// ============================================================================

static constexpr uint16_t MASK_FLAGS_RESERVED = 0xFFF0;
static constexpr uint16_t MASK_OVERLOAD_FLAG = 0x0008;
static constexpr uint16_t MASK_CONVERSION_READY_FLAG = 0x0004;
static constexpr uint16_t MASK_FLAG_H = 0x0002;
static constexpr uint16_t MASK_FLAG_L = 0x0001;

// ============================================================================
// Device ID Register 0x11
// ============================================================================

static constexpr uint16_t MASK_DIDL = 0x3000;
static constexpr uint16_t MASK_DIDH = 0x0FFF;
static constexpr uint16_t DIDH_EXPECTED = 0x0121;

// ============================================================================
// Timing And Scaling
// ============================================================================

static constexpr uint32_t CONVERSION_TIME_US[] = {
  600U, 1000U, 1800U, 3400U, 6500U, 12700U,
  25000U, 50000U, 100000U, 200000U, 400000U, 800000U
};

static constexpr uint32_t CONVERSION_TIME_MS_CEIL[] = {
  1U, 1U, 2U, 4U, 7U, 13U, 25U, 50U, 100U, 200U, 400U, 800U
};

static constexpr uint32_t ONE_SHOT_STANDBY_US = 500U;
static constexpr uint32_t FORCED_AUTO_RANGE_EXTRA_US = 500U;

static constexpr float LUX_LSB_PICOSTAR = 312.5e-6f;
static constexpr float LUX_LSB_SOT_5X3 = 437.5e-6f;
static constexpr uint16_t MICRO_LUX_NUMERATOR_PICOSTAR = 3125U;  ///< divide by 10
static constexpr uint16_t MICRO_LUX_NUMERATOR_SOT_5X3 = 4375U;   ///< divide by 10

// ============================================================================
// Miscellaneous
// ============================================================================

static constexpr uint8_t GENERAL_CALL_ADDRESS = 0x00;
static constexpr uint8_t GENERAL_CALL_RESET = 0x06;
static constexpr uint8_t SAMPLE_COUNT_MODULO = 16;

}  // namespace cmd
}  // namespace OPT4001
