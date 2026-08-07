#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "OPT4001/OPT4001.h"
#include "../../../common/CliLineBuffer.h"
#include "Opt4001IdfI2cTransport.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
constexpr gpio_num_t INT_PIN = GPIO_NUM_NC;
constexpr uint8_t I2C_ADDRESS = 0x45;
constexpr uint32_t I2C_FREQ_HZ = 400000;
constexpr uint32_t I2C_TIMEOUT_MS = 50;
constexpr uint32_t STRESS_COUNT_MAX = 10000;
constexpr uint32_t WATCH_COUNT_MAX = 100000;
constexpr uint32_t WATCH_INTERVAL_MIN_MS = 1;
constexpr uint32_t WATCH_INTERVAL_MAX_MS = 3600000;
constexpr size_t REGISTER_DUMP_MAX_BYTES = 64;

const char* COLOR_RESET = "\033[0m";
const char* COLOR_RED = "\033[31m";
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_YELLOW = "\033[33m";
const char* COLOR_CYAN = "\033[36m";
constexpr int HELP_COMMAND_WIDTH = 32;

OPT4001::OPT4001 device;
Opt4001IdfI2c i2c;
bool verboseMode = false;
bool colorEnabled = true;
uint8_t selectedAddress = I2C_ADDRESS;
OPT4001::PackageVariant selectedPackage = OPT4001::PackageVariant::SOT_5X3;

struct WatchState {
  bool active = false;
  bool forceAuto = false;
  uint32_t remaining = 0;
  uint32_t intervalMs = 1000;
  uint32_t nextMs = 0;
  uint32_t completed = 0;
  uint32_t failed = 0;
};

WatchState watchState;

enum class StressPhase : uint8_t { START, READY, POLL, SLOT };

struct StressState {
  bool active = false;
  bool mixed = false;
  uint32_t total = 0;
  uint32_t completed = 0;
  uint32_t ok = 0;
  uint32_t warn = 0;
  uint32_t fail = 0;
  uint32_t startMs = 0;
  uint8_t operation = 0;
  StressPhase phase = StressPhase::START;
};

StressState stressState;

struct HealthMonitorState {
  bool enabled = false;
  bool seeded = false;
  uint32_t intervalMs = 1000U;
  uint32_t nextMs = 0U;
  OPT4001::DriverState state = OPT4001::DriverState::UNINIT;
  uint8_t consecutiveFailures = 0U;
  uint32_t totalFailures = 0U;
  uint32_t totalSuccess = 0U;
};

HealthMonitorState healthMonitor;

void setColorEnabled(bool enabled) {
  colorEnabled = enabled;
  COLOR_RESET = enabled ? "\033[0m" : "";
  COLOR_RED = enabled ? "\033[31m" : "";
  COLOR_GREEN = enabled ? "\033[32m" : "";
  COLOR_YELLOW = enabled ? "\033[33m" : "";
  COLOR_CYAN = enabled ? "\033[36m" : "";
}

void printHelpSection(const char* title) {
  printf("\n%s[%s]%s\n", COLOR_GREEN, title, COLOR_RESET);
}

void printHelpItem(const char* command, const char* description) {
  printf("  %s%-*s%s - %s\n", COLOR_CYAN, HELP_COMMAND_WIDTH, command,
         COLOR_RESET, description);
}

void printUsage(const char* usage) {
  printf("%s[W]%s Usage: %s\n", COLOR_YELLOW, COLOR_RESET, usage);
}

void printInfo(const char* message) {
  printf("%s[I]%s %s\n", COLOR_CYAN, COLOR_RESET, message);
}

void lowerInPlace(char* text) {
  for (; text != nullptr && *text != '\0'; ++text) {
    *text = static_cast<char>(tolower(static_cast<unsigned char>(*text)));
  }
}

char* nextToken(char** save) {
  return strtok_r(nullptr, " \t", save);
}

bool parseU32(const char* text, uint32_t& out) {
  if (text == nullptr || text[0] == '\0' || text[0] == '-') return false;
  char* end = nullptr;
  const unsigned long value = strtoul(text, &end, 0);
  if (end == text || *end != '\0' || value > UINT32_MAX) return false;
  out = static_cast<uint32_t>(value);
  return true;
}

bool parseF32(const char* text, float& out) {
  if (text == nullptr || text[0] == '\0') return false;
  char* end = nullptr;
  out = strtof(text, &end);
  return end != text && *end == '\0';
}

bool parseBool(const char* text, bool& out) {
  if (text == nullptr) return false;
  if (strcmp(text, "1") == 0 || strcmp(text, "on") == 0 || strcmp(text, "true") == 0) {
    out = true;
    return true;
  }
  if (strcmp(text, "0") == 0 || strcmp(text, "off") == 0 || strcmp(text, "false") == 0) {
    out = false;
    return true;
  }
  return false;
}

bool parseFaultCount(const char* text, OPT4001::FaultCount& out) {
  uint32_t value = 0;
  if (!parseU32(text, value)) return false;
  switch (value) {
    case 1: out = OPT4001::FaultCount::FAULTS_1; return true;
    case 2: out = OPT4001::FaultCount::FAULTS_2; return true;
    case 4: out = OPT4001::FaultCount::FAULTS_4; return true;
    case 8: out = OPT4001::FaultCount::FAULTS_8; return true;
    default: return false;
  }
}

const char* errToStr(OPT4001::Err err) { return OPT4001::errorName(err); }

const char* stateToStr(OPT4001::DriverState state) {
  return OPT4001::driverStateName(state);
}

bool sampleStatusHasData(const OPT4001::Status& st) {
  return st.ok() || st.code == OPT4001::Err::CRC_ERROR;
}

bool conversionStartAccepted(const OPT4001::Status& st) {
  return st.inProgress() || st.code == OPT4001::Err::BUSY;
}

void printStatus(OPT4001::Status st) {
  const char* color = st.ok() ? COLOR_GREEN
                              : (st.code == OPT4001::Err::CRC_ERROR ||
                                 st.code == OPT4001::Err::MEASUREMENT_NOT_READY ||
                                 st.inProgress())
                                    ? COLOR_YELLOW
                                    : COLOR_RED;
  printf("  Status: %s%s%s (code=%u, detail=%ld)\n", color,
         errToStr(st.code), COLOR_RESET, static_cast<unsigned>(st.code),
         static_cast<long>(st.detail));
  if (st.msg != nullptr && st.msg[0] != '\0') printf("  Message: %s\n", st.msg);
}

OPT4001::Range parseRange(uint32_t value) {
  return value == 12 ? OPT4001::Range::AUTO : static_cast<OPT4001::Range>(value);
}

OPT4001::ConversionTime parseConversionTime(uint32_t value) {
  return static_cast<OPT4001::ConversionTime>(value);
}

OPT4001::Config makeConfig() {
  OPT4001::Config cfg{};
  cfg.i2cWrite = opt4001IdfI2cWrite;
  cfg.i2cWriteRead = opt4001IdfI2cWriteRead;
  cfg.i2cUser = &i2c;
  cfg.nowMs = opt4001IdfNowMs;
  cfg.cooperativeYield = opt4001IdfYield;
  cfg.timeUser = &i2c;
  cfg.i2cAddress = selectedAddress;
  cfg.i2cTimeoutMs = I2C_TIMEOUT_MS;
  cfg.packageVariant = selectedPackage;
  const bool hasInt = selectedPackage == OPT4001::PackageVariant::SOT_5X3 &&
                      INT_PIN != GPIO_NUM_NC;
  cfg.intPin = hasInt ? static_cast<int>(INT_PIN) : -1;
  cfg.gpioRead = hasInt ? opt4001IdfGpioRead : nullptr;
  cfg.gpioUser = &i2c;
  cfg.offlineThreshold = 5;
  return cfg;
}

