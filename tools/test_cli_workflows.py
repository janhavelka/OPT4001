#!/usr/bin/env python3
"""Run verbatim CLI workflow functions on the host, with controlled I/O.

Rollback cases link the real driver; watch cases isolate CLI scheduling with a
fake driver. This does not compile Arduino/ESP-IDF adapters or prove hardware.
"""

from __future__ import annotations

import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
ARDUINO = ROOT / "examples/01_basic_bringup_cli/main.cpp"
IDF = ROOT / "examples/esp_idf/basic/main/main.cpp"


def function(path: pathlib.Path, name: str) -> str:
    match = re.search(rf"^(?:bool|void) {name}\([^\n]*\) \{{.*?^\}}",
                      path.read_text(encoding="utf-8"), re.MULTILINE | re.DOTALL)
    if match is None:
        raise AssertionError(f"Cannot find {name} in {path}")
    return match.group(0)


PREAMBLE = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include "OPT4001/OPT4001.h"
#define CHECK(condition) do { if (!(condition)) { \
  std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); return 1; \
} } while (false)
#define LOGW(...) ((void)0)
#define LOGE(...) ((void)0)
void printStatus(const OPT4001::Status&) {}
'''


def rollback_source() -> str:
    return PREAMBLE + r'''
struct Bus {
  uint8_t address = 0x45;
  bool checkHandle = false;
  bool failPrevious = false;
  bool failReplacement = false;
  bool failRollback = false;
  bool mismatch = false;
  unsigned reads = 0;
  unsigned writes = 0;
} bus;
OPT4001::Status write(uint8_t address, const uint8_t*, size_t, uint32_t, void*) {
  ++bus.writes;
  bus.mismatch |= bus.checkHandle && address != bus.address;
  return OPT4001::Status::Ok();
}
OPT4001::Status read(uint8_t address, const uint8_t* tx, size_t, uint8_t* rx,
                     size_t len, uint32_t, void*) {
  ++bus.reads;
  bus.mismatch |= bus.checkHandle && address != bus.address;
  if (address == 0x46 || bus.failPrevious)
    return OPT4001::Status::Error(OPT4001::Err::I2C_ERROR, "injected probe failure");
  if (len != 2 || tx[0] != OPT4001::cmd::REG_DEVICE_ID)
    return OPT4001::Status::Error(OPT4001::Err::I2C_ERROR, "unexpected register read");
  rx[0] = static_cast<uint8_t>(OPT4001::cmd::DEVICE_ID_RESET >> 8);
  rx[1] = static_cast<uint8_t>(OPT4001::cmd::DEVICE_ID_RESET);
  return OPT4001::Status::Ok();
}
OPT4001::Config config() {
  OPT4001::Config cfg;
  cfg.i2cWrite = write;
  cfg.i2cWriteRead = read;
  cfg.i2cUser = &bus;
  cfg.packageVariant = OPT4001::PackageVariant::SOT_5X3;
  cfg.i2cAddress = 0x45;
  cfg.range = OPT4001::Range::RANGE_3;
  return cfg;
}
namespace arduino {
OPT4001::OPT4001 device;
struct Activity { bool active = false; } watchState, stressState;
void finishWatch(bool) { watchState.active = false; }
void finishStress(bool) { stressState.active = false; }
void printDriverHealth() {}
''' + function(ARDUINO, "rebeginWithConfig") + r'''
}
namespace idf {
OPT4001::OPT4001 device;
struct Activity { bool active = false; } watchState, stressState;
uint8_t selectedAddress = 0x45;
OPT4001::PackageVariant selectedPackage = OPT4001::PackageVariant::SOT_5X3;
constexpr int GPIO_NUM_NC = -1;
constexpr int INT_PIN = GPIO_NUM_NC;
bool opt4001IdfGpioRead(int, void*) { return false; }
void finishStress(bool) { stressState.active = false; }
void printInfo(const char*) {}
OPT4001::Config makeConfig() { return config(); }
bool replaceDeviceAddress(uint8_t address) {
  if ((address == 0x46 && bus.failReplacement) ||
      (address == 0x45 && bus.failRollback)) return false;
  bus.address = address;
  selectedAddress = address;
  return true;
}
''' + function(IDF, "rebeginWithProfile") + r'''
}
int main(int argc, char** argv) {
  CHECK(argc == 4);
  const bool isIdf = std::string(argv[1]) == "idf";
  const int state = std::atoi(argv[2]); // 0: unbound, 1: bound, 2: initialized
  const std::string failure = argv[3];
  auto& device = isIdf ? idf::device : arduino::device;
  auto previous = config();
  previous.range = OPT4001::Range::RANGE_7;
  if (state == 1) CHECK(device.bind(previous).ok());
  if (state == 2) CHECK(device.begin(previous).ok());
  bus.checkHandle = isIdf;
  bus.failPrevious = failure == "restore_probe";
  bus.failReplacement = failure == "replace";
  bus.failRollback = failure == "restore_handle";
  bus.reads = 0;
  bus.writes = 0;
  auto next = previous;
  next.i2cAddress = failure == "success" ? 0x44 : 0x46;
  next.range = OPT4001::Range::RANGE_0;
  if (failure == "validation") next.packageVariant = OPT4001::PackageVariant::PICOSTAR;
  const bool result = isIdf ? idf::rebeginWithProfile(next.i2cAddress, next.packageVariant)
                            : arduino::rebeginWithConfig(next);
  CHECK(!bus.mismatch);
  if (failure == "success") {
    CHECK(result && device.isInitialized());
    CHECK(device.getConfig().i2cAddress == 0x44);
    CHECK(device.getConfig().range == (isIdf ? (state == 0 ? config().range : previous.range)
                                           : next.range));
    if (isIdf) CHECK(bus.address == 0x44 && idf::selectedAddress == bus.address);
    return 0;
  }
  CHECK(!result);
  if (failure == "restore_handle") {
    CHECK(!device.isBound());
    CHECK(!device.isInitialized());
    CHECK(bus.address == 0x46 && idf::selectedAddress == bus.address);
    return 0;
  }
  CHECK(device.isBound() == (state != 0));
  CHECK(device.isInitialized() == (state == 2 && failure != "restore_probe"));
  if (state != 0) {
    CHECK(device.getConfig().i2cAddress == previous.i2cAddress);
    CHECK(device.getConfig().packageVariant == previous.packageVariant);
    CHECK(device.getConfig().range == previous.range);
    CHECK(device.getConfig().i2cUser == previous.i2cUser);
  }
  if (state < 2) {
    CHECK(bus.reads == (failure == "validation" || failure == "replace" ? 0U : 1U));
    CHECK(bus.writes == 0); // A bound-only rollback must not initialize hardware.
  }
  if (isIdf) {
    CHECK(bus.address == previous.i2cAddress);
    CHECK(idf::selectedAddress == previous.i2cAddress);
    CHECK(idf::selectedPackage == previous.packageVariant);
  }
}
'''


def watch_source() -> str:
    source = ARDUINO.read_text(encoding="utf-8")
    state = re.search(r"^struct WatchState \{.*?^\};", source, re.MULTILINE | re.DOTALL)
    if state is None:
        raise AssertionError("Cannot find WatchState")
    return PREAMBLE + r'''
