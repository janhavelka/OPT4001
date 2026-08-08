/**
 * @file HealthDiag.h
 * @brief Periodic health monitor for the OPT4001 Arduino example.
 *
 * NOT part of the library API. Example-only.
 */

#pragma once

#include <Arduino.h>

#include "OPT4001/OPT4001.h"
#include "examples/common/HealthView.h"
#include "examples/common/Log.h"

namespace diag {

/** Periodically print health or print immediately when state/counters change. */
class HealthMonitor {
public:
  /**
   * @param intervalMs Periodic reporting interval; zero reports changes only.
   */
  void begin(uint32_t intervalMs = 1000U) {
    _intervalMs = intervalMs;
    _lastLogMs = 0U;
    _lastState = OPT4001::DriverState::UNINIT;
    _lastConsecutiveFailures = 0U;
  }

  /**
   * @param driver Driver whose cache-only health accessors are sampled.
   * @param forceLog Print even when no tracked value or interval changed.
   */
  void tick(const OPT4001::OPT4001& driver, bool forceLog = false) {
    const uint32_t nowMs = millis();
    const OPT4001::DriverState currentState = driver.state();
    const uint8_t currentFailures = driver.consecutiveFailures();
    const bool stateChanged = currentState != _lastState;
    const bool failuresChanged = currentFailures != _lastConsecutiveFailures;
    const bool intervalElapsed =
        _intervalMs > 0U && static_cast<uint32_t>(nowMs - _lastLogMs) >= _intervalMs;

    if (!stateChanged && !failuresChanged && !intervalElapsed && !forceLog) {
      return;
    }

    if (stateChanged) {
      LOGI("[HEALTH] State transition: %s -> %s",
           OPT4001::driverStateName(_lastState),
           OPT4001::driverStateName(currentState));
    }
    printHealthView(driver);

    _lastState = currentState;
    _lastConsecutiveFailures = currentFailures;
    _lastLogMs = nowMs;
  }

private:
  uint32_t _intervalMs = 1000U;
  uint32_t _lastLogMs = 0U;
  OPT4001::DriverState _lastState = OPT4001::DriverState::UNINIT;
  uint8_t _lastConsecutiveFailures = 0U;
};

}  // namespace diag
