/// @file main.cpp
/// @brief OPT4001 basic bring-up CLI example.
/// @note This is an EXAMPLE, not part of the library.

#include <Arduino.h>
#include <cstdlib>

#include "examples/common/BoardConfig.h"
#include "examples/common/BusDiag.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/Log.h"

#include "OPT4001/OPT4001.h"

OPT4001::OPT4001 device;
bool verboseMode = false;

const char* errToStr(OPT4001::Err err) {
  using OPT4001::Err;
  switch (err) {
    case Err::OK: return "OK";
    case Err::NOT_INITIALIZED: return "NOT_INITIALIZED";
    case Err::INVALID_CONFIG: return "INVALID_CONFIG";
    case Err::I2C_ERROR: return "I2C_ERROR";
    case Err::TIMEOUT: return "TIMEOUT";
    case Err::INVALID_PARAM: return "INVALID_PARAM";
    case Err::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
    case Err::DEVICE_ID_MISMATCH: return "DEVICE_ID_MISMATCH";
    case Err::CRC_ERROR: return "CRC_ERROR";
    case Err::MEASUREMENT_NOT_READY: return "MEASUREMENT_NOT_READY";
    case Err::BUSY: return "BUSY";
    case Err::IN_PROGRESS: return "IN_PROGRESS";
    case Err::I2C_NACK_ADDR: return "I2C_NACK_ADDR";
    case Err::I2C_NACK_DATA: return "I2C_NACK_DATA";
    case Err::I2C_TIMEOUT: return "I2C_TIMEOUT";
    case Err::I2C_BUS: return "I2C_BUS";
    default: return "UNKNOWN";
  }
}

const char* stateToStr(OPT4001::DriverState state) {
  using OPT4001::DriverState;
  switch (state) {
    case DriverState::UNINIT: return "UNINIT";
    case DriverState::READY: return "READY";
    case DriverState::DEGRADED: return "DEGRADED";
    case DriverState::OFFLINE: return "OFFLINE";
    default: return "UNKNOWN";
  }
}

const char* modeToStr(OPT4001::Mode mode) {
  using OPT4001::Mode;
  switch (mode) {
    case Mode::POWER_DOWN: return "POWER_DOWN";
    case Mode::ONE_SHOT_FORCED_AUTO: return "ONE_SHOT_FORCED_AUTO";
    case Mode::ONE_SHOT: return "ONE_SHOT";
    case Mode::CONTINUOUS: return "CONTINUOUS";
    default: return "UNKNOWN";
  }
}

const char* rangeToStr(OPT4001::Range range) {
  switch (range) {
    case OPT4001::Range::AUTO: return "AUTO";
    case OPT4001::Range::RANGE_0: return "RANGE_0";
    case OPT4001::Range::RANGE_1: return "RANGE_1";
    case OPT4001::Range::RANGE_2: return "RANGE_2";
    case OPT4001::Range::RANGE_3: return "RANGE_3";
    case OPT4001::Range::RANGE_4: return "RANGE_4";
    case OPT4001::Range::RANGE_5: return "RANGE_5";
    case OPT4001::Range::RANGE_6: return "RANGE_6";
    case OPT4001::Range::RANGE_7: return "RANGE_7";
    case OPT4001::Range::RANGE_8: return "RANGE_8";
    default: return "UNKNOWN";
  }
}

void printStatus(const OPT4001::Status& st) {
  Serial.printf("  Status: %s%s%s (detail=%ld)\n",
                LOG_COLOR_RESULT(st.ok()),
                errToStr(st.code),
                LOG_COLOR_RESET,
                static_cast<long>(st.detail));
  if (st.msg && st.msg[0]) {
    Serial.printf("  Message: %s%s%s\n", LOG_COLOR_YELLOW, st.msg, LOG_COLOR_RESET);
  }
}