void printHelp() {
  printf("\n%s=== OPT4001 Native ESP-IDF CLI Help ===%s\n", COLOR_CYAN,
         COLOR_RESET);

  printHelpSection("Common");
  printHelpItem("help / ?", "Show this help");
  printHelpItem("version / ver", "Print library/build version");
  printHelpItem("scan", "Scan I2C bus for ACKs");
  printHelpItem("discover", "Probe legal addresses and validate DEVICE_ID");
  printHelpItem("init / begin", "Initialize/reinitialize selected profile");
  printHelpItem("end", "End driver and return to UNINIT");
  printHelpItem("bind / unbind", "Bus-silent cache bind or release");
  printHelpItem("attach", "Start five-step poll-chunked attach");
  printHelpItem("powerdown", "Error-reporting one-write power down");
  printHelpItem("addr [0x44|0x45|0x46]", "Show or set target address");
  printHelpItem("pkg [pico|sot]", "Show or set package variant");
  printHelpItem("verbose [0|1]", "Toggle verbose diagnostics");
  printHelpItem("color [0|1|off|on]", "Toggle ANSI color output");

  printHelpSection("Data");
  printHelpItem("read [force] [N]", "Bounded blocking fresh sample read(s)");
  printHelpItem("readblocking [force]", "Explicit blocking-read alias");
  printHelpItem("tryread / trylux", "Poll-friendly fresh read helpers");
  printHelpItem("start [force]", "Start regular/forced-auto one-shot");
  printHelpItem("poll / drdy / ready", "Check fresh conversion readiness");
  printHelpItem("readburst [force]", "Read newest plus FIFO0..FIFO2");
  printHelpItem("slot <0..3>", "Read one result/FIFO history slot");
  printHelpItem("sample / sampleage", "Show cached sample or age");
  printHelpItem("lux / mlux / ulux", "Read scaled lux helpers");
  printHelpItem("watch [N] [interval]", "Run bounded sample stream");
  printHelpItem("watch force [N] [interval]", "Stream forced-auto one-shots");
  printHelpItem("stop", "Stop active watch/stress session");
  printHelpItem("job", "Show active poll-job state");
  printHelpItem("job sample|burst", "Start poll-chunked fresh read job");
  printHelpItem("job measure <r> <ct> <m> [qw]", "Start four-write config job");
  printHelpItem("job reset confirm", "Start bus-wide reset/reapply job");
  printHelpItem("job poll [1..16]", "Poll job; default 1 is owner-fair, >1 batches diagnostics");
  printHelpItem("job result|cancel", "Show cached result or cancel without I2C");

  printHelpSection("Configuration");
  printHelpItem("cfg / settings", "Show live and cached configuration");
  printHelpItem("snapshot", "Show cached settings only");
  printHelpItem("range [0..8|auto]", "Show or set full-scale range");
  printHelpItem("ctime [0..11]", "Show or set conversion-time enum");
  printHelpItem("mode [power|cont]", "Show or set stable operating mode");
  printHelpItem("measure <r> <ct> <m> [qw]", "Range/ctime/power|cont tuple");
  printHelpItem("qwake / quickwake [0|1]", "Show or set quick-wake");
  printHelpItem("crc [0|1]", "Show or set host CRC verification");
  printHelpItem("burst [0|1]", "Show or set I2C burst mode");
  printHelpItem("threshold [low high]", "Show or set lux thresholds");
  printHelpItem("threshold raw <low> <high>", "Set packed threshold registers");
  printHelpItem("threshold default", "Restore reset threshold window");
  printHelpItem("thcalc <lux> / thdecode <raw>", "Threshold conversion helpers");
  printHelpItem("int ready|fifo|th ...", "Apply interrupt presets");
  printHelpItem("int latch|pol|faults|dir|cfg", "Low-level INT fields");
  printHelpItem("intpin", "Read configured INT GPIO assertion");

  printHelpSection("Registers");
  printHelpItem("id / identify", "Read and decode DEVICE_ID");
  printHelpItem("config [write <hex>]", "Read/decode or write CONFIGURATION");
  printHelpItem("intcfg [write <hex>]", "Read/decode or write INT_CONFIGURATION");
  printHelpItem("status / flags", "Read/decode FLAGS (clear-on-read)");
  printHelpItem("status_raw / flags_raw / flags raw", "Read raw FLAGS (clear-on-read)");
  printHelpItem("flags readyclear|clear / clearflags", "Clear ready-only or all flags");
  printHelpItem("dump", "Dump public register map");
  printHelpItem("reg / rreg <addr>", "Read one 16-bit register");
  printHelpItem("regs <start> <bytes>", "Read bounded register byte block");
  printHelpItem("wreg <addr> <value>", "Diagnostic raw register write");

  printHelpSection("Diagnostics / Helpers");
  printHelpItem("drv / health / online", "Show full driver health");
  printHelpItem("healthmon [0|1] [interval]", "Cached health monitor; 0 ms is change-only");
  printHelpItem("state", "Show compact health line");
  printHelpItem("diag", "Consolidated diagnostic report");
  printHelpItem("scale / timing", "Show scaling and timing tables");
  printHelpItem("adc2lux <codes>", "Convert linear ADC codes to lux");
  printHelpItem("raw", "Read current RESULT registers without freshness proof");
  printHelpItem("raw2lux <exp> <mant>", "Convert result fields to lux");
  printHelpItem("probe / recover", "Raw identity probe or manual recovery");
  printHelpItem("reset / resetreapply", "General-call reset paths (bus-wide)");
  printHelpItem("selfcheck / selftest", "Bounded identity/config/sample/recovery checks");
  printHelpItem("stress / stress_mix [N]", "Run finite cooperative diagnostic stress");
  printHelpItem("demo", "Alias for watch 10 1000");
}

void printHealth() {
  const OPT4001::DriverState state = device.state();
  const char* color = state == OPT4001::DriverState::READY ? COLOR_GREEN
      : state == OPT4001::DriverState::OFFLINE ? COLOR_RED : COLOR_YELLOW;
  printf("Driver: state=%s%s%s online=%s consec=%u ok=%lu fail=%lu lastOk=%lu lastErr=%lu\n",
         color, stateToStr(state), COLOR_RESET, device.isOnline() ? "yes" : "no",
         static_cast<unsigned>(device.consecutiveFailures()),
         static_cast<unsigned long>(device.totalSuccess()),
         static_cast<unsigned long>(device.totalFailures()),
         static_cast<unsigned long>(device.lastOkMs()),
         static_cast<unsigned long>(device.lastErrorMs()));
  if (!device.lastError().ok()) printStatus(device.lastError());
}

void printSample(const OPT4001::Sample& sample) {
  printf("Sample: lux=%.6f adc=%lu exp=%u mant=0x%05lX counter=%u crc=0x%X valid=%s\n",
         sample.lux, static_cast<unsigned long>(sample.adcCodes), sample.exponent,
         static_cast<unsigned long>(sample.mantissa), sample.counter, sample.crc,
         sample.crcValid ? "yes" : "no");
}

void readAndPrintSample() {
  OPT4001::Sample sample;
  const OPT4001::Status st = device.readLatestSample(sample);
  if (sampleStatusHasData(st)) printSample(sample);
  printStatus(st);
}

void serviceHealthMonitor() {
  if (!healthMonitor.enabled) return;
  const uint32_t now = opt4001IdfNowMs(&i2c);
  const bool changed = !healthMonitor.seeded ||
      healthMonitor.state != device.state() ||
      healthMonitor.consecutiveFailures != device.consecutiveFailures() ||
      healthMonitor.totalFailures != device.totalFailures() ||
      healthMonitor.totalSuccess != device.totalSuccess();
  const bool due = healthMonitor.intervalMs > 0U &&
      static_cast<int32_t>(now - healthMonitor.nextMs) >= 0;
  if (!changed && !due) return;
  printHealth();
  healthMonitor.seeded = true;
  healthMonitor.state = device.state();
  healthMonitor.consecutiveFailures = device.consecutiveFailures();
  healthMonitor.totalFailures = device.totalFailures();
  healthMonitor.totalSuccess = device.totalSuccess();
  healthMonitor.nextMs = now + healthMonitor.intervalMs;
}

void scanI2c() {
  puts("I2C scan:");
  for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
    if (i2c_master_probe(i2c.bus, addr, static_cast<int>(I2C_TIMEOUT_MS)) == ESP_OK) {
      printf("  0x%02X ACK\n", addr);
    }
  }
}

void printSettings() {
  OPT4001::SettingsSnapshot s;
  printStatus(device.getSettings(s));
  printf("Settings: bound=%s init=%s state=%s addr=0x%02X range=%u ctime=%u mode=%u quick=%s crc=%s sample=%s age=%lu\n",
         s.bound ? "yes" : "no", s.initialized ? "yes" : "no",
         stateToStr(s.state), s.i2cAddress,
         static_cast<unsigned>(s.range), static_cast<unsigned>(s.conversionTime),
         static_cast<unsigned>(s.mode), s.quickWake ? "yes" : "no",
         s.verifyCrc ? "yes" : "no", s.hasSample ? "yes" : "no",
         static_cast<unsigned long>(device.sampleAgeMs(opt4001IdfNowMs(&i2c))));
}

void dumpRegisters(uint8_t start, size_t len) {
  if (len > REGISTER_DUMP_MAX_BYTES) len = REGISTER_DUMP_MAX_BYTES;
  uint8_t data[REGISTER_DUMP_MAX_BYTES] = {};
  const OPT4001::Status st = device.readRegisters(start, data, len);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  for (size_t i = 0; i < len; ++i) {
    printf("  Reg 0x%02X %-3s = 0x%02X\n",
           static_cast<unsigned>(start + (i / 2U)),
           ((i & 1U) == 0U) ? "MSB" : "LSB", data[i]);
  }
}