constexpr uint32_t WATCH_DEFAULT_INTERVAL_MS = 250U;
constexpr uint32_t WATCH_MIN_POLL_MS = 5U;
constexpr uint32_t BLOCKING_READ_TIMEOUT_MS = 1500U;
constexpr const char* LOG_COLOR_CYAN = "";
constexpr const char* LOG_COLOR_RESET = "";
uint32_t nowMs = 100;
uint32_t millis() { return nowMs; }
template <typename T> struct HealthSnapshot {
  template <typename Driver> void capture(const Driver&) {}
};
void printHealthDiff(const HealthSnapshot<OPT4001::OPT4001>&,
                     const HealthSnapshot<OPT4001::OPT4001>&) {}
const char* goodIfNonZeroColor(uint32_t) { return ""; }
const char* warnCountColor(uint32_t) { return ""; }
const char* goodIfZeroColor(uint32_t) { return ""; }
namespace cli { const char* successRateColor(float) { return ""; } }
struct Output {
  std::string text;
  void println(const char* line) { text += line; text += '\n'; }
  void printf(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    text += buffer;
  }
} Serial;
struct Driver {
  OPT4001::Mode mode = OPT4001::Mode::CONTINUOUS;
  unsigned polls = 0;
  unsigned samples = 0;
  unsigned starts = 0;
  bool ready = false;
  OPT4001::Mode getMode() const { return mode; }
  OPT4001::Status conversionReady(bool& value) {
    ++polls;
    value = ready;
    return OPT4001::Status::Ok();
  }
  OPT4001::Status readSample(OPT4001::Sample&) { ++samples; return OPT4001::Status::Ok(); }
  OPT4001::Status tryReadSample(OPT4001::Sample& sample, bool& didRead) {
    ++polls;
    didRead = ready;
    return ready ? readSample(sample) : OPT4001::Status::Ok();
  }
  OPT4001::Status startConversion(OPT4001::Mode) {
    ++starts;
    return OPT4001::Status::Error(OPT4001::Err::IN_PROGRESS, "started");
  }
  uint32_t getOneShotBudgetMs(OPT4001::Mode) { return 1; }
  uint8_t sampleCounterDelta(uint8_t before, uint8_t after) {
    return OPT4001::OPT4001{}.sampleCounterDelta(before, after);
  }
} device;
void printSample(const OPT4001::Sample&) {}
''' + state.group(0) + "\nWatchState watchState;\n" + "\n".join(
        function(ARDUINO, name) for name in (
            "sampleStatusHasData", "sampleStatusWarn", "conversionStartAccepted",
            "resetWatchState", "recordWatchSample", "recordWatchStatus", "finishWatch", "handleWatch",
        )
    ) + r'''