void printSample(const OPT4001::Sample& sample) {
  Serial.printf("  Lux: %.6f lx\n", sample.lux);
  Serial.printf("  ADC codes: %lu\n", static_cast<unsigned long>(sample.adcCodes));
  Serial.printf("  Exponent: %u\n", sample.exponent);
  Serial.printf("  Mantissa: 0x%05lX\n", static_cast<unsigned long>(sample.mantissa));
  Serial.printf("  Counter: %u\n", sample.counter);
  Serial.printf("  CRC: 0x%X (%s)\n", sample.crc, sample.crcValid ? "OK" : "BAD");
}

void printDriverHealth() {
  Serial.println("=== Driver Health ===");
  Serial.printf("  State: %s\n", stateToStr(device.state()));
  Serial.printf("  Online: %s\n", log_bool_str(device.isOnline()));
  Serial.printf("  Last OK: %lu ms\n", static_cast<unsigned long>(device.lastOkMs()));
  Serial.printf("  Last error: %lu ms\n", static_cast<unsigned long>(device.lastErrorMs()));
  Serial.printf("  Consecutive failures: %u\n", device.consecutiveFailures());
  Serial.printf("  Total success/fail: %lu / %lu\n",
                static_cast<unsigned long>(device.totalSuccess()),
                static_cast<unsigned long>(device.totalFailures()));
  if (!device.lastError().ok()) {
    Serial.printf("  Last error code: %s\n", errToStr(device.lastError().code));
  }
}

void printSettings() {
  OPT4001::SettingsSnapshot snap;
  device.getSettings(snap);

  Serial.println("=== Cached Settings ===");
  Serial.printf("  Address: 0x%02X\n", snap.i2cAddress);
  Serial.printf("  Package: %s\n",
                snap.packageVariant == OPT4001::PackageVariant::PICOSTAR ? "PICOSTAR" : "SOT_5X3");
  Serial.printf("  Mode: %s\n", modeToStr(snap.mode));
  Serial.printf("  Pending: %s\n", modeToStr(snap.pendingMode));
  Serial.printf("  Range: %s\n", rangeToStr(snap.range));
  Serial.printf("  Conversion time: %lu ms\n",
                static_cast<unsigned long>(device.getConversionTimeMs()));
  Serial.printf("  Quick wake: %s\n", log_bool_str(snap.quickWake));
  Serial.printf("  Burst mode: %s\n", log_bool_str(snap.burstMode));
  Serial.printf("  Verify CRC: %s\n", log_bool_str(snap.verifyCrc));
  Serial.printf("  Sample available: %s\n", log_bool_str(snap.sampleAvailable));
  Serial.printf("  Last lux: %.6f lx\n", snap.lastLux);
}

void printHelp() {
  Serial.println();
  Serial.println("=== OPT4001 CLI Help ===");
  Serial.println("  help / ?                  Show this help");
  Serial.println("  scan                      Scan I2C bus");
  Serial.println("  probe                     Probe device without health tracking");
  Serial.println("  recover                   Re-apply cached config");
  Serial.println("  drv                       Show driver health");
  Serial.println("  cfg / settings            Show cached settings");
  Serial.println("  read                      Trigger one-shot and print sample");
  Serial.println("  read force                Trigger forced auto-range one-shot");
  Serial.println("  read N                    Read N samples using one-shot mode");
  Serial.println("  flags                     Read and print FLAGS register");
  Serial.println("  mode [power|cont]         Set stable mode");
  Serial.println("  range [0..8|auto]         Set range");
  Serial.println("  ctime [0..11]             Set conversion time");
  Serial.println("  qwake [0|1]               Set QWAKE");
  Serial.println("  burst [0|1]               Set I2C burst mode");
  Serial.println("  threshold <low> <high>    Set thresholds in lux");
  Serial.println("  reg <addr>                Read 16-bit register");
  Serial.println("  wreg <addr> <value>       Write 16-bit register");
  Serial.println("  verbose [0|1]             Toggle verbose output");
  Serial.println("  stress [N]                Run N blocking reads");
}

