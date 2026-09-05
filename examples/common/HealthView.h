#pragma once

#include <Arduino.h>

#include "OPT4001/OPT4001.h"
#include "examples/common/CliStyle.h"

namespace health_view {

inline const char* colorGreen() { return LOG_COLOR_GREEN; }
inline const char* colorYellow() { return LOG_COLOR_YELLOW; }
inline const char* colorRed() { return LOG_COLOR_RED; }
inline const char* colorGray() { return LOG_COLOR_GRAY; }
inline const char* colorReset() { return LOG_COLOR_RESET; }

inline const char* boolColor(bool value) {
  return value ? colorGreen() : colorRed();
}

inline const char* failureColor(uint32_t failures) {
  if (failures == 0U) {
    return colorGreen();
  }
  if (failures < 3U) {
    return colorYellow();
  }
  return colorRed();
}

inline const char* successColor(uint32_t successes) {
  return (successes > 0U) ? colorGreen() : colorGray();
}


inline const char* stateColor(OPT4001::DriverState state, bool online,
                              uint8_t consecutiveFailures) {
  if (state == OPT4001::DriverState::UNINIT) {
    return colorYellow();
  }
  return LOG_COLOR_STATE(online, consecutiveFailures);
}

template <typename DriverT>
struct Snapshot {
  OPT4001::DriverState state = OPT4001::DriverState::UNINIT;
  bool online = false;
  bool hardwareConfigDirty = false;
  uint8_t consecutiveFailures = 0;
  uint32_t totalFailures = 0;
  uint32_t totalSuccess = 0;

  void capture(const DriverT& driver) {
    state = driver.state();
    online = driver.isOnline();
    hardwareConfigDirty = driver.hardwareConfigDirty();
    consecutiveFailures = driver.consecutiveFailures();
    totalFailures = driver.totalFailures();
    totalSuccess = driver.totalSuccess();
  }
};

template <typename DriverT>
inline void printHealthView(const DriverT& driver) {
  Snapshot<DriverT> snap;
  snap.capture(driver);
  const uint64_t total = static_cast<uint64_t>(snap.totalSuccess) + snap.totalFailures;
  const float pct = (total > 0U)
                        ? (100.0f * static_cast<float>(snap.totalSuccess) /
                           static_cast<float>(total))
                        : 0.0f;

  Serial.printf("Health: state=%s%s%s online=%s%s%s dirty=%s%s%s consec=%s%u%s ok=%s%lu%s fail=%s%lu%s rate=%s%.1f%%%s\n",
                stateColor(snap.state, snap.online, snap.consecutiveFailures),
                OPT4001::driverStateName(snap.state),
                colorReset(),
                boolColor(snap.online),
                snap.online ? "true" : "false",
                colorReset(),
                snap.hardwareConfigDirty ? colorYellow() : colorGreen(),
                snap.hardwareConfigDirty ? "true" : "false",
                colorReset(),
                failureColor(static_cast<uint32_t>(snap.consecutiveFailures)),
                static_cast<unsigned>(snap.consecutiveFailures),
                colorReset(),
                successColor(snap.totalSuccess),
                static_cast<unsigned long>(snap.totalSuccess),
                colorReset(),
                failureColor(snap.totalFailures),
                static_cast<unsigned long>(snap.totalFailures),
                colorReset(),
                cli::successRateColor(pct),
                pct,
                colorReset());
}

template <typename DriverT>
inline void printHealthDiff(const Snapshot<DriverT>& before,
                            const Snapshot<DriverT>& after) {
  bool changed = false;

  if (before.state != after.state) {
    Serial.printf("  State: %s%s%s -> %s%s%s\n",
                  stateColor(before.state, before.online, before.consecutiveFailures),
                  OPT4001::driverStateName(before.state),
                  colorReset(),
                  stateColor(after.state, after.online, after.consecutiveFailures),
                  OPT4001::driverStateName(after.state),
                  colorReset());
    changed = true;
  }
  if (before.online != after.online) {
    Serial.printf("  Online: %s%s%s -> %s%s%s\n",
                  boolColor(before.online),
                  before.online ? "true" : "false",
                  colorReset(),
                  boolColor(after.online),
                  after.online ? "true" : "false",
                  colorReset());
    changed = true;
  }
  if (before.hardwareConfigDirty != after.hardwareConfigDirty) {
    Serial.printf("  Dirty: %s -> %s\n",
                  before.hardwareConfigDirty ? "true" : "false",
                  after.hardwareConfigDirty ? "true" : "false");
    changed = true;
  }
  if (before.consecutiveFailures != after.consecutiveFailures) {
    const bool improved = after.consecutiveFailures < before.consecutiveFailures;
    Serial.printf("  ConsecFail: %s%u -> %u%s\n",
                  improved ? colorGreen() : colorRed(),
                  static_cast<unsigned>(before.consecutiveFailures),
                  static_cast<unsigned>(after.consecutiveFailures),
                  colorReset());
    changed = true;
  }
  if (before.totalSuccess != after.totalSuccess) {
    Serial.printf("  TotalOK: %lu -> %s%lu (+%lu)%s\n",
                  static_cast<unsigned long>(before.totalSuccess),
                  colorGreen(),
                  static_cast<unsigned long>(after.totalSuccess),
                  static_cast<unsigned long>(after.totalSuccess - before.totalSuccess),
                  colorReset());
    changed = true;
  }
  if (before.totalFailures != after.totalFailures) {
    Serial.printf("  TotalFail: %lu -> %s%lu (+%lu)%s\n",
                  static_cast<unsigned long>(before.totalFailures),
                  colorRed(),
                  static_cast<unsigned long>(after.totalFailures),
                  static_cast<unsigned long>(after.totalFailures - before.totalFailures),
                  colorReset());
    changed = true;
  }
  if (!changed) {
    Serial.println("  (no health changes)");
  }
}

}  // namespace health_view

template <typename DriverT>
using HealthSnapshot = health_view::Snapshot<DriverT>;

using health_view::printHealthDiff;
using health_view::printHealthView;
