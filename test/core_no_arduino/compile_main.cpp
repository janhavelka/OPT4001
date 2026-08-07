#include <cstdint>
#include <type_traits>

#include "OPT4001/CommandTable.h"
#include "OPT4001/Config.h"
#include "OPT4001/OPT4001.h"
#include "OPT4001/Status.h"
#include "OPT4001/Version.h"

static_assert(std::is_trivially_copyable<OPT4001::Status>::value);
static_assert(std::is_trivially_copyable<OPT4001::Threshold>::value);
static_assert(std::is_trivially_copyable<OPT4001::Sample>::value);
static_assert(std::is_trivially_copyable<OPT4001::SettingsSnapshot>::value);

static_assert(!std::is_copy_constructible<OPT4001::OPT4001>::value);
static_assert(!std::is_copy_assignable<OPT4001::OPT4001>::value);
static_assert(!std::is_move_constructible<OPT4001::OPT4001>::value);
static_assert(!std::is_move_assignable<OPT4001::OPT4001>::value);

static_assert(OPT4001::cmd::I2C_ADDR_GND == 0x44U);
static_assert(OPT4001::cmd::I2C_ADDR_DEFAULT == 0x45U);
static_assert(OPT4001::cmd::I2C_ADDR_SDA == 0x46U);

int main() {
  OPT4001::OPT4001 sensor;
  OPT4001::SettingsSnapshot settings{};
  const OPT4001::Status status = sensor.getSettings(settings);
  return status.ok() && !settings.bound && !settings.initialized ? 0 : 1;
}
