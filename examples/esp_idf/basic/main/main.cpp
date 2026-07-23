#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "OPT4001/OPT4001.h"
#include "Opt4001IdfI2cTransport.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
constexpr gpio_num_t INT_PIN = GPIO_NUM_NC;
constexpr uint8_t I2C_ADDRESS = 0x45;
constexpr uint32_t I2C_FREQ_HZ = 400000;
constexpr uint32_t I2C_TIMEOUT_MS = 50;
constexpr size_t INPUT_MAX = 192;

OPT4001::OPT4001 device;
Opt4001IdfI2c i2c;
bool verboseMode = false;
bool watchMode = false;

void lowerInPlace(char* text) {
  for (; text != nullptr && *text != '\0'; ++text) {
    *text = static_cast<char>(tolower(static_cast<unsigned char>(*text)));
  }
}

char* nextToken(char** save) {
  return strtok_r(nullptr, " \t", save);
}

bool parseU32(const char* text, uint32_t& out) {
  if (text == nullptr || text[0] == '\0') return false;
  char* end = nullptr;
  const unsigned long value = strtoul(text, &end, 0);
  if (end == text || *end != '\0') return false;
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

const char* errToStr(OPT4001::Err err) {
  switch (err) {
    case OPT4001::Err::OK: return "OK";
    case OPT4001::Err::NOT_INITIALIZED: return "NOT_INITIALIZED";
    case OPT4001::Err::INVALID_CONFIG: return "INVALID_CONFIG";
    case OPT4001::Err::I2C_ERROR: return "I2C_ERROR";
    case OPT4001::Err::TIMEOUT: return "TIMEOUT";
    case OPT4001::Err::INVALID_PARAM: return "INVALID_PARAM";
    case OPT4001::Err::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
    case OPT4001::Err::DEVICE_ID_MISMATCH: return "DEVICE_ID_MISMATCH";
    case OPT4001::Err::CRC_ERROR: return "CRC_ERROR";
    case OPT4001::Err::MEASUREMENT_NOT_READY: return "MEASUREMENT_NOT_READY";
    case OPT4001::Err::BUSY: return "BUSY";
    case OPT4001::Err::IN_PROGRESS: return "IN_PROGRESS";
    case OPT4001::Err::I2C_NACK_ADDR: return "I2C_NACK_ADDR";
    case OPT4001::Err::I2C_NACK_DATA: return "I2C_NACK_DATA";
    case OPT4001::Err::I2C_TIMEOUT: return "I2C_TIMEOUT";
    case OPT4001::Err::I2C_BUS: return "I2C_BUS";
    case OPT4001::Err::OFFLINE: return "OFFLINE";
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

bool sampleStatusHasData(const OPT4001::Status& st) {
  return st.ok() || st.code == OPT4001::Err::CRC_ERROR;
}

void printStatus(OPT4001::Status st) {
  printf("  Status: %s (code=%u, detail=%ld)\n", errToStr(st.code),
         static_cast<unsigned>(st.code), static_cast<long>(st.detail));
  if (st.msg != nullptr && st.msg[0] != '\0') printf("  Message: %s\n", st.msg);
}

OPT4001::Range parseRange(uint32_t value) {
  return value == 12 ? OPT4001::Range::AUTO : static_cast<OPT4001::Range>(value);
}

OPT4001::ConversionTime parseConversionTime(uint32_t value) {
  if (value == 600) return OPT4001::ConversionTime::US_600;
  if (value == 1) return OPT4001::ConversionTime::MS_1;
  if (value == 2) return OPT4001::ConversionTime::MS_1_8;
  if (value == 3) return OPT4001::ConversionTime::MS_3_4;
  if (value == 6) return OPT4001::ConversionTime::MS_6_5;
  if (value == 12) return OPT4001::ConversionTime::MS_12_7;
  if (value == 25) return OPT4001::ConversionTime::MS_25;
  if (value == 50) return OPT4001::ConversionTime::MS_50;
  if (value == 100) return OPT4001::ConversionTime::MS_100;
  if (value == 200) return OPT4001::ConversionTime::MS_200;
  if (value == 400) return OPT4001::ConversionTime::MS_400;
  if (value == 800) return OPT4001::ConversionTime::MS_800;
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
  cfg.i2cAddress = I2C_ADDRESS;
  cfg.i2cTimeoutMs = I2C_TIMEOUT_MS;
  cfg.packageVariant = OPT4001::PackageVariant::SOT_5X3;
  cfg.intPin = INT_PIN == GPIO_NUM_NC ? -1 : static_cast<int>(INT_PIN);
  cfg.gpioRead = INT_PIN == GPIO_NUM_NC ? nullptr : opt4001IdfGpioRead;
  cfg.gpioUser = &i2c;
  cfg.offlineThreshold = 5;
  return cfg;
}

void printHelp() {
  puts("\n=== OPT4001 native ESP-IDF CLI ===");
  puts("Common: help ? version ver scan verbose <0|1> init begin end drv online probe recover reset resetreapply");
  puts("Data: read lux mlux ulux sample sampleage start ready flags status status_raw flags_raw clearflags burst fifo");
  puts("Config: cfg settings snapshot addr range <0..8|12> ctime <600|1|2|3|6|12|25|50|100|200|400|800>");
  puts("Config: mode <0..3> quickwake <0|1> crc <0|1> latch <0|1> pol <0|1> fault <0..3>");
  puts("Interrupts: int ready|fifo|threshold|dir|pin threshold raw <low> <high> threshold lux <low> <high>");
  puts("Registers: id config intcfg reg <addr> rreg <addr> wreg <addr> <value> regs <start> <len> raw <exp> <mant>");
  puts("Tools: scale diag selftest stress [n] stress_mix [n] watch demo");
}

void printHealth() {
  printf("Driver: state=%s online=%s consec=%u ok=%lu fail=%lu lastOk=%lu lastErr=%lu\n",
         stateToStr(device.state()), device.isOnline() ? "yes" : "no",
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
  const OPT4001::Status st = device.readSample(sample);
  if (st.ok() || st.code == OPT4001::Err::CRC_ERROR) printSample(sample);
  printStatus(st);
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
  printf("Settings: init=%s state=%s addr=0x%02X range=%u ctime=%u mode=%u quick=%s crc=%s sample=%s age=%lu\n",
         s.initialized ? "yes" : "no", stateToStr(s.state), s.i2cAddress,
         static_cast<unsigned>(s.range), static_cast<unsigned>(s.conversionTime),
         static_cast<unsigned>(s.mode), s.quickWake ? "yes" : "no",
         s.verifyCrc ? "yes" : "no", s.hasSample ? "yes" : "no",
         static_cast<unsigned long>(device.sampleAgeMs(opt4001IdfNowMs(&i2c))));
}

void dumpRegisters(uint8_t start, size_t len) {
  if (len > 64) len = 64;
  uint8_t data[64] = {};
  const OPT4001::Status st = device.readRegisters(start, data, len);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  for (size_t i = 0; i < len; ++i) {
    printf("  [0x%02X] = 0x%02X\n", static_cast<unsigned>(start + i), data[i]);
  }
}

void runStress(uint32_t count) {
  uint32_t ok = 0;
  uint32_t warn = 0;
  uint32_t fail = 0;
  for (uint32_t i = 0; i < count; ++i) {
    OPT4001::Sample sample;
    const OPT4001::Status st = device.readBlocking(sample, 1000);
    if (st.ok()) ++ok;
    else if (st.code == OPT4001::Err::CRC_ERROR) ++warn;
    else ++fail;
    if (!st.ok() && verboseMode) printStatus(st);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  printf("Stress: ok=%lu warn=%lu fail=%lu\n", static_cast<unsigned long>(ok),
         static_cast<unsigned long>(warn), static_cast<unsigned long>(fail));
  printHealth();
}

void processCommand(char* line) {
  char* save = nullptr;
  char* cmd = strtok_r(line, " \t", &save);
  if (cmd == nullptr) return;
  lowerInPlace(cmd);

  if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
    printHelp();
  } else if (strcmp(cmd, "version") == 0 || strcmp(cmd, "ver") == 0) {
    printf("Version: %s\n", OPT4001::VERSION_FULL);
  } else if (strcmp(cmd, "scan") == 0) {
    scanI2c();
  } else if (strcmp(cmd, "verbose") == 0) {
    bool value = false;
    if (parseBool(nextToken(&save), value)) verboseMode = value;
    printf("Verbose: %s\n", verboseMode ? "ON" : "OFF");
  } else if (strcmp(cmd, "init") == 0 || strcmp(cmd, "begin") == 0) {
    device.end();
    printStatus(device.begin(makeConfig()));
    printHealth();
  } else if (strcmp(cmd, "end") == 0) {
    device.end();
    puts("Driver state: UNINIT");
  } else if (strcmp(cmd, "drv") == 0 || strcmp(cmd, "online") == 0) {
    printHealth();
  } else if (strcmp(cmd, "probe") == 0) {
    printStatus(device.probe());
  } else if (strcmp(cmd, "recover") == 0) {
    printStatus(device.recover());
  } else if (strcmp(cmd, "reset") == 0) {
    printStatus(device.softReset());
  } else if (strcmp(cmd, "resetreapply") == 0) {
    printStatus(device.resetAndReapply());
  } else if (strcmp(cmd, "read") == 0 || strcmp(cmd, "lux") == 0) {
    float lux = 0.0f;
    const auto st = device.readBlockingLux(lux, 1000);
    if (sampleStatusHasData(st)) printf("Lux: %.6f\n", lux);
    printStatus(st);
  } else if (strcmp(cmd, "mlux") == 0) {
    uint32_t value = 0;
    const auto st = device.readMilliLux(value);
    if (sampleStatusHasData(st)) printf("Milli-lux: %lu\n", static_cast<unsigned long>(value));
    printStatus(st);
  } else if (strcmp(cmd, "ulux") == 0) {
    uint64_t value = 0;
    const auto st = device.readMicroLux(value);
    if (sampleStatusHasData(st)) printf("Micro-lux: %llu\n", static_cast<unsigned long long>(value));
    printStatus(st);
  } else if (strcmp(cmd, "sample") == 0) {
    OPT4001::Sample sample;
    const auto st = device.getLastSample(sample);
    if (st.ok()) printSample(sample);
    printStatus(st);
  } else if (strcmp(cmd, "sampleage") == 0) {
    printf("Sample age: %lu ms\n", static_cast<unsigned long>(device.sampleAgeMs(opt4001IdfNowMs(&i2c))));
  } else if (strcmp(cmd, "start") == 0) {
    printStatus(device.startConversion());
  } else if (strcmp(cmd, "ready") == 0) {
    bool ready = false;
    const auto st = device.conversionReady(ready);
    if (st.ok()) {
      printf("Ready: %s\n", ready ? "yes" : "no");
    } else {
      printStatus(st);
    }
  } else if (strcmp(cmd, "flags") == 0 || strcmp(cmd, "status") == 0) {
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
  } else if (strcmp(cmd, "burst") == 0 || strcmp(cmd, "fifo") == 0) {
    OPT4001::BurstFrame frame;
    const auto st = device.readBurst(frame);
    if (sampleStatusHasData(st)) {
      printSample(frame.newest);
      printSample(frame.fifo0);
      printSample(frame.fifo1);
      printSample(frame.fifo2);
    }
    printStatus(st);
  } else if (strcmp(cmd, "cfg") == 0 || strcmp(cmd, "settings") == 0 || strcmp(cmd, "snapshot") == 0) {
    printSettings();
  } else if (strcmp(cmd, "range") == 0 || strcmp(cmd, "ctime") == 0 || strcmp(cmd, "mode") == 0) {
    uint32_t value = 0;
    if (!parseU32(nextToken(&save), value)) { puts("  Expected value"); return; }
    if (strcmp(cmd, "range") == 0) printStatus(device.setRange(parseRange(value)));
    if (strcmp(cmd, "ctime") == 0) printStatus(device.setConversionTime(parseConversionTime(value)));
    if (strcmp(cmd, "mode") == 0) printStatus(device.setMode(static_cast<OPT4001::Mode>(value)));
  } else if (strcmp(cmd, "quickwake") == 0 || strcmp(cmd, "crc") == 0) {
    bool value = false; if (!parseBool(nextToken(&save), value)) return;
    strcmp(cmd, "quickwake") == 0 ? printStatus(device.setQuickWake(value)) : printStatus(device.setVerifyCrc(value));
  } else if (strcmp(cmd, "latch") == 0 || strcmp(cmd, "pol") == 0 || strcmp(cmd, "fault") == 0) {
    uint32_t value = 0; if (!parseU32(nextToken(&save), value)) return;
    if (strcmp(cmd, "latch") == 0) printStatus(device.setInterruptLatch(value ? OPT4001::InterruptLatch::LATCHED : OPT4001::InterruptLatch::TRANSPARENT));
    if (strcmp(cmd, "pol") == 0) printStatus(device.setInterruptPolarity(value ? OPT4001::InterruptPolarity::ACTIVE_HIGH : OPT4001::InterruptPolarity::ACTIVE_LOW));
    if (strcmp(cmd, "fault") == 0) printStatus(device.setFaultCount(static_cast<OPT4001::FaultCount>(value)));
  } else if (strcmp(cmd, "int") == 0) {
    char* arg = nextToken(&save);
    if (arg == nullptr) {
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
    } else if (strcmp(arg, "threshold") == 0) {
      printStatus(device.setIntConfig(OPT4001::IntConfig::THRESHOLD));
    } else if (strcmp(arg, "dir") == 0) {
      uint32_t value = 1; parseU32(nextToken(&save), value);
      printStatus(device.setIntDirection(value ? OPT4001::IntDirection::PIN_OUTPUT : OPT4001::IntDirection::PIN_INPUT));
    } else if (strcmp(arg, "pin") == 0) {
      bool asserted = false; const auto st = device.readIntPinAsserted(asserted);
      if (st.ok()) {
        printf("INT asserted: %s\n", asserted ? "yes" : "no");
      } else {
        printStatus(st);
      }
    }
  } else if (strcmp(cmd, "threshold") == 0) {
    char* mode = nextToken(&save);
    if (mode != nullptr && strcmp(mode, "raw") == 0) {
      uint32_t low = 0, high = 0;
      if (!parseU32(nextToken(&save), low) || !parseU32(nextToken(&save), high)) return;
      printStatus(device.setThresholds({static_cast<uint8_t>((low >> 12) & 0x0F), static_cast<uint16_t>(low & 0x0FFF)},
                                       {static_cast<uint8_t>((high >> 12) & 0x0F), static_cast<uint16_t>(high & 0x0FFF)}));
    } else if (mode != nullptr && strcmp(mode, "lux") == 0) {
      float low = 0.0f, high = 0.0f;
      if (!parseF32(nextToken(&save), low) || !parseF32(nextToken(&save), high)) return;
      printStatus(device.setThresholdsLux(low, high));
    } else {
      float low = 0.0f, high = 0.0f;
      const auto st = device.getThresholdsLux(low, high);
      if (st.ok()) {
        printf("Thresholds: low=%.6f high=%.6f lx\n", low, high);
      } else {
        printStatus(st);
      }
    }
  } else if (strcmp(cmd, "id") == 0) {
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
    uint16_t raw = 0;
    const auto st = strcmp(cmd, "config") == 0 ? device.readConfiguration(raw) : device.readIntConfiguration(raw);
    if (st.ok()) {
      printf("%s=0x%04X\n", cmd, raw);
    } else {
      printStatus(st);
    }
  } else if (strcmp(cmd, "reg") == 0 || strcmp(cmd, "rreg") == 0) {
    uint32_t reg = 0; if (!parseU32(nextToken(&save), reg)) return;
    uint16_t value = 0; const auto st = device.readRegister16(static_cast<uint8_t>(reg), value);
    if (st.ok()) {
      printf("[0x%02lX]=0x%04X\n", static_cast<unsigned long>(reg), value);
    } else {
      printStatus(st);
    }
  } else if (strcmp(cmd, "wreg") == 0) {
    uint32_t reg = 0, value = 0; if (!parseU32(nextToken(&save), reg) || !parseU32(nextToken(&save), value)) return;
    printStatus(device.writeRegister16(static_cast<uint8_t>(reg), static_cast<uint16_t>(value)));
  } else if (strcmp(cmd, "regs") == 0) {
    uint32_t start = 0, len = 16; parseU32(nextToken(&save), start); parseU32(nextToken(&save), len);
    dumpRegisters(static_cast<uint8_t>(start), len);
  } else if (strcmp(cmd, "raw") == 0) {
    uint32_t exp = 0, mant = 0;
    if (parseU32(nextToken(&save), exp) && parseU32(nextToken(&save), mant)) {
      printf("Lux: %.6f\n", device.rawToLux(static_cast<uint8_t>(exp), mant));
    } else {
      readAndPrintSample();
    }
  } else if (strcmp(cmd, "scale") == 0 || strcmp(cmd, "diag") == 0) {
    printf("Scale: lsb=%.9f fullscale=%.3f resolution=%.9f bits=%u\n",
           device.getLuxLsb(), device.getCurrentFullScaleLux(),
           device.getCurrentResolutionLux(), device.getEffectiveBits());
    printHealth();
  } else if (strcmp(cmd, "selftest") == 0) {
    puts("Selftest:");
    printStatus(device.probe());
    printHealth();
  } else if (strcmp(cmd, "stress") == 0 || strcmp(cmd, "stress_mix") == 0) {
    uint32_t count = strcmp(cmd, "stress_mix") == 0 ? 50 : 10;
    parseU32(nextToken(&save), count);
    runStress(count);
  } else if (strcmp(cmd, "watch") == 0 || strcmp(cmd, "demo") == 0) {
    watchMode = !watchMode;
    printf("Watch: %s\n", watchMode ? "ON" : "OFF");
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
  static char input[INPUT_MAX];
  size_t len = 0;
  printf("> ");
  while (true) {
    device.tick(opt4001IdfNowMs(&i2c));
    if (watchMode && device.isOnline()) {
      readAndPrintSample();
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    const int c = getchar();
    if (c == EOF) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if (c == '\b' || c == 0x7F) {
      if (len > 0) --len;
      continue;
    }
    if (c == '\n' || c == '\r') {
      if (len > 0) {
        input[len] = '\0';
        processCommand(input);
        len = 0;
        printf("> ");
      }
      continue;
    }
    if (len < sizeof(input) - 1) input[len++] = static_cast<char>(c);
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
