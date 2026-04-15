/// @file main.cpp
/// @brief OPT4001 basic bring-up CLI example.
/// @note This is an EXAMPLE, not part of the library.

#include <Arduino.h>
#include <cstdlib>

#include "examples/common/BoardConfig.h"
#include "examples/common/BusDiag.h"
#include "examples/common/I2cScanner.h"
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
  switch (state) {
    case OPT4001::DriverState::UNINIT: return "UNINIT";
    case OPT4001::DriverState::READY: return "READY";
    case OPT4001::DriverState::DEGRADED: return "DEGRADED";
    case OPT4001::DriverState::OFFLINE: return "OFFLINE";
    default: return "UNKNOWN";
  }
}

const char* modeToStr(OPT4001::Mode mode) {
  switch (mode) {
    case OPT4001::Mode::POWER_DOWN: return "POWER_DOWN";
    case OPT4001::Mode::ONE_SHOT_FORCED_AUTO: return "ONE_SHOT_FORCED_AUTO";
    case OPT4001::Mode::ONE_SHOT: return "ONE_SHOT";
    case OPT4001::Mode::CONTINUOUS: return "CONTINUOUS";
    default: return "UNKNOWN";
  }
}

const char* packageToStr(OPT4001::PackageVariant variant) {
  return (variant == OPT4001::PackageVariant::PICOSTAR) ? "PICOSTAR" : "SOT_5X3";
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

bool parseF32(const String& token, float& out) {
  char* end = nullptr;
  const float value = strtof(token.c_str(), &end);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = value;
  return true;
}

OPT4001::Config makeDefaultConfig() {
  OPT4001::Config cfg;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cUser = &Wire;
  cfg.i2cAddress = OPT4001::cmd::I2C_ADDR_DEFAULT;
  cfg.i2cTimeoutMs = board::I2C_TIMEOUT_MS;
  cfg.packageVariant = OPT4001::PackageVariant::SOT_5X3;
  cfg.mode = OPT4001::Mode::POWER_DOWN;
  cfg.offlineThreshold = 5;
  if (board::INT_PIN >= 0) {
    cfg.intPin = board::INT_PIN;
    cfg.gpioRead = board::readIntPin;
  }
  return cfg;
}

void printStatus(const OPT4001::Status& st) {
  Serial.printf("  Status: %s%s%s (code=%u detail=%ld)\n",
                LOG_COLOR_RESULT(st.ok()),
                errToStr(st.code),
                LOG_COLOR_RESET,
                static_cast<unsigned>(st.code),
                static_cast<long>(st.detail));
  if (st.msg && st.msg[0]) {
    Serial.printf("  Message: %s\n", st.msg);
  }
}

void printSample(const OPT4001::Sample& s) {
  Serial.printf("  Lux=%.6f lx adc=%lu exp=%u mant=0x%05lX ctr=%u crc=0x%X(%s)\n",
                s.lux,
                static_cast<unsigned long>(s.adcCodes),
                s.exponent,
                static_cast<unsigned long>(s.mantissa),
                s.counter,
                s.crc,
                s.crcValid ? "OK" : "BAD");
}

void printVersion() {
  Serial.println("=== Version Info ===");
  Serial.printf("  Example build: %s %s\n", __DATE__, __TIME__);
  Serial.printf("  Library version: %s\n", OPT4001::VERSION);
  Serial.printf("  Library full: %s\n", OPT4001::VERSION_FULL);
}

void printHealth() {
  Serial.println("=== Driver Health ===");
  Serial.printf("  State: %s\n", stateToStr(device.state()));
  Serial.printf("  Online: %s\n", log_bool_str(device.isOnline()));
  Serial.printf("  Total success/fail: %lu / %lu\n",
                static_cast<unsigned long>(device.totalSuccess()),
                static_cast<unsigned long>(device.totalFailures()));
  Serial.printf("  Consecutive failures: %u\n", device.consecutiveFailures());
  Serial.printf("  Last OK / error: %lu / %lu ms\n",
                static_cast<unsigned long>(device.lastOkMs()),
                static_cast<unsigned long>(device.lastErrorMs()));
  if (!device.lastError().ok()) {
    Serial.printf("  Last error: %s\n", errToStr(device.lastError().code));
  }
}

void printSnapshot() {
  OPT4001::SettingsSnapshot snap;
  device.getSettings(snap);
  Serial.println("=== Cached Settings ===");
  Serial.printf("  Initialized: %s\n", log_bool_str(snap.initialized));
  Serial.printf("  State: %s\n", stateToStr(snap.state));
  Serial.printf("  Package: %s\n", packageToStr(snap.packageVariant));
  Serial.printf("  Address: 0x%02X\n", snap.i2cAddress);
  Serial.printf("  Mode / pending: %s / %s\n", modeToStr(snap.mode), modeToStr(snap.pendingMode));
  Serial.printf("  Quick wake / CRC / burst: %s / %s / %s\n",
                log_bool_str(snap.quickWake),
                log_bool_str(snap.verifyCrc),
                log_bool_str(snap.burstMode));
  Serial.printf("  Sample valid / ts: %s / %lu ms\n",
                log_bool_str(snap.lastSampleValid),
                static_cast<unsigned long>(snap.sampleTimestampMs));
  Serial.printf("  Last lux: %.6f lx\n", snap.lastLux);
}

void printLiveConfig() {
  if (!device.isInitialized()) {
    LOGW("Driver not initialized.");
    return;
  }
  uint16_t did = 0, cfg = 0, intcfg = 0;
  OPT4001::Threshold low, high;
  OPT4001::Status st = device.readDeviceId(did);
  if (!st.ok()) { printStatus(st); return; }
  st = device.readConfiguration(cfg);
  if (!st.ok()) { printStatus(st); return; }
  st = device.readIntConfiguration(intcfg);
  if (!st.ok()) { printStatus(st); return; }
  st = device.getThresholds(low, high);
  if (!st.ok()) { printStatus(st); return; }
  Serial.println("=== Live Device Registers ===");
  Serial.printf("  DEVICE_ID=0x%04X CONFIG=0x%04X INTCFG=0x%04X\n", did, cfg, intcfg);
  Serial.printf("  Thresholds low=(%u,0x%03X) high=(%u,0x%03X)\n",
                low.exponent, low.result, high.exponent, high.result);
}

void printRegisterDump() {
  if (!device.isInitialized()) {
    LOGW("Driver not initialized.");
    return;
  }

  struct DumpReg {
    uint8_t addr;
    const char* name;
    bool clearOnRead;
  };

  static constexpr DumpReg kRegs[] = {
    {OPT4001::cmd::REG_RESULT, "RESULT", false},
    {OPT4001::cmd::REG_RESULT_LSB_CRC, "RESULT_LSB_CRC", false},
    {OPT4001::cmd::REG_FIFO0_MSB, "FIFO0_MSB", false},
    {OPT4001::cmd::REG_FIFO0_LSB_CRC, "FIFO0_LSB_CRC", false},
    {OPT4001::cmd::REG_FIFO1_MSB, "FIFO1_MSB", false},
    {OPT4001::cmd::REG_FIFO1_LSB_CRC, "FIFO1_LSB_CRC", false},
    {OPT4001::cmd::REG_FIFO2_MSB, "FIFO2_MSB", false},
    {OPT4001::cmd::REG_FIFO2_LSB_CRC, "FIFO2_LSB_CRC", false},
    {OPT4001::cmd::REG_THRESHOLD_L, "THRESHOLD_L", false},
    {OPT4001::cmd::REG_THRESHOLD_H, "THRESHOLD_H", false},
    {OPT4001::cmd::REG_CONFIGURATION, "CONFIGURATION", false},
    {OPT4001::cmd::REG_INT_CONFIGURATION, "INT_CONFIGURATION", false},
    {OPT4001::cmd::REG_FLAGS, "FLAGS", true},
    {OPT4001::cmd::REG_DEVICE_ID, "DEVICE_ID", false},
  };

  Serial.println("=== Register Dump ===");
  for (const DumpReg& reg : kRegs) {
    uint16_t value = 0;
    OPT4001::Status st = device.readRegister16(reg.addr, value);
    if (!st.ok()) {
      Serial.printf("  0x%02X %-18s : <error>\n", reg.addr, reg.name);
      printStatus(st);
      return;
    }
    Serial.printf("  0x%02X %-18s : 0x%04X%s\n",
                  reg.addr,
                  reg.name,
                  value,
                  reg.clearOnRead ? "  (clear-on-read)" : "");
  }
}

bool blockingRead(OPT4001::Mode mode) {
  OPT4001::Sample sample;
  OPT4001::Status st = device.readBlocking(sample, mode, 1500);
  if (!st.ok() && st.code != OPT4001::Err::CRC_ERROR) {
    printStatus(st);
    return false;
  }
  printSample(sample);
  if (!st.ok()) {
    printStatus(st);
  }
  return true;
}

void runSelfTest() {
  struct Stats { uint32_t pass = 0, fail = 0, skip = 0; } stats;
  auto report = [&](const char* name, bool pass, const char* note) {
    Serial.printf("  [%s%s%s] %s", LOG_COLOR_RESULT(pass), pass ? "PASS" : "FAIL", LOG_COLOR_RESET, name);
    if (note && note[0]) Serial.printf(" - %s", note);
    Serial.println();
    if (pass) stats.pass++; else stats.fail++;
  };
  Serial.println("=== OPT4001 selftest (safe commands) ===");
  OPT4001::Status pst = device.probe();
  if (pst.code == OPT4001::Err::NOT_INITIALIZED) {
    Serial.println("  [SKIP] driver not initialized");
    return;
  }
  report("probe responds", pst.ok(), pst.ok() ? "" : errToStr(pst.code));
  uint16_t did = 0;
  OPT4001::Status st = device.readDeviceId(did);
  report("readDeviceId", st.ok(), st.ok() ? "" : errToStr(st.code));
  report("device id matches", st.ok() && ((did & OPT4001::cmd::MASK_DIDH) == OPT4001::cmd::DIDH_EXPECTED), "");
  OPT4001::Sample sample;
  st = device.readBlocking(sample, 1500);
  report("readBlocking", st.ok() || st.code == OPT4001::Err::CRC_ERROR, "");
  OPT4001::Sample cached;
  st = device.getLastSample(cached);
  report("getLastSample", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.clearFlags();
  report("clearFlags", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.recover();
  report("recover", st.ok(), st.ok() ? "" : errToStr(st.code));
  Serial.printf("Selftest result: pass=%lu fail=%lu skip=%lu\n",
                static_cast<unsigned long>(stats.pass),
                static_cast<unsigned long>(stats.fail),
                static_cast<unsigned long>(stats.skip));
}

void printHelp() {
  auto section = [](const char* title) { Serial.printf("\n%s[%s]%s\n", LOG_COLOR_GREEN, title, LOG_COLOR_RESET); };
  auto item = [](const char* cmd, const char* desc) { Serial.printf("  %s%-28s%s - %s\n", LOG_COLOR_CYAN, cmd, LOG_COLOR_RESET, desc); };
  Serial.println();
  Serial.printf("%s=== OPT4001 CLI Help ===%s\n", LOG_COLOR_CYAN, LOG_COLOR_RESET);
  section("Common");
  item("help / ?", "Show this help");
  item("version / ver", "Print version info");
  item("scan", "Scan I2C bus");
  item("init", "Reinitialize device");
  item("end", "Shut down driver");
  section("Data");
  item("read", "Blocking one-shot read");
  item("read force", "Blocking forced-auto read");
  item("read N", "Run N blocking reads");
  item("readblocking [force]", "Explicit blocking alias");
  item("start [force]", "Start one-shot conversion");
  item("poll", "Check conversion ready");
  item("readburst [force]", "Read RESULT + FIFO frame");
  item("sample / sampleage", "Cached sample and age");
  section("Configuration");
  item("cfg / settings", "Show live config and cache");
  item("snapshot", "Show cached settings only");
  item("id / identify", "Read device ID");
  item("mode [power|cont]", "Set or show stable mode");
  item("range [0..8|auto]", "Set or show range");
  item("ctime [0..11]", "Set or show conversion time");
  item("qwake [0|1]", "Set or show quick wake");
  item("crc [0|1]", "Set or show host-side CRC verification");
  item("pkg [pico|sot]", "Set or show package variant");
  item("burst [0|1]", "Set or show I2C burst mode");
  item("threshold [low high]", "Read or set thresholds in lux");
  item("int latch|pol|faults|dir|cfg ...", "Interrupt configuration");
  section("Registers");
  item("config / intcfg", "Read raw config registers");
  item("config write <hex>", "Write full CONFIGURATION");
  item("intcfg write <hex>", "Write full INT_CONFIGURATION");
  item("flags / flags raw / flags clear", "Read or clear FLAGS");
  item("dump", "Dump key registers");
  item("reg <addr>", "Read 16-bit register");
  item("wreg <addr> <val>", "Write 16-bit register");
  section("Diagnostics");
  item("drv", "Show driver health");
  item("probe", "Probe without health tracking");
  item("recover", "Manual recovery");
  item("reset", "General-call reset (bus-wide)");
  item("resetreapply", "General-call reset + re-apply");
  item("verbose [0|1]", "Toggle verbose output");
  item("stress [N]", "Run blocking read stress");
  item("stress_mix [N]", "Run mixed-operation stress");
  item("selftest", "Run safe command self-test");
}

void processCommand(const String& cmdLine) {
  String cmd = cmdLine;
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "help" || cmd == "?") { printHelp(); return; }
  if (cmd == "version" || cmd == "ver") { printVersion(); return; }
  if (cmd == "scan") { bus_diag::scan(); return; }
  if (cmd == "init") { device.end(); printStatus(device.begin(makeDefaultConfig())); return; }
  if (cmd == "end") { device.end(); LOGI("Driver state: UNINIT"); return; }
  if (cmd == "drv") { printHealth(); return; }
  if (cmd == "probe") { printStatus(device.probe()); return; }
  if (cmd == "recover") { printStatus(device.recover()); return; }
  if (cmd == "reset") { LOGW("Issuing general-call reset (bus-wide)."); printStatus(device.softReset()); return; }
  if (cmd == "resetreapply") { LOGW("Issuing general-call reset + re-apply."); printStatus(device.resetAndReapply()); return; }
  if (cmd == "cfg" || cmd == "settings") { printLiveConfig(); printSnapshot(); return; }
  if (cmd == "snapshot") { printSnapshot(); return; }
  if (cmd == "id" || cmd == "identify") { uint16_t v = 0; OPT4001::Status st = device.readDeviceId(v); if (!st.ok()) printStatus(st); else Serial.printf("  Device ID: 0x%04X\n", v); return; }
  if (cmd == "config") { uint16_t v = 0; OPT4001::Status st = device.readConfiguration(v); if (!st.ok()) printStatus(st); else Serial.printf("  CONFIG=0x%04X\n", v); return; }
  if (cmd.startsWith("config write ")) { uint32_t v = 0; if (!parseU32(cmd.substring(13), v) || v > 0xFFFFu) LOGW("Usage: config write <0..0xFFFF>"); else printStatus(device.writeConfiguration(static_cast<uint16_t>(v))); return; }
  if (cmd == "intcfg") { uint16_t v = 0; OPT4001::Status st = device.readIntConfiguration(v); if (!st.ok()) printStatus(st); else Serial.printf("  INTCFG=0x%04X\n", v); return; }
  if (cmd.startsWith("intcfg write ")) { uint32_t v = 0; if (!parseU32(cmd.substring(13), v) || v > 0xFFFFu) LOGW("Usage: intcfg write <0..0xFFFF>"); else printStatus(device.writeIntConfiguration(static_cast<uint16_t>(v))); return; }
  if (cmd == "flags") { OPT4001::Flags f; OPT4001::Status st = device.readFlags(f); if (!st.ok()) printStatus(st); else Serial.printf("  FLAGS=0x%04X ovl=%s ready=%s hi=%s lo=%s\n", f.raw, log_bool_str(f.overload), log_bool_str(f.conversionReady), log_bool_str(f.highThreshold), log_bool_str(f.lowThreshold)); return; }
  if (cmd == "flags raw") { uint16_t v = 0; OPT4001::Status st = device.readRegister16(OPT4001::cmd::REG_FLAGS, v); if (!st.ok()) printStatus(st); else Serial.printf("  FLAGS raw=0x%04X (clear-on-read)\n", v); return; }
  if (cmd == "flags clear") { printStatus(device.clearFlags()); return; }
  if (cmd == "dump") { printRegisterDump(); return; }
  if (cmd == "read") { (void)blockingRead(OPT4001::Mode::ONE_SHOT); return; }
  if (cmd == "read force") { (void)blockingRead(OPT4001::Mode::ONE_SHOT_FORCED_AUTO); return; }
  if (cmd == "readblocking") { (void)blockingRead(OPT4001::Mode::ONE_SHOT); return; }
  if (cmd == "readblocking force") { (void)blockingRead(OPT4001::Mode::ONE_SHOT_FORCED_AUTO); return; }
  if (cmd.startsWith("read ")) { int count = cmd.substring(5).toInt(); if (count <= 0 || count > 10000) LOGW("Invalid count (1-10000)"); else for (int i = 0; i < count; ++i) if (!blockingRead(OPT4001::Mode::ONE_SHOT)) break; return; }
  if (cmd == "start") { printStatus(device.startConversion()); return; }
  if (cmd == "start force") { printStatus(device.startConversion(OPT4001::Mode::ONE_SHOT_FORCED_AUTO)); return; }
  if (cmd == "poll") { Serial.printf("  Ready: %s\n", device.conversionReady() ? "YES" : "no"); return; }
  if (cmd == "readburst" || cmd == "readburst force") { OPT4001::Mode mode = (cmd.endsWith("force")) ? OPT4001::Mode::ONE_SHOT_FORCED_AUTO : OPT4001::Mode::ONE_SHOT; if (device.getMode() == OPT4001::Mode::POWER_DOWN) (void)blockingRead(mode); OPT4001::BurstFrame f; OPT4001::Status st = device.readBurst(f); if (!st.ok() && st.code != OPT4001::Err::CRC_ERROR) printStatus(st); else { printSample(f.newest); printSample(f.fifo0); printSample(f.fifo1); printSample(f.fifo2); if (!st.ok()) printStatus(st); } return; }
  if (cmd == "sample") { OPT4001::Sample s; OPT4001::Status st = device.getLastSample(s); if (!st.ok()) printStatus(st); else printSample(s); return; }
  if (cmd == "sampleage") { Serial.printf("  Sample age: %lu ms\n", static_cast<unsigned long>(device.sampleAgeMs(millis()))); return; }
  if (cmd == "mode") { Serial.printf("  Mode: %s\n", modeToStr(device.getMode())); return; }
  if (cmd.startsWith("mode ")) { String t = cmd.substring(5); t.trim(); printStatus((t == "cont" || t == "continuous") ? device.setMode(OPT4001::Mode::CONTINUOUS) : (t == "power" || t == "pd") ? device.setMode(OPT4001::Mode::POWER_DOWN) : OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "Usage: mode [power|cont]")); return; }
  if (cmd == "range") { Serial.printf("  Range: %u\n", static_cast<unsigned>(device.getRange())); return; }
  if (cmd.startsWith("range ")) { String t = cmd.substring(6); t.trim(); OPT4001::Range r = OPT4001::Range::AUTO; if (t != "auto") { int v = t.toInt(); if (v < 0 || v > 8) { LOGW("Usage: range [0..8|auto]"); return; } r = static_cast<OPT4001::Range>(v); } printStatus(device.setRange(r)); return; }
  if (cmd == "ctime") { Serial.printf("  Conversion time: %u\n", static_cast<unsigned>(device.getConversionTime())); return; }
  if (cmd.startsWith("ctime ")) { int v = cmd.substring(6).toInt(); if (v < 0 || v > 11) LOGW("Usage: ctime [0..11]"); else printStatus(device.setConversionTime(static_cast<OPT4001::ConversionTime>(v))); return; }
  if (cmd == "qwake") { Serial.printf("  Quick wake: %s\n", log_bool_str(device.getQuickWake())); return; }
  if (cmd.startsWith("qwake ")) { printStatus(device.setQuickWake(cmd.substring(6).toInt() != 0)); return; }
  if (cmd == "crc") { Serial.printf("  Verify CRC: %s\n", log_bool_str(device.getVerifyCrc())); return; }
  if (cmd.startsWith("crc ")) { printStatus(device.setVerifyCrc(cmd.substring(4).toInt() != 0)); return; }
  if (cmd == "pkg") { Serial.printf("  Package: %s\n", packageToStr(device.getPackageVariant())); return; }
  if (cmd.startsWith("pkg ")) { String t = cmd.substring(4); t.trim(); OPT4001::PackageVariant v = (t == "pico" || t == "picostar") ? OPT4001::PackageVariant::PICOSTAR : (t == "sot" || t == "sot_5x3") ? OPT4001::PackageVariant::SOT_5X3 : static_cast<OPT4001::PackageVariant>(0xFF); if (v != OPT4001::PackageVariant::PICOSTAR && v != OPT4001::PackageVariant::SOT_5X3) LOGW("Usage: pkg [pico|sot]"); else printStatus(device.setPackageVariant(v)); return; }
  if (cmd == "burst") { Serial.printf("  Burst mode: %s\n", log_bool_str(device.getBurstMode())); return; }
  if (cmd.startsWith("burst ")) { printStatus(device.setBurstMode(cmd.substring(6).toInt() != 0)); return; }
  if (cmd == "threshold") { printLiveConfig(); return; }
  if (cmd.startsWith("threshold ")) { String args = cmd.substring(10); args.trim(); int sp = args.indexOf(' '); if (sp < 0) { LOGW("Usage: threshold <lowLux> <highLux>"); return; } float low = 0.0f, high = 0.0f; if (!parseF32(args.substring(0, sp), low) || !parseF32(args.substring(sp + 1), high)) LOGW("Usage: threshold <lowLux> <highLux>"); else printStatus(device.setThresholdsLux(low, high)); return; }
  if (cmd.startsWith("int latch ")) { int v = cmd.substring(10).toInt(); if (v != 0 && v != 1) LOGW("Usage: int latch [0|1]"); else printStatus(device.setInterruptLatch(v == 0 ? OPT4001::InterruptLatch::TRANSPARENT : OPT4001::InterruptLatch::LATCHED)); return; }
  if (cmd.startsWith("int pol ")) { String t = cmd.substring(8); t.trim(); printStatus((t == "low") ? device.setInterruptPolarity(OPT4001::InterruptPolarity::ACTIVE_LOW) : (t == "high") ? device.setInterruptPolarity(OPT4001::InterruptPolarity::ACTIVE_HIGH) : OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "Usage: int pol [low|high]")); return; }
  if (cmd.startsWith("int faults ")) { String t = cmd.substring(11); t.trim(); OPT4001::FaultCount f = (t == "1") ? OPT4001::FaultCount::FAULTS_1 : (t == "2") ? OPT4001::FaultCount::FAULTS_2 : (t == "4") ? OPT4001::FaultCount::FAULTS_4 : (t == "8") ? OPT4001::FaultCount::FAULTS_8 : static_cast<OPT4001::FaultCount>(0xFF); if (f != OPT4001::FaultCount::FAULTS_1 && f != OPT4001::FaultCount::FAULTS_2 && f != OPT4001::FaultCount::FAULTS_4 && f != OPT4001::FaultCount::FAULTS_8) LOGW("Usage: int faults [1|2|4|8]"); else printStatus(device.setFaultCount(f)); return; }
  if (cmd.startsWith("int dir ")) { String t = cmd.substring(8); t.trim(); OPT4001::IntDirection d = (t == "in" || t == "input") ? OPT4001::IntDirection::PIN_INPUT : (t == "out" || t == "output") ? OPT4001::IntDirection::PIN_OUTPUT : static_cast<OPT4001::IntDirection>(0xFF); if (d != OPT4001::IntDirection::PIN_INPUT && d != OPT4001::IntDirection::PIN_OUTPUT) LOGW("Usage: int dir [in|out]"); else printStatus(device.setIntDirection(d)); return; }
  if (cmd.startsWith("int cfg ")) { String t = cmd.substring(8); t.trim(); OPT4001::IntConfig c = (t == "threshold" || t == "thresh") ? OPT4001::IntConfig::THRESHOLD : (t == "conv" || t == "every") ? OPT4001::IntConfig::EVERY_CONVERSION : (t == "fifo" || t == "full") ? OPT4001::IntConfig::FIFO_FULL : static_cast<OPT4001::IntConfig>(0xFF); if (c != OPT4001::IntConfig::THRESHOLD && c != OPT4001::IntConfig::EVERY_CONVERSION && c != OPT4001::IntConfig::FIFO_FULL) LOGW("Usage: int cfg [threshold|conv|fifo]"); else printStatus(device.setIntConfig(c)); return; }
  if (cmd == "int") { uint16_t v = 0; OPT4001::Status st = device.readIntConfiguration(v); if (!st.ok()) printStatus(st); else Serial.printf("  INTCFG=0x%04X latch=%u pol=%u faults=%u\n", v, static_cast<unsigned>(device.getInterruptLatch()), static_cast<unsigned>(device.getInterruptPolarity()), static_cast<unsigned>(device.getFaultCount())); return; }
  if (cmd.startsWith("reg ")) { uint32_t a = 0; if (!parseU32(cmd.substring(4), a) || a > 0xFFu) LOGW("Usage: reg <addr>"); else { uint16_t v = 0; OPT4001::Status st = device.readRegister16(static_cast<uint8_t>(a), v); if (!st.ok()) printStatus(st); else Serial.printf("  Reg 0x%02lX = 0x%04X (%u)\n", static_cast<unsigned long>(a), v, v); } return; }
  if (cmd.startsWith("wreg ")) { String args = cmd.substring(5); args.trim(); int sp = args.indexOf(' '); uint32_t a = 0, v = 0; if (sp < 0 || !parseU32(args.substring(0, sp), a) || !parseU32(args.substring(sp + 1), v) || a > 0xFFu || v > 0xFFFFu) LOGW("Usage: wreg <addr> <val>"); else printStatus(device.writeRegister16(static_cast<uint8_t>(a), static_cast<uint16_t>(v))); return; }
  if (cmd == "verbose") { Serial.printf("  Verbose: %s\n", verboseMode ? "ON" : "OFF"); return; }
  if (cmd.startsWith("verbose ")) { verboseMode = cmd.substring(8).toInt() != 0; Serial.printf("  Verbose: %s\n", verboseMode ? "ON" : "OFF"); return; }
  if (cmd == "selftest") { runSelfTest(); return; }
  if (cmd == "stress_mix" || cmd.startsWith("stress_mix ")) { int count = (cmd == "stress_mix") ? 50 : cmd.substring(11).toInt(); if (count <= 0 || count > 100000) LOGW("Invalid count (1-100000)"); else { for (int i = 0; i < count; ++i) { if ((i % 3) == 0) { OPT4001::Sample s; OPT4001::Status st = device.readBlocking(s, 1500); if (!st.ok() && st.code != OPT4001::Err::CRC_ERROR && verboseMode) printStatus(st); } else if ((i % 3) == 1) { uint16_t v = 0; OPT4001::Status st = device.readConfiguration(v); if (!st.ok() && verboseMode) printStatus(st); } else { uint16_t v = 0; OPT4001::Status st = device.readDeviceId(v); if (!st.ok() && verboseMode) printStatus(st); } } Serial.printf("  stress_mix complete: %d iterations\n", count); } return; }
  if (cmd == "stress" || cmd.startsWith("stress ")) { int count = (cmd == "stress") ? 10 : cmd.substring(7).toInt(); if (count <= 0 || count > 100000) LOGW("Invalid count (1-100000)"); else for (int i = 0; i < count; ++i) if (!blockingRead(OPT4001::Mode::ONE_SHOT)) break; return; }
  LOGW("Unknown command: %s", cmd.c_str());
}

void setup() {
  board::initSerial();
  delay(100);
  LOGI("=== OPT4001 Bring-up Example ===");
  if (!board::initI2c()) {
    LOGE("Failed to initialize I2C");
    return;
  }
  LOGI("I2C initialized (SDA=%d, SCL=%d)", board::I2C_SDA, board::I2C_SCL);
  board::initIntPin();
  bus_diag::scan();
  OPT4001::Status st = device.begin(makeDefaultConfig());
  if (!st.ok()) {
    LOGE("Failed to initialize device");
    printStatus(st);
  } else {
    LOGI("Device initialized successfully");
    printHealth();
  }
  Serial.println("\nType 'help' for commands");
  Serial.print("> ");
}

void loop() {
  device.tick(millis());
  static String inputBuffer;
  static constexpr size_t kMaxInputLen = 128;
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
        Serial.print("> ");
      }
    } else if (inputBuffer.length() < kMaxInputLen) {
      inputBuffer += c;
    }
  }
}