int main(int argc, char** argv) {
  CHECK(argc == 2);
  const std::string scenario = argv[1];
  watchState.active = true;
  watchState.remaining = 20;
  watchState.intervalMs = 1;
  watchState.lastStepMs = nowMs - 1;
  watchState.lastPollMs = nowMs - WATCH_MIN_POLL_MS;
  if (scenario == "continuous_cadence") {
    device.ready = true;
    for (unsigned i = 0; i < 4; ++i) { handleWatch(); ++nowMs; }
    CHECK(device.samples == 4); // Ready data follows the requested 1 ms cadence.
    CHECK(watchState.lastStepMs == nowMs - 1);
  } else if (scenario == "continuous_retries" || scenario == "oneshot_retries") {
    const bool oneShot = scenario == "oneshot_retries";
    if (oneShot) {
      device.mode = OPT4001::Mode::POWER_DOWN;
      handleWatch();
      CHECK(device.starts == 1);
    }
    handleWatch();
    CHECK(device.polls == 1);
    for (unsigned i = 1; i < WATCH_MIN_POLL_MS; ++i) { ++nowMs; handleWatch(); }
    CHECK(device.polls == 1); // Repeated pending checks must not hit the transport.
    ++nowMs;
    device.ready = true;
    handleWatch();
    CHECK(device.polls == 2 && device.samples == 1);
    ++nowMs;
    handleWatch();
    if (oneShot) {
      CHECK(device.starts == 2); // Starting the next conversion is not poll-throttled.
      handleWatch();
    }
    CHECK(device.samples == 2);
  } else if (scenario == "wraparound") {
    device.mode = OPT4001::Mode::POWER_DOWN;
    watchState.waitingConversion = true;
    watchState.lastPollMs = UINT32_MAX - 2U;
    watchState.conversionDeadlineMs = 100;
    nowMs = 1;
    handleWatch();
    CHECK(device.polls == 0);
    nowMs = 2;
    handleWatch();
    CHECK(device.polls == 1);
  } else if (scenario == "timeout") {
    device.mode = OPT4001::Mode::POWER_DOWN;
    watchState.waitingConversion = true;
    watchState.conversionDeadlineMs = nowMs;
    handleWatch();
    CHECK(watchState.failCount == 1);
    CHECK(watchState.lastFail.code == OPT4001::Err::TIMEOUT);
    CHECK(!watchState.waitingConversion);
  } else if (scenario == "continuous_summary" || scenario == "oneshot_summary") {
    if (scenario == "oneshot_summary") device.mode = OPT4001::Mode::POWER_DOWN;
    watchState.haveSample = true;
    watchState.counterGapCount = 3;
    finishWatch(false);
    if (scenario == "continuous_summary") {
      CHECK(Serial.text.find("Counter gaps: n/a (continuous mode)") != std::string::npos);
      CHECK(Serial.text.find("Counter gaps: 3") == std::string::npos);
    } else {
      CHECK(Serial.text.find("Counter gaps: 3") != std::string::npos);
    }
  } else {
    CHECK(false);
  }
}
'''


class CliWorkflowsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        compiler = shutil.which("g++")
        if compiler is None:
            raise AssertionError("g++ is required for CLI workflow regressions")
        cls.directory = tempfile.TemporaryDirectory(prefix="opt4001-cli-workflows-")
        cls.addClassCleanup(cls.directory.cleanup)
        cls.binaries = {}
        for name, source in (("rollback", rollback_source()), ("watch", watch_source())):
            cpp = pathlib.Path(cls.directory.name) / f"{name}.cpp"
            binary = cpp.with_suffix(".exe")
            cpp.write_text(source, encoding="utf-8")
            build = subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "include"),
                 str(cpp), str(ROOT / "src/OPT4001.cpp"), "-o", str(binary)],
                capture_output=True, text=True, timeout=30, check=False,
            )
            if build.returncode != 0:
                raise AssertionError(build.stdout + build.stderr)
            cls.binaries[name] = binary

    def run_case(self, binary: str, *args: str) -> None:
        run = subprocess.run([str(self.binaries[binary]), *args], capture_output=True,
                             text=True, timeout=5, check=False)
        self.assertEqual(0, run.returncode, run.stdout + run.stderr)

    def test_arduino_rollback_preserves_lifecycle_and_config(self) -> None:
        for state in range(3):
            for failure in ("probe", "validation", "restore_probe"):
                with self.subTest(state=state, failure=failure):
                    self.run_case("rollback", "arduino", str(state), failure)

    def test_idf_rollback_preserves_lifecycle_config_and_transport(self) -> None:
        for state in range(3):
            for failure in ("probe", "validation", "replace", "restore_probe", "restore_handle"):
                with self.subTest(state=state, failure=failure):
                    self.run_case("rollback", "idf", str(state), failure)

    def test_continuous_cadence(self) -> None:
        self.run_case("watch", "continuous_cadence")

    def test_profile_success_preserves_bound_settings(self) -> None:
        for example in ("arduino", "idf"):
            for state in range(3):
                with self.subTest(example=example, state=state):
                    self.run_case("rollback", example, str(state), "success")

    def test_continuous_readiness_retries(self) -> None:
        self.run_case("watch", "continuous_retries")

    def test_oneshot_readiness_retries(self) -> None:
        self.run_case("watch", "oneshot_retries")

    def test_watch_wraparound_and_timeout(self) -> None:
        self.run_case("watch", "wraparound")
        self.run_case("watch", "timeout")

    def test_watch_summary(self) -> None:
        self.run_case("watch", "continuous_summary")
        self.run_case("watch", "oneshot_summary")


if __name__ == "__main__":
    unittest.main(verbosity=2)