bool parseU32(const String& token, uint32_t& out) {
  char* end = nullptr;
  const unsigned long value = strtoul(token.c_str(), &end, 0);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = static_cast<uint32_t>(value);
  return true;
}

bool parseFloatToken(const String& token, float& out) {
  char* end = nullptr;
  const float value = strtof(token.c_str(), &end);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = value;
  return true;
}

void doRead(OPT4001::Mode mode) {
  OPT4001::Sample sample;
  OPT4001::Status st = device.readBlocking(sample, mode, 1500);
  if (!st.ok() && st.code != OPT4001::Err::CRC_ERROR) {
    printStatus(st);
    return;
  }
  printSample(sample);
  if (!st.ok()) {
    printStatus(st);
  }
}

void processCommand(const String& line) {
  String cmd = line;
  cmd.trim();
  if (cmd.length() == 0) {
    return;
  }

  if (cmd == "help" || cmd == "?") {
    printHelp();
  } else if (cmd == "scan") {
    bus_diag::scan();
  } else if (cmd == "probe") {
    printStatus(device.probe());
  } else if (cmd == "recover") {
    printStatus(device.recover());
  } else if (cmd == "drv") {
    printDriverHealth();
  } else if (cmd == "cfg" || cmd == "settings") {
    printSettings();
  } else if (cmd == "read") {
    doRead(OPT4001::Mode::ONE_SHOT);
  } else if (cmd == "read force") {
    doRead(OPT4001::Mode::ONE_SHOT_FORCED_AUTO);
  } else if (cmd.startsWith("read ")) {
    int count = cmd.substring(5).toInt();
    if (count <= 0 || count > 10000) {
      LOGW("Invalid count");
      return;
    }
    for (int i = 0; i < count; ++i) {
      doRead(OPT4001::Mode::ONE_SHOT);
    }
  } else if (cmd == "flags") {
    OPT4001::Flags flags;
    OPT4001::Status st = device.readFlags(flags);
    if (!st.ok()) {
      printStatus(st);
      return;
    }
    Serial.printf("  Raw: 0x%04X overload=%s ready=%s high=%s low=%s\n",
                  flags.raw,
                  log_bool_str(flags.overload),
                  log_bool_str(flags.conversionReady),
                  log_bool_str(flags.highThreshold),
                  log_bool_str(flags.lowThreshold));
  } else if (cmd.startsWith("mode ")) {
    String token = cmd.substring(5);
    token.trim();
    OPT4001::Status st =
        (token == "cont" || token == "continuous")
            ? device.setMode(OPT4001::Mode::CONTINUOUS)
            : (token == "power" || token == "pd")
                  ? device.setMode(OPT4001::Mode::POWER_DOWN)
                  : OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "Usage: mode [power|cont]");
    printStatus(st);
  } else if (cmd.startsWith("range ")) {
    String token = cmd.substring(6);
    token.trim();
    OPT4001::Range range = OPT4001::Range::AUTO;
    if (token != "auto") {
      int value = token.toInt();
      if (value < 0 || value > 8) {
        LOGW("Usage: range [0..8|auto]");
        return;
      }
      range = static_cast<OPT4001::Range>(value);
    }
    printStatus(device.setRange(range));
  } else if (cmd.startsWith("ctime ")) {
    int value = cmd.substring(6).toInt();
    if (value < 0 || value > 11) {
      LOGW("Usage: ctime [0..11]");
      return;
    }
    printStatus(device.setConversionTime(static_cast<OPT4001::ConversionTime>(value)));
  } else if (cmd.startsWith("qwake ")) {
    printStatus(device.setQuickWake(cmd.substring(6).toInt() != 0));
  } else if (cmd.startsWith("burst ")) {
    printStatus(device.setBurstMode(cmd.substring(6).toInt() != 0));
  } else if (cmd.startsWith("threshold ")) {
    String args = cmd.substring(10);
    int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: threshold <lowLux> <highLux>");
      return;
    }
    float low = 0.0f;
    float high = 0.0f;
    if (!parseFloatToken(args.substring(0, split), low) ||
        !parseFloatToken(args.substring(split + 1), high)) {
      LOGW("Usage: threshold <lowLux> <highLux>");
      return;
    }
    printStatus(device.setThresholdsLux(low, high));
  } else if (cmd.startsWith("reg ")) {
    uint32_t reg = 0;
    if (!parseU32(cmd.substring(4), reg) || reg > 0xFFu) {
      LOGW("Usage: reg <addr>");
      return;
    }
    uint16_t value = 0;
    OPT4001::Status st = device.readRegister16(static_cast<uint8_t>(reg), value);
    if (!st.ok()) {
      printStatus(st);
      return;
    }
    Serial.printf("  Reg 0x%02lX = 0x%04X\n", static_cast<unsigned long>(reg), value);
  } else if (cmd.startsWith("wreg ")) {
    String args = cmd.substring(5);
    int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: wreg <addr> <value>");
      return;
    }
    uint32_t reg = 0;
    uint32_t value = 0;
    if (!parseU32(args.substring(0, split), reg) ||
        !parseU32(args.substring(split + 1), value) ||
        reg > 0xFFu || value > 0xFFFFu) {
      LOGW("Usage: wreg <addr> <value>");
      return;
    }
    printStatus(device.writeRegister16(static_cast<uint8_t>(reg), static_cast<uint16_t>(value)));
  } else if (cmd == "verbose") {
    Serial.printf("  verbose = %s\n", verboseMode ? "ON" : "OFF");
  } else if (cmd.startsWith("verbose ")) {
    verboseMode = cmd.substring(8).toInt() != 0;
    Serial.printf("  verbose = %s\n", verboseMode ? "ON" : "OFF");
  } else if (cmd == "stress" || cmd.startsWith("stress ")) {
    int count = 10;
    if (cmd.length() > 6) {
      count = cmd.substring(7).toInt();
    }
    if (count <= 0 || count > 100000) {
      LOGW("Invalid count");
      return;
    }
    int ok = 0;
    int fail = 0;
    for (int i = 0; i < count; ++i) {
      OPT4001::Sample sample;
      OPT4001::Status st = device.readBlocking(sample, 1500);
      if (st.ok() || st.code == OPT4001::Err::CRC_ERROR) {
        ok++;
        if (verboseMode) {
          printSample(sample);
        }
      } else {
        fail++;
        if (verboseMode) {
          printStatus(st);
        }
      }
    }
    Serial.printf("  stress: ok=%d fail=%d\n", ok, fail);
  } else {
    LOGW("Unknown command: %s", cmd.c_str());
  }
}

void setup() {
  board::initSerial();
  delay(100);

  LOGI("=== OPT4001 Bring-up Example ===");

  if (!board::initI2c()) {
    LOGE("Failed to initialize I2C");
    return;
  }
  board::initIntPin();
  bus_diag::scan();

  OPT4001::Config cfg;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cUser = &Wire;
  cfg.i2cAddress = 0x45;
  cfg.i2cTimeoutMs = board::I2C_TIMEOUT_MS;
  cfg.packageVariant = OPT4001::PackageVariant::SOT_5X3;
  cfg.mode = OPT4001::Mode::POWER_DOWN;
  if (board::INT_PIN >= 0) {
    cfg.intPin = board::INT_PIN;
    cfg.gpioRead = board::readIntPin;
  }

  OPT4001::Status st = device.begin(cfg);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  LOGI("Device initialized");
  printSettings();
  Serial.println();
  Serial.println("Type 'help' for commands.");
  Serial.print("> ");
}

void loop() {
  device.tick(millis());

  static String input;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (input.length() > 0) {
        processCommand(input);
        input = "";
        Serial.print("> ");
      }
    } else {
      input += c;
    }
  }
}