void discoverOpt4001() {
  puts("OPT4001 protocol-qualified discovery (DEVICE_ID):");
  bool found = false;
  for (uint8_t address = OPT4001::cmd::I2C_ADDR_GND;
       address <= OPT4001::cmd::I2C_ADDR_SDA;
       ++address) {
    Opt4001IdfI2c candidate = i2c;
    candidate.address = address;
    candidate.generalCallDev = nullptr;
    i2c_master_dev_handle_t temporary = nullptr;
    if (address != i2c.address) {
      i2c_device_config_t devConfig{};
      devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
      devConfig.device_address = address;
      devConfig.scl_speed_hz = I2C_FREQ_HZ;
      const esp_err_t addResult =
          i2c_master_bus_add_device(i2c.bus, &devConfig, &temporary);
      if (addResult != ESP_OK) {
        if (verboseMode) {
          printf("  0x%02X: add handle failed (%s)\n", address,
                 esp_err_to_name(addResult));
        }
        continue;
      }
      candidate.dev = temporary;
    }

    OPT4001::Config cfg = makeConfig();
    cfg.i2cUser = &candidate;
    cfg.timeUser = &candidate;
    cfg.gpioRead = nullptr;
    cfg.intPin = -1;
    cfg.packageVariant = OPT4001::PackageVariant::SOT_5X3;
    cfg.i2cAddress = address;
    OPT4001::OPT4001 sensor;
    OPT4001::Status st = sensor.bind(cfg);
    if (st.ok()) {
      st = sensor.probe();
    }
    if (st.ok()) {
      found = true;
      printf("  0x%02X: %sOPT4001%s (0x45 may be SOT-5X3 or PicoStar)\n",
             address, COLOR_GREEN, COLOR_RESET);
    } else if (verboseMode) {
      printf("  0x%02X: %s\n", address, errToStr(st.code));
    }
    if (temporary != nullptr) {
      (void)i2c_master_bus_rm_device(temporary);
    }
  }
  if (!found) {
    printf("  %sNo qualified OPT4001 found%s\n", COLOR_YELLOW, COLOR_RESET);
  }
}

bool replaceDeviceAddress(uint8_t address) {
  if (address < 0x44 || address > 0x46 || i2c.bus == nullptr) return false;
  if (address == i2c.address) {
    selectedAddress = address;
    return true;
  }
  i2c_device_config_t cfg{};
  cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  cfg.device_address = address;
  cfg.scl_speed_hz = I2C_FREQ_HZ;
  i2c_master_dev_handle_t replacement = nullptr;
  esp_err_t err = i2c_master_bus_add_device(i2c.bus, &cfg, &replacement);
  if (err != ESP_OK) {
    printf("%s[E]%s Could not add address 0x%02X: %s\n", COLOR_RED,
           COLOR_RESET, address, esp_err_to_name(err));
    return false;
  }
  err = i2c_master_bus_rm_device(i2c.dev);
  if (err != ESP_OK) {
    i2c_master_bus_rm_device(replacement);
    printf("%s[E]%s Could not remove previous address handle: %s\n",
           COLOR_RED, COLOR_RESET, esp_err_to_name(err));
    return false;
  }
  i2c.dev = replacement;
  i2c.address = address;
  selectedAddress = address;
  return true;
}

void printAddressPackage() {
  printf("Profile: address=0x%02X package=%s\n", selectedAddress,
         selectedPackage == OPT4001::PackageVariant::PICOSTAR ? "PICOSTAR" : "SOT_5X3");
}

void printConfigRegisters() {
  OPT4001::ConfigurationInfo cfg{};
  OPT4001::Status st = device.readConfiguration(cfg);
  if (st.ok()) {
    printf("CONFIG=0x%04X range=%u ctime=%u mode=%u qwake=%s latch=%u pol=%u faults=%u valid=%s\n",
           cfg.raw, static_cast<unsigned>(cfg.range),
           static_cast<unsigned>(cfg.conversionTime), static_cast<unsigned>(cfg.mode),
           cfg.quickWake ? "on" : "off", static_cast<unsigned>(cfg.interruptLatch),
           static_cast<unsigned>(cfg.interruptPolarity), static_cast<unsigned>(cfg.faultCount),
           cfg.valid ? "yes" : "no");
  }
  printStatus(st);
  OPT4001::IntConfigurationInfo intCfg{};
  st = device.readIntConfiguration(intCfg);
  if (st.ok()) {
    printf("INT_CONFIG=0x%04X dir=%u function=%u burst=%s fixed=%s valid=%s\n",
           intCfg.raw, static_cast<unsigned>(intCfg.intDirection),
           static_cast<unsigned>(intCfg.intConfig), intCfg.burstMode ? "on" : "off",
           intCfg.fixedPatternValid ? "yes" : "no", intCfg.valid ? "yes" : "no");
  }
  printStatus(st);
}

void printThresholds() {
  OPT4001::Threshold low{}, high{};
  float lowLux = 0.0f, highLux = 0.0f;
  OPT4001::Status st = device.getThresholds(low, high);
  if (st.ok()) {
    printf("Threshold raw: low=0x%X%03X high=0x%X%03X\n", low.exponent,
           low.result, high.exponent, high.result);
  }
  printStatus(st);
  st = device.getThresholdsLux(lowLux, highLux);
  if (st.ok()) printf("Threshold lux: low=%.6f high=%.6f lx\n", lowLux, highLux);
  printStatus(st);
}

void printBurstFrame(const OPT4001::BurstFrame& frame) {
  puts("Newest:"); printSample(frame.newest);
  puts("FIFO0:"); printSample(frame.fifo0);
  puts("FIFO1:"); printSample(frame.fifo1);
  puts("FIFO2:"); printSample(frame.fifo2);
}

