/**
 * @file BoardConfig.h
 * @brief Example board configuration for ESP32-S2 / ESP32-S3 reference hardware.
 *
 * These are convenience defaults for reference designs only.
 * NOT part of the library API. Override for your hardware.
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "examples/common/I2cTransport.h"

namespace board {

static constexpr int I2C_SDA = 8;
static constexpr int I2C_SCL = 9;
static constexpr uint32_t I2C_FREQ_HZ = 400000;
static constexpr uint16_t I2C_TIMEOUT_MS = 50;
static constexpr int LED = 48;

/// Optional INT pin from the SOT-5X3 package. Set to -1 to disable.
static constexpr int INT_PIN = -1;

inline bool initI2c() {
  return transport::initWire(I2C_SDA, I2C_SCL, I2C_FREQ_HZ, I2C_TIMEOUT_MS);
}

inline void initIntPin() {
  if (INT_PIN >= 0) {
    pinMode(INT_PIN, INPUT_PULLUP);
  }
}

inline bool readIntPin(int pin, void* user) {
  (void)user;
  return digitalRead(pin) != 0;
}

inline void initSerial(uint32_t baud = 115200) {
  Serial.begin(baud);
  const uint32_t startMs = millis();
  while (!Serial && (millis() - startMs) < 3000U) {
    delay(10);
  }
}

}  // namespace board