OPT4001::Status readBurstBlocking(bool forceAuto, OPT4001::BurstFrame& frame) {
  const OPT4001::Mode mode = forceAuto ? OPT4001::Mode::ONE_SHOT_FORCED_AUTO
                                        : OPT4001::Mode::ONE_SHOT;
  OPT4001::Status st = device.startConversion(mode);
  if (!conversionStartAccepted(st)) return st;
  const uint32_t start = opt4001IdfNowMs(&i2c);
  const uint32_t timeout = device.getOneShotBudgetMs(mode) + 100U;
  for (uint32_t polls = 0; polls < 2000U; ++polls) {
    bool ready = false;
    st = device.conversionReady(ready);
    if (!st.ok()) return st;
    if (ready) return device.readBurst(frame);
    if (static_cast<uint32_t>(opt4001IdfNowMs(&i2c) - start) >= timeout) {
      return OPT4001::Status::Error(OPT4001::Err::TIMEOUT, "Burst read timed out");
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return OPT4001::Status::Error(OPT4001::Err::TIMEOUT, "Burst poll limit reached");
}

OPT4001::Status primeFreshSample() {
  OPT4001::Status st = device.startConversion(OPT4001::Mode::ONE_SHOT);
  if (!conversionStartAccepted(st)) return st;
  const uint32_t start = opt4001IdfNowMs(&i2c);
  const uint32_t timeout = device.getOneShotBudgetMs(OPT4001::Mode::ONE_SHOT) + 100U;
  for (uint32_t polls = 0; polls < 2000U; ++polls) {
    bool ready = false;
    st = device.conversionReady(ready);
    if (!st.ok() || ready) return st;
    if (static_cast<uint32_t>(opt4001IdfNowMs(&i2c) - start) >= timeout) {
      return OPT4001::Status::Error(OPT4001::Err::TIMEOUT, "Conversion timed out");
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return OPT4001::Status::Error(OPT4001::Err::TIMEOUT, "Conversion poll limit reached");
}

void finishStress(bool cancelled) {
  if (!stressState.active) return;
  const uint32_t elapsed = opt4001IdfNowMs(&i2c) - stressState.startMs;
  if (device.pollBusy()) (void)device.cancelPollJob();
  stressState.active = false;
  printf("%s%s summary: completed=%lu/%lu ok=%lu warn=%lu fail=%lu elapsed=%lu ms\n",
         stressState.mixed ? "stress_mix" : "stress",
         cancelled ? " (cancelled)" : "",
         static_cast<unsigned long>(stressState.completed),
         static_cast<unsigned long>(stressState.total),
         static_cast<unsigned long>(stressState.ok),
         static_cast<unsigned long>(stressState.warn),
         static_cast<unsigned long>(stressState.fail),
         static_cast<unsigned long>(elapsed));
  printHealth();
}

void recordStressStatus(const OPT4001::Status& st) {
  if (st.ok()) ++stressState.ok;
  else if (st.code == OPT4001::Err::CRC_ERROR) ++stressState.warn;
  else {
    ++stressState.fail;
    if (verboseMode) printStatus(st);
  }
  ++stressState.completed;
  stressState.phase = StressPhase::START;
  if (stressState.completed >= stressState.total) finishStress(false);
}

void startStressSession(uint32_t count, bool mixed) {
  if (!device.isInitialized() || count == 0U || count > STRESS_COUNT_MAX) {
    printUsage("stress [1..10000] (initialized driver required)");
    return;
  }
  if (stressState.active) finishStress(true);
  watchState.active = false;
  stressState = {};
  stressState.active = true;
  stressState.mixed = mixed;
  stressState.total = count;
  stressState.startMs = opt4001IdfNowMs(&i2c);
  printf("%s started: %lu finite operations, cooperative one-callback service budget\n",
         mixed ? "stress_mix" : "stress", static_cast<unsigned long>(count));
}

void serviceStress() {
  if (!stressState.active) return;
  if (stressState.phase == StressPhase::READY) {
    bool ready = false;
    const OPT4001::Status st = device.conversionReady(ready);
    if (!st.ok()) recordStressStatus(st);
    else if (ready) stressState.phase = StressPhase::SLOT;
    return;
  }
  if (stressState.phase == StressPhase::POLL) {
    const OPT4001::Status st = device.poll(opt4001IdfNowMs(&i2c), 1);
    if (st.inProgress()) return;
    recordStressStatus(st);
    return;
  }
  if (stressState.phase == StressPhase::SLOT) {
    OPT4001::Sample sample{};
    recordStressStatus(device.readSampleSlot(0U, sample));
    return;
  }

  stressState.operation = stressState.mixed
                              ? static_cast<uint8_t>(stressState.completed % 8U)
                              : 0U;
  if (stressState.operation <= 2U) {
    OPT4001::Status st = device.startConversion(OPT4001::Mode::ONE_SHOT);
    if (!conversionStartAccepted(st)) {
      recordStressStatus(st);
      return;
    }
    if (stressState.operation == 2U) {
      stressState.phase = StressPhase::READY;
      return;
    }
    st = stressState.operation == 1U ? device.startReadBurst()
                                     : device.startReadSample();
    if (!st.inProgress()) {
      recordStressStatus(st);
      return;
    }
    stressState.phase = StressPhase::POLL;
    return;
  }

  OPT4001::Status st = OPT4001::Status::Ok();
  switch (stressState.operation) {
    case 3: { OPT4001::ConfigurationInfo info{}; st = device.readConfiguration(info); break; }
    case 4: { OPT4001::IntConfigurationInfo info{}; st = device.readIntConfiguration(info); break; }
    case 5: { OPT4001::DeviceIdInfo info{}; st = device.readDeviceId(info); break; }
    case 6: { uint16_t raw = 0; st = device.readFlagsRaw(raw); break; }
    case 7: st = device.probe(); break;
    default: st = OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM,
                                         "Invalid stress operation"); break;
  }
  recordStressStatus(st);
}

void runSelfTest() {
  uint32_t pass = 0;
  uint32_t fail = 0;
  auto report = [&](const char* name, bool passed, const char* detail = "") {
    printf("  %s[%s]%s %-34s%s%s\n",
           passed ? COLOR_GREEN : COLOR_RED, passed ? "PASS" : "FAIL",
           COLOR_RESET, name, detail[0] == '\0' ? "" : " - ", detail);
    passed ? ++pass : ++fail;
  };

  printf("\n%s=== OPT4001 selfcheck (bounded diagnostics; FLAGS/read/recover may affect hardware state) ===%s\n",
         COLOR_CYAN, COLOR_RESET);
  if (!device.isInitialized()) {
    report("driver initialized", false, "run init first");
    return;
  }

  const uint32_t successBefore = device.totalSuccess();
  const uint32_t failureBefore = device.totalFailures();
  OPT4001::Status st = device.probe();
  report("probe responds", st.ok(), st.ok() ? "" : OPT4001::errorName(st.code));
  report("probe leaves health counters unchanged",
         device.totalSuccess() == successBefore && device.totalFailures() == failureBefore);

  OPT4001::DeviceIdInfo id{};
  st = device.readDeviceId(id);
  report("DEVICE_ID read and fixed pattern", st.ok() && id.matchesExpected,
         st.ok() ? (id.matchesExpected ? "" : "unexpected ID") : OPT4001::errorName(st.code));

  OPT4001::ConfigurationInfo config{};
  st = device.readConfiguration(config);
  report("CONFIGURATION decode", st.ok() && config.valid,
         st.ok() ? (config.valid ? "" : "invalid fields") : OPT4001::errorName(st.code));

  OPT4001::IntConfigurationInfo intConfig{};
  st = device.readIntConfiguration(intConfig);
  report("INT_CONFIGURATION decode", st.ok() && intConfig.valid,
         st.ok() ? (intConfig.valid ? "" : "invalid fixed/reserved fields")
                 : OPT4001::errorName(st.code));

  OPT4001::Threshold low{}, high{};
  st = device.getThresholds(low, high);
  report("threshold registers readable", st.ok(),
         st.ok() ? "" : OPT4001::errorName(st.code));

  float lowLux = 0.0f;
  float highLux = 0.0f;
  st = device.getThresholdsLux(lowLux, highLux);
  report("threshold lux decode", st.ok() && highLux >= lowLux,
         st.ok() ? (highLux >= lowLux ? "" : "threshold order invalid")
                 : OPT4001::errorName(st.code));
  report("scale helpers sane",
         device.getCurrentFullScaleLux() > 0.0f &&
             device.getCurrentResolutionLux() > 0.0f &&
             device.getEffectiveBits() > 0U);

  OPT4001::SettingsSnapshot settings{};
  st = device.getSettings(settings);
  report("cache-only settings snapshot",
         st.ok() && settings.bound && settings.initialized,
         st.ok() ? "" : OPT4001::errorName(st.code));

  OPT4001::Sample sample{};
  st = device.readBlocking(sample, 1500);
  report("fresh sample read", sampleStatusHasData(st),
         sampleStatusHasData(st) ? "" : OPT4001::errorName(st.code));

  OPT4001::BurstFrame frame{};
  st = readBurstBlocking(false, frame);
  report("newest plus FIFO history read", sampleStatusHasData(st),
         sampleStatusHasData(st) ? "" : OPT4001::errorName(st.code));

  uint16_t flagsRaw = 0;
  st = device.readFlagsRaw(flagsRaw);
  report("FLAGS raw read (clear-on-read)", st.ok(),
         st.ok() ? "" : OPT4001::errorName(st.code));

  st = device.recover();
  report("manual recover/reapply", st.ok(), st.ok() ? "" : OPT4001::errorName(st.code));
  printf("Selfcheck: %spass=%lu%s %sfail=%lu%s\n", COLOR_GREEN,
         static_cast<unsigned long>(pass), COLOR_RESET,
         fail == 0 ? COLOR_GREEN : COLOR_RED,
         static_cast<unsigned long>(fail), COLOR_RESET);
}

void serviceWatch() {
  if (!watchState.active) return;
  const uint32_t now = opt4001IdfNowMs(&i2c);
  if (static_cast<int32_t>(now - watchState.nextMs) < 0) return;
  OPT4001::Sample sample{};
  const OPT4001::Status st = watchState.forceAuto
      ? device.readBlocking(sample, OPT4001::Mode::ONE_SHOT_FORCED_AUTO, 1500)
      : device.readBlocking(sample, 1500);
  if (sampleStatusHasData(st)) {
    printSample(sample);
    ++watchState.completed;
  } else {
    ++watchState.failed;
  }
  if (!st.ok()) printStatus(st);
  if (--watchState.remaining == 0) {
    printf("Watch complete: samples=%lu failed=%lu\n",
           static_cast<unsigned long>(watchState.completed),
           static_cast<unsigned long>(watchState.failed));
    watchState.active = false;
  } else {
    watchState.nextMs = opt4001IdfNowMs(&i2c) + watchState.intervalMs;
  }
}

void dumpPublicRegisters() {
  static constexpr uint8_t REGISTERS[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
      0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x11};
  for (uint8_t reg : REGISTERS) {
    uint16_t value = 0;
    const OPT4001::Status st = device.readRegister16(reg, value);
    if (st.ok()) printf("  [0x%02X] = 0x%04X\n", reg, value);
    else {
      printf("  [0x%02X] ", reg);
      printStatus(st);
    }
  }
}

void processCommand(char* line) {
  lowerInPlace(line);
  char* save = nullptr;
  char* cmd = strtok_r(line, " \t", &save);
  if (cmd == nullptr) return;

  if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
    printHelp();
  } else if (strcmp(cmd, "version") == 0 || strcmp(cmd, "ver") == 0) {
    printf("Version: %s\n", OPT4001::VERSION_FULL);
  } else if (strcmp(cmd, "scan") == 0) {
    scanI2c();
  } else if (strcmp(cmd, "discover") == 0) {
    discoverOpt4001();
  } else if (strcmp(cmd, "bind") == 0) {
    stressState.active = false;
    watchState.active = false;
    printStatus(device.bind(makeConfig()));
  } else if (strcmp(cmd, "unbind") == 0) {
    stressState.active = false;
    watchState.active = false;
    device.unbind();
    printInfo("Driver transport released without I2C");
  } else if (strcmp(cmd, "attach") == 0) {
    printStatus(device.startAttach());
  } else if (strcmp(cmd, "powerdown") == 0) {
    printStatus(device.powerDown());
  } else if (strcmp(cmd, "job") == 0) {
    char* op = nextToken(&save);
    if (op == nullptr) {
      printf("Poll job: %s status=%s\n", device.pollBusy() ? "ACTIVE" : "IDLE",
             errToStr(device.lastPollStatus().code));
    } else if (strcmp(op, "sample") == 0) {
      printStatus(device.startReadSample());
    } else if (strcmp(op, "burst") == 0) {
      printStatus(device.startReadBurst());
    } else if (strcmp(op, "poll") == 0) {
      uint32_t budget = 1U;
      char* budgetArg = nextToken(&save);
      if (budgetArg != nullptr &&
          (!parseU32(budgetArg, budget) || budget == 0U || budget > 16U)) {
        printUsage("job poll [1..16]");
        return;
      }
      printStatus(device.poll(opt4001IdfNowMs(&i2c),
                              static_cast<uint8_t>(budget)));
    } else if (strcmp(op, "result") == 0) {
      printStatus(device.lastPollStatus());
      OPT4001::Sample sample{};
      if (device.getLastSample(sample).ok()) printSample(sample);
      OPT4001::BurstFrame frame{};
      if (device.getLastBurst(frame).ok()) printBurstFrame(frame);
    } else if (strcmp(op, "cancel") == 0) {
      printStatus(device.cancelPollJob());
    } else if (strcmp(op, "reset") == 0) {
      char* confirm = nextToken(&save);
      if (confirm == nullptr || strcmp(confirm, "confirm") != 0) {
        printUsage("job reset confirm");
        return;
      }
      printStatus(device.startResetAndReapply());
    } else if (strcmp(op, "measure") == 0) {
      char* rangeArg = nextToken(&save);
      char* ctimeArg = nextToken(&save);
      char* modeArg = nextToken(&save);
      char* quickArg = nextToken(&save);
      uint32_t range = 12U;
      uint32_t ctime = 0U;
      bool quick = device.getQuickWake();
      const bool rangeOk = rangeArg != nullptr &&
          (strcmp(rangeArg, "auto") == 0 ||
           (parseU32(rangeArg, range) && range <= 8U));
      const bool timeOk = parseU32(ctimeArg, ctime) && ctime <= 11U;
      const bool continuous = modeArg != nullptr &&
          (strcmp(modeArg, "cont") == 0 || strcmp(modeArg, "continuous") == 0);
      const bool power = modeArg != nullptr &&
          (strcmp(modeArg, "power") == 0 || strcmp(modeArg, "pd") == 0);
      if (!rangeOk || !timeOk || (!continuous && !power) ||
          (quickArg != nullptr && !parseBool(quickArg, quick))) {
        printUsage("job measure <0..8|auto> <0..11> <power|cont> [qw]");
        return;
      }
      printStatus(device.startConfigureMeasurement(
          range == 12U ? OPT4001::Range::AUTO
                       : static_cast<OPT4001::Range>(range),
          static_cast<OPT4001::ConversionTime>(ctime),
          continuous ? OPT4001::Mode::CONTINUOUS
                     : OPT4001::Mode::POWER_DOWN,
          quick));
    } else {
      printUsage("job [sample|burst|measure|reset|poll|result|cancel]");
    }
  } else if (strcmp(cmd, "color") == 0) {
    char* arg = nextToken(&save);
    if (arg == nullptr) {
      printf("Color: %s\n", colorEnabled ? "ON" : "OFF");
      return;
    }
    bool enabled = false;
    if (!parseBool(arg, enabled)) {
      printUsage("color [0|1|off|on]");
      return;
    }
    setColorEnabled(enabled);
    printf("Color: %s\n", enabled ? "ON" : "OFF");
  } else if (strcmp(cmd, "verbose") == 0) {
    bool value = false;
    if (parseBool(nextToken(&save), value)) verboseMode = value;
    printf("Verbose: %s\n", verboseMode ? "ON" : "OFF");
  } else if (strcmp(cmd, "healthmon") == 0) {
    char* enabledArg = nextToken(&save);
    if (enabledArg == nullptr) {
      printf("Health monitor: %s interval=%lu ms%s\n",
             healthMonitor.enabled ? "ON" : "OFF",
             static_cast<unsigned long>(healthMonitor.intervalMs),
             healthMonitor.intervalMs == 0U ? " (change-only)" : "");
      return;
    }
    bool enabled = false;
    if (!parseBool(enabledArg, enabled)) {
      printUsage("healthmon [0|1] [intervalMs]");
      return;
    }
    uint32_t interval = healthMonitor.intervalMs;
    char* intervalArg = nextToken(&save);
    if (intervalArg != nullptr &&
        (!parseU32(intervalArg, interval) || interval > 3600000U)) {
      printUsage("healthmon [0|1] [0..3600000]");
      return;
    }
    healthMonitor.enabled = enabled;
    healthMonitor.intervalMs = interval;
    healthMonitor.seeded = false;
    healthMonitor.nextMs = opt4001IdfNowMs(&i2c);
    printf("Health monitor: %s interval=%lu ms%s\n",
           enabled ? "ON" : "OFF", static_cast<unsigned long>(interval),
           interval == 0U ? " (change-only)" : "");
  } else if (strcmp(cmd, "addr") == 0) {
    char* arg = nextToken(&save);
    if (arg != nullptr) {
      uint32_t value = 0;
      if (!parseU32(arg, value) || value < 0x44 || value > 0x46) {
        printUsage("addr [0x44|0x45|0x46]"); return;
      }
      if (selectedPackage == OPT4001::PackageVariant::PICOSTAR && value != 0x45) {
        printInfo("PicoStar has fixed address 0x45"); return;
      }
      watchState.active = false;
      device.end();
      if (replaceDeviceAddress(static_cast<uint8_t>(value))) printStatus(device.begin(makeConfig()));
    }
    printAddressPackage();
  } else if (strcmp(cmd, "pkg") == 0) {
    char* arg = nextToken(&save);
    if (arg != nullptr) {
      lowerInPlace(arg);
      OPT4001::PackageVariant next;
      if (strcmp(arg, "pico") == 0 || strcmp(arg, "picostar") == 0) {
        next = OPT4001::PackageVariant::PICOSTAR;
      } else if (strcmp(arg, "sot") == 0 || strcmp(arg, "sot_5x3") == 0) {
        next = OPT4001::PackageVariant::SOT_5X3;
      } else {
        printUsage("pkg [pico|sot]"); return;
      }
      watchState.active = false;
      device.end();
      if (next == OPT4001::PackageVariant::PICOSTAR && selectedAddress != 0x45) {
        if (!replaceDeviceAddress(0x45)) return;
      }
      selectedPackage = next;
      printStatus(device.begin(makeConfig()));
    }
    printAddressPackage();
  } else if (strcmp(cmd, "init") == 0 || strcmp(cmd, "begin") == 0) {
    watchState.active = false;
    device.end();
    printStatus(device.begin(makeConfig()));
    printHealth();
  } else if (strcmp(cmd, "end") == 0) {
    watchState.active = false;
    device.end();
    puts("Driver state: UNINIT");
  } else if (strcmp(cmd, "drv") == 0 || strcmp(cmd, "health") == 0 ||
             strcmp(cmd, "online") == 0 || strcmp(cmd, "state") == 0) {
    printHealth();
  } else if (strcmp(cmd, "probe") == 0) {
    printStatus(device.probe());
  } else if (strcmp(cmd, "recover") == 0) {
    printStatus(device.recover());
  } else if (strcmp(cmd, "reset") == 0) {
    watchState.active = false;
    printf("%s[W]%s Issuing general-call reset (bus-wide).\n", COLOR_YELLOW, COLOR_RESET);
    printStatus(device.softReset());
  } else if (strcmp(cmd, "resetreapply") == 0) {
    watchState.active = false;
    printf("%s[W]%s Issuing general-call reset and reapply (bus-wide).\n", COLOR_YELLOW, COLOR_RESET);
    printStatus(device.resetAndReapply());
  } else if (strcmp(cmd, "lux") == 0) {
    float lux = 0.0f;
    const OPT4001::Status st = device.readBlockingLux(lux, 1500);
    if (sampleStatusHasData(st)) printf("Lux: %.6f lx\n", lux);
    printStatus(st);
  } else if (strcmp(cmd, "mlux") == 0 || strcmp(cmd, "ulux") == 0) {
    OPT4001::Status st = primeFreshSample();
    if (st.ok() && strcmp(cmd, "mlux") == 0) {
      uint32_t value = 0;
      st = device.readMilliLux(value);
      if (sampleStatusHasData(st)) printf("Milli-lux: %lu mlux\n", static_cast<unsigned long>(value));
    } else if (st.ok()) {
      uint64_t value = 0;
      st = device.readMicroLux(value);
      if (sampleStatusHasData(st)) printf("Micro-lux: %llu ulux\n", static_cast<unsigned long long>(value));
    }
    printStatus(st);
  } else if (strcmp(cmd, "read") == 0 || strcmp(cmd, "readblocking") == 0) {
    char* arg = nextToken(&save);
    bool force = arg != nullptr && strcmp(arg, "force") == 0;
    uint32_t count = 1;
    if (arg != nullptr && !force && !parseU32(arg, count)) { printUsage("read [force] [N]"); return; }
    if (force) {
      char* countArg = nextToken(&save);
      if (countArg != nullptr && !parseU32(countArg, count)) { printUsage("read [force] [N]"); return; }
    }
    if (count == 0 || count > STRESS_COUNT_MAX) { printUsage("read [force] [1..10000]"); return; }
    for (uint32_t i = 0; i < count; ++i) {
      OPT4001::Sample sample{};
      const OPT4001::Status st = force
          ? device.readBlocking(sample, OPT4001::Mode::ONE_SHOT_FORCED_AUTO, 1500)
          : device.readBlocking(sample, 1500);
      if (sampleStatusHasData(st)) printSample(sample);
      printStatus(st);
      if (!sampleStatusHasData(st)) break;
    }
  } else if (strcmp(cmd, "tryread") == 0 || strcmp(cmd, "trylux") == 0) {
    bool didRead = false;
    if (strcmp(cmd, "trylux") == 0) {
      float lux = 0.0f; const auto st = device.tryReadLux(lux, didRead);
      if (didRead) printf("Lux: %.6f\n", lux);
      printStatus(st);
    } else {
      OPT4001::Sample sample{}; const auto st = device.tryReadSample(sample, didRead);
      if (didRead) printSample(sample);
      printStatus(st);
    }
    printf("Fresh sample: %s\n", didRead ? "yes" : "no");
  } else if (strcmp(cmd, "sample") == 0) {
    OPT4001::Sample sample;
    const auto st = device.getLastSample(sample);
    if (st.ok()) printSample(sample);
    printStatus(st);
  } else if (strcmp(cmd, "sampleage") == 0) {
    printf("Sample age: %lu ms\n", static_cast<unsigned long>(device.sampleAgeMs(opt4001IdfNowMs(&i2c))));
  } else if (strcmp(cmd, "start") == 0) {
    char* arg = nextToken(&save);
    printStatus(arg != nullptr && strcmp(arg, "force") == 0
                    ? device.startConversion(OPT4001::Mode::ONE_SHOT_FORCED_AUTO)
                    : device.startConversion());
  } else if (strcmp(cmd, "ready") == 0 || strcmp(cmd, "poll") == 0 || strcmp(cmd, "drdy") == 0) {
    bool ready = false;
    const auto st = device.conversionReady(ready);
    if (st.ok()) {
      printf("Ready: %s\n", ready ? "yes" : "no");
    } else {
      printStatus(st);
    }
  } else if (strcmp(cmd, "flags") == 0 || strcmp(cmd, "status") == 0) {
    char* arg = nextToken(&save);
    if (arg != nullptr && strcmp(arg, "raw") == 0) {
      uint16_t raw = 0; const auto st = device.readFlagsRaw(raw);
      if (st.ok()) printf("FLAGS raw=0x%04X\n", raw);
      printStatus(st);
      return;
    }
    if (arg != nullptr && strcmp(arg, "readyclear") == 0) {
      printStatus(device.clearConversionReadyFlag()); return;
    }
    if (arg != nullptr && strcmp(arg, "clear") == 0) {
      printStatus(device.clearFlags()); return;
    }
    OPT4001::Flags flags;
    const auto st = device.readFlags(flags);
    if (st.ok()) printf("FLAGS=0x%04X ov=%d ready=%d hi=%d lo=%d\n", flags.raw,
                        flags.overload, flags.conversionReady,
                        flags.highThreshold, flags.lowThreshold);
    else printStatus(st);
  } else if (strcmp(cmd, "status_raw") == 0 || strcmp(cmd, "flags_raw") == 0) {
    uint16_t raw = 0;
    const auto st = device.readFlagsRaw(raw);
    if (st.ok()) {
      printf("FLAGS raw=0x%04X\n", raw);
    } else {
      printStatus(st);
    }
  } else if (strcmp(cmd, "clearflags") == 0) {
    printStatus(device.clearFlags());
  } else if (strcmp(cmd, "readburst") == 0 || strcmp(cmd, "fifo") == 0) {
    OPT4001::BurstFrame frame;
    char* arg = nextToken(&save);
    const auto st = readBurstBlocking(arg != nullptr && strcmp(arg, "force") == 0, frame);
    if (sampleStatusHasData(st)) printBurstFrame(frame);
    printStatus(st);
  } else if (strcmp(cmd, "slot") == 0) {
    uint32_t slot = 0;
    if (!parseU32(nextToken(&save), slot) || slot > 3) { printUsage("slot <0..3>"); return; }
    OPT4001::Sample sample{}; const auto st = device.readSampleSlot(static_cast<uint8_t>(slot), sample);
    if (sampleStatusHasData(st)) printSample(sample);
    printStatus(st);
  } else if (strcmp(cmd, "cfg") == 0 || strcmp(cmd, "settings") == 0 || strcmp(cmd, "snapshot") == 0) {
    printSettings();
  } else if (strcmp(cmd, "range") == 0) {
    char* arg = nextToken(&save);
    if (arg == nullptr) { printf("Range: %u\n", static_cast<unsigned>(device.getRange())); return; }
    lowerInPlace(arg); uint32_t value = 12;
    if (strcmp(arg, "auto") != 0 && (!parseU32(arg, value) || value > 8)) { printUsage("range [0..8|auto]"); return; }
    printStatus(device.setRange(parseRange(value)));
  } else if (strcmp(cmd, "ctime") == 0) {
    char* arg = nextToken(&save);
    if (arg == nullptr) { printf("Conversion time enum: %u (%lu us)\n",
        static_cast<unsigned>(device.getConversionTime()), static_cast<unsigned long>(device.getConversionTimeUs())); return; }
    uint32_t value = 0; if (!parseU32(arg, value) || value > 11) { printUsage("ctime [0..11]"); return; }
    printStatus(device.setConversionTime(parseConversionTime(value)));
  } else if (strcmp(cmd, "mode") == 0) {
    char* arg = nextToken(&save);
    if (arg == nullptr) { printf("Mode: %u\n", static_cast<unsigned>(device.getMode())); return; }
    lowerInPlace(arg); OPT4001::Mode value;
    if (strcmp(arg, "power") == 0 || strcmp(arg, "0") == 0) value = OPT4001::Mode::POWER_DOWN;
    else if (strcmp(arg, "cont") == 0 || strcmp(arg, "continuous") == 0 || strcmp(arg, "3") == 0) value = OPT4001::Mode::CONTINUOUS;
    else { printUsage("mode [power|cont]"); return; }
    printStatus(device.setMode(value));
  } else if (strcmp(cmd, "measure") == 0) {
    char* rangeArg = nextToken(&save); char* ctimeArg = nextToken(&save);
    char* modeArg = nextToken(&save); uint32_t range = 12, ctime = 0;
    OPT4001::Mode mode = OPT4001::Mode::POWER_DOWN; bool qwake = false;
    const bool rangeOk = rangeArg != nullptr &&
        (strcmp(rangeArg, "auto") == 0 || (parseU32(rangeArg, range) && range <= 8));
    const bool ctimeOk = parseU32(ctimeArg, ctime) && ctime <= 11;
    const bool modeOk = modeArg != nullptr &&
        ((strcmp(modeArg, "power") == 0 || strcmp(modeArg, "0") == 0) ||
         (strcmp(modeArg, "cont") == 0 || strcmp(modeArg, "continuous") == 0 ||
          strcmp(modeArg, "3") == 0));
    if (modeOk && (strcmp(modeArg, "cont") == 0 || strcmp(modeArg, "continuous") == 0 ||
                   strcmp(modeArg, "3") == 0)) mode = OPT4001::Mode::CONTINUOUS;
    if (!rangeOk || !ctimeOk || !modeOk) {
      printUsage("measure <range|auto> <ctime0..11> <power|cont> [quickwake]"); return;
    }
    char* qw = nextToken(&save); if (qw != nullptr && !parseBool(qw, qwake)) { printUsage("measure ... [0|1]"); return; }
    printStatus(device.configureMeasurement(parseRange(range), parseConversionTime(ctime), mode, qwake));
  } else if (strcmp(cmd, "quickwake") == 0 || strcmp(cmd, "qwake") == 0 || strcmp(cmd, "crc") == 0) {
    char* arg = nextToken(&save);
    if (arg == nullptr) { printf("%s: %s\n", cmd, strcmp(cmd, "crc") == 0 ?
        (device.getVerifyCrc() ? "ON" : "OFF") : (device.getQuickWake() ? "ON" : "OFF")); return; }
    bool value = false; if (!parseBool(arg, value)) { printUsage("quickwake|crc [0|1]"); return; }
    strcmp(cmd, "crc") == 0 ? printStatus(device.setVerifyCrc(value)) : printStatus(device.setQuickWake(value));
  } else if (strcmp(cmd, "burst") == 0) {
    char* arg = nextToken(&save);
    if (arg == nullptr) { printf("Burst mode: %s\n", device.getBurstMode() ? "ON" : "OFF"); return; }
    bool value = false; if (!parseBool(arg, value)) { printUsage("burst [0|1]"); return; }
    printStatus(device.setBurstMode(value));
  } else if (strcmp(cmd, "latch") == 0 || strcmp(cmd, "pol") == 0 || strcmp(cmd, "fault") == 0) {
    char* valueArg = nextToken(&save);
    uint32_t value = 0;
    if (strcmp(cmd, "fault") == 0) {
      OPT4001::FaultCount faults{};
      if (!parseFaultCount(valueArg, faults)) { printUsage("fault <1|2|4|8>"); return; }
      printStatus(device.setFaultCount(faults));
      return;
    }
    if (!parseU32(valueArg, value) || value > 1U) {
      printUsage("latch|pol <0|1>"); return;
    }
    if (strcmp(cmd, "latch") == 0) printStatus(device.setInterruptLatch(value ? OPT4001::InterruptLatch::LATCHED : OPT4001::InterruptLatch::TRANSPARENT));
    if (strcmp(cmd, "pol") == 0) printStatus(device.setInterruptPolarity(value ? OPT4001::InterruptPolarity::ACTIVE_HIGH : OPT4001::InterruptPolarity::ACTIVE_LOW));
  } else if (strcmp(cmd, "int") == 0 || strcmp(cmd, "intpin") == 0) {
    char* arg = nextToken(&save);
    if (strcmp(cmd, "intpin") == 0 || arg == nullptr) {
      bool asserted = false; const auto st = device.readIntPinAsserted(asserted);
      if (st.ok()) {
        printf("INT asserted: %s\n", asserted ? "yes" : "no");
      } else {
        printStatus(st);
      }
    } else if (strcmp(arg, "ready") == 0) {
      printStatus(device.enableConversionReadyInterrupt());
    } else if (strcmp(arg, "fifo") == 0) {
      printStatus(device.enableFifoFullInterrupt());
    } else if (strcmp(arg, "threshold") == 0 || strcmp(arg, "th") == 0) {
      float low = 0.0f, high = 0.0f;
      char* lowArg = nextToken(&save); char* highArg = nextToken(&save);
      if (lowArg != nullptr && highArg != nullptr && parseF32(lowArg, low) && parseF32(highArg, high))
        printStatus(device.enableThresholdInterruptLux(low, high));
      else printStatus(device.setIntConfig(OPT4001::IntConfig::THRESHOLD));
    } else if (strcmp(arg, "dir") == 0) {
      uint32_t value = 0;
      if (!parseU32(nextToken(&save), value) || value > 1) { printUsage("int dir <0|1>"); return; }
      printStatus(device.setIntDirection(value ? OPT4001::IntDirection::PIN_OUTPUT : OPT4001::IntDirection::PIN_INPUT));
    } else if (strcmp(arg, "latch") == 0 || strcmp(arg, "pol") == 0 ||
               strcmp(arg, "faults") == 0 || strcmp(arg, "cfg") == 0) {
      char* valueArg = nextToken(&save);
      if (strcmp(arg, "faults") == 0) {
        OPT4001::FaultCount faults{};
        if (!parseFaultCount(valueArg, faults)) { printUsage("int faults <1|2|4|8>"); return; }
        printStatus(device.setFaultCount(faults));
        return;
      }
      uint32_t value = 0;
      if (!parseU32(valueArg, value)) { printUsage("int latch|pol <0|1> | int cfg <0|1|3>"); return; }
      if ((strcmp(arg, "latch") == 0 || strcmp(arg, "pol") == 0) && value > 1U) {
        printUsage("int latch|pol <0|1>"); return;
      }
      if (strcmp(arg, "latch") == 0) printStatus(device.setInterruptLatch(value ? OPT4001::InterruptLatch::LATCHED : OPT4001::InterruptLatch::TRANSPARENT));
      else if (strcmp(arg, "pol") == 0) printStatus(device.setInterruptPolarity(value ? OPT4001::InterruptPolarity::ACTIVE_HIGH : OPT4001::InterruptPolarity::ACTIVE_LOW));
      else printStatus(device.setIntConfig(static_cast<OPT4001::IntConfig>(value)));
    }
  } else if (strcmp(cmd, "threshold") == 0) {
    char* mode = nextToken(&save);
    if (mode != nullptr && strcmp(mode, "default") == 0) {
      printStatus(device.restoreDefaultThresholds());
    } else if (mode != nullptr && strcmp(mode, "raw") == 0) {
      uint32_t low = 0, high = 0;
      if (!parseU32(nextToken(&save), low) || !parseU32(nextToken(&save), high) ||
          low > 0xFFFF || high > 0xFFFF) { printUsage("threshold raw <low16> <high16>"); return; }
      printStatus(device.setThresholds({static_cast<uint8_t>((low >> 12) & 0x0F), static_cast<uint16_t>(low & 0x0FFF)},
                                       {static_cast<uint8_t>((high >> 12) & 0x0F), static_cast<uint16_t>(high & 0x0FFF)}));
    } else if (mode != nullptr && strcmp(mode, "lux") == 0) {
      float low = 0.0f, high = 0.0f;
      if (!parseF32(nextToken(&save), low) || !parseF32(nextToken(&save), high)) return;
      printStatus(device.setThresholdsLux(low, high));
    } else if (mode != nullptr) {
      float low = 0.0f, high = 0.0f;
      if (!parseF32(mode, low) || !parseF32(nextToken(&save), high)) { printUsage("threshold [low high]"); return; }
      printStatus(device.setThresholdsLux(low, high));
    } else {
      printThresholds();
    }
  } else if (strcmp(cmd, "thcalc") == 0) {
    float lux = 0.0f; if (!parseF32(nextToken(&save), lux)) { printUsage("thcalc <lux>"); return; }
    OPT4001::Threshold th{}; const auto st = device.luxToThreshold(lux, th);
    if (st.ok()) printf("Threshold: exp=%u result=0x%03X raw=0x%X%03X decoded=%.6f lx\n",
                        th.exponent, th.result, th.exponent, th.result, device.thresholdToLux(th));
    printStatus(st);
  } else if (strcmp(cmd, "thdecode") == 0) {
    uint32_t raw = 0; if (!parseU32(nextToken(&save), raw) || raw > 0xFFFF) { printUsage("thdecode <raw>"); return; }
    OPT4001::Threshold th{static_cast<uint8_t>(raw >> 12), static_cast<uint16_t>(raw & 0x0FFF)};
    uint64_t adc = 0; const auto st = device.thresholdToAdcCodes(th, adc);
    if (st.ok()) printf("Threshold: adc=%llu lux=%.6f\n", static_cast<unsigned long long>(adc), device.thresholdToLux(th));
    printStatus(st);
  } else if (strcmp(cmd, "id") == 0 || strcmp(cmd, "identify") == 0) {
    OPT4001::DeviceIdInfo info;
    const auto st = device.readDeviceId(info);
    if (st.ok()) {
      printf("DEVICE_ID raw=0x%04X didh=0x%03X didl=%u reserved_clear=%s match=%s\n",
             info.raw, info.didh, info.didl,
             info.reservedBitsClear ? "yes" : "no",
             info.matchesExpected ? "yes" : "no");
    } else {
      printStatus(st);
    }
  } else if (strcmp(cmd, "config") == 0 || strcmp(cmd, "intcfg") == 0) {
    char* op = nextToken(&save);
    if (op != nullptr && strcmp(op, "write") == 0) {
      uint32_t raw = 0; if (!parseU32(nextToken(&save), raw) || raw > 0xFFFF) { printUsage("config|intcfg write <hex>"); return; }
      printStatus(strcmp(cmd, "config") == 0 ? device.writeConfiguration(static_cast<uint16_t>(raw))
                                               : device.writeIntConfiguration(static_cast<uint16_t>(raw)));
    } else {
      printConfigRegisters();
    }
  } else if (strcmp(cmd, "reg") == 0 || strcmp(cmd, "rreg") == 0) {
    uint32_t reg = 0; if (!parseU32(nextToken(&save), reg) || reg > UINT8_MAX) { printUsage("reg <addr>"); return; }
    uint16_t value = 0; const auto st = device.readRegister16(static_cast<uint8_t>(reg), value);
    if (st.ok()) {
      printf("[0x%02lX]=0x%04X\n", static_cast<unsigned long>(reg), value);
    } else {
      printStatus(st);
    }
  } else if (strcmp(cmd, "wreg") == 0) {
    uint32_t reg = 0, value = 0;
    if (!parseU32(nextToken(&save), reg) || !parseU32(nextToken(&save), value) ||
        reg > UINT8_MAX || value > UINT16_MAX) { printUsage("wreg <addr> <value16>"); return; }
    printStatus(device.writeRegister16(static_cast<uint8_t>(reg), static_cast<uint16_t>(value)));
  } else if (strcmp(cmd, "regs") == 0) {
    uint32_t start = 0, len = 16;
    if (!parseU32(nextToken(&save), start) || !parseU32(nextToken(&save), len) ||
        start > UINT8_MAX || len == 0 || len > REGISTER_DUMP_MAX_BYTES) {
      printUsage("regs <start> <bytes 1..64>"); return;
    }
    dumpRegisters(static_cast<uint8_t>(start), len);
  } else if (strcmp(cmd, "dump") == 0) {
    dumpPublicRegisters();
  } else if (strcmp(cmd, "raw") == 0 || strcmp(cmd, "raw2lux") == 0) {
    uint32_t exp = 0, mant = 0;
    char* expArg = nextToken(&save);
    char* mantArg = nextToken(&save);
    if (expArg == nullptr && strcmp(cmd, "raw") == 0) {
      readAndPrintSample();
    } else if (parseU32(expArg, exp) && parseU32(mantArg, mant)) {
      float lux = 0.0f;
      const OPT4001::Status st = exp <= UINT8_MAX
          ? device.rawToLux(static_cast<uint8_t>(exp), mant, lux)
          : OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "Exponent out of range");
      if (st.ok()) printf("Lux: %.6f\n", lux);
      printStatus(st);
    } else {
      printUsage("raw2lux <exponent> <mantissa>");
    }
  } else if (strcmp(cmd, "adc2lux") == 0) {
    uint32_t adc = 0; if (!parseU32(nextToken(&save), adc)) { printUsage("adc2lux <codes>"); return; }
    printf("Lux: %.6f\n", device.adcCodesToLux(adc));
  } else if (strcmp(cmd, "scale") == 0 || strcmp(cmd, "timing") == 0 || strcmp(cmd, "diag") == 0) {
    printf("Scale: lsb=%.9f fullscale=%.3f resolution=%.9f bits=%u\n",
           device.getLuxLsb(), device.getCurrentFullScaleLux(),
           device.getCurrentResolutionLux(), device.getEffectiveBits());
    printf("Timing: conversion=%lu us one-shot=%lu us forced=%lu us\n",
           static_cast<unsigned long>(device.getConversionTimeUs()),
           static_cast<unsigned long>(device.getOneShotBudgetUs(OPT4001::Mode::ONE_SHOT)),
           static_cast<unsigned long>(device.getOneShotBudgetUs(OPT4001::Mode::ONE_SHOT_FORCED_AUTO)));
    printHealth();
    if (strcmp(cmd, "diag") == 0) { printAddressPackage(); printSettings(); printConfigRegisters(); }
  } else if (strcmp(cmd, "selfcheck") == 0 || strcmp(cmd, "selftest") == 0) {
    runSelfTest();
  } else if (strcmp(cmd, "stress") == 0 || strcmp(cmd, "stress_mix") == 0) {
    uint32_t count = strcmp(cmd, "stress_mix") == 0 ? 50 : 10;
    char* countArg = nextToken(&save);
    if (countArg != nullptr && !parseU32(countArg, count)) { printUsage("stress [1..10000]"); return; }
    if (count == 0 || count > STRESS_COUNT_MAX) { printUsage("stress [1..10000]"); return; }
    startStressSession(count, strcmp(cmd, "stress_mix") == 0);
  } else if (strcmp(cmd, "watch") == 0 || strcmp(cmd, "demo") == 0) {
    if (stressState.active) finishStress(true);
    bool force = false; uint32_t count = strcmp(cmd, "demo") == 0 ? 10U : 10U;
    uint32_t interval = strcmp(cmd, "demo") == 0 ? 1000U : 1000U;
    char* arg = nextToken(&save);
    if (arg != nullptr && strcmp(arg, "force") == 0) { force = true; arg = nextToken(&save); }
    if (arg != nullptr && !parseU32(arg, count)) { printUsage("watch [force] [N] [interval]"); return; }
    char* intervalArg = nextToken(&save);
    if (intervalArg != nullptr && !parseU32(intervalArg, interval)) { printUsage("watch [force] [N] [interval]"); return; }
    if (count == 0 || count > WATCH_COUNT_MAX || interval < WATCH_INTERVAL_MIN_MS || interval > WATCH_INTERVAL_MAX_MS) {
      printUsage("watch [force] [1..100000] [1..3600000]"); return;
    }
    watchState = {};
    watchState.active = true; watchState.forceAuto = force; watchState.remaining = count;
    watchState.intervalMs = interval; watchState.nextMs = opt4001IdfNowMs(&i2c);
    printf("Watch: ON count=%lu interval=%lu force=%s\n",
           static_cast<unsigned long>(count), static_cast<unsigned long>(interval), force ? "yes" : "no");
  } else if (strcmp(cmd, "stop") == 0) {
    if (stressState.active) finishStress(true);
    watchState.active = false;
    if (device.pollBusy()) printStatus(device.cancelPollJob());
    printInfo("Active session stopped");
  } else {
    printf("Unknown command: %s\n", cmd);
  }
}

void configureI2c() {
  i2c.address = I2C_ADDRESS;
  i2c.intPin = INT_PIN;

  i2c_master_bus_config_t busConfig{};
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.i2c_port = I2C_NUM_0;
  busConfig.sda_io_num = I2C_SDA;
  busConfig.scl_io_num = I2C_SCL;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;
  ESP_ERROR_CHECK(i2c_new_master_bus(&busConfig, &i2c.bus));

  i2c_device_config_t devConfig{};
  devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devConfig.device_address = I2C_ADDRESS;
  devConfig.scl_speed_hz = I2C_FREQ_HZ;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c.bus, &devConfig, &i2c.dev));

  i2c_device_config_t generalCallConfig{};
  generalCallConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  generalCallConfig.device_address = 0x00;
  generalCallConfig.scl_speed_hz = I2C_FREQ_HZ;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c.bus, &generalCallConfig, &i2c.generalCallDev));
}

bool configureConsole() {
  setvbuf(stdin, nullptr, _IONBF, 0);
  setvbuf(stdout, nullptr, _IONBF, 0);

  const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags < 0) {
    puts("stdin flags unavailable; refusing to start blocking diagnostic CLI.");
    return false;
  }
  if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0) {
    puts("stdin nonblocking mode failed; refusing to start blocking diagnostic CLI.");
    return false;
  }
  return true;
}

void cliLoop() {
  static cli_shell::FixedLineBuffer line;
  static char input[cli_shell::FixedLineBuffer::CAPACITY]{};
  printf("> ");
  while (true) {
    device.tick(opt4001IdfNowMs(&i2c));
    serviceWatch();
    serviceStress();
    serviceHealthMonitor();

    const int c = getchar();
    if (c == EOF) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    const cli_shell::LineResult result =
        line.push(static_cast<char>(c), input, sizeof(input));
    if (result == cli_shell::LineResult::READY) {
      processCommand(input);
      printf("> ");
    } else if (result == cli_shell::LineResult::TOO_LONG) {
      printUsage("command shorter than 192 bytes; complete line discarded");
      printf("> ");
    } else if (result == cli_shell::LineResult::OUTPUT_TOO_SMALL) {
      puts("CLI output buffer too small");
      printf("> ");
    }
  }
}

}  // namespace

extern "C" void app_main(void) {
  if (!configureConsole()) {
    return;
  }
  puts("=== OPT4001 native ESP-IDF bringup ===");
  configureI2c();
  scanI2c();
  printStatus(device.begin(makeConfig()));
  printHealth();
  puts("Type 'help' for commands.");
  cliLoop();
}
