# OPT4001 Driver Library

Production-grade OPT4001 ambient light sensor driver for ESP32-S2 / ESP32-S3
using the Arduino framework and PlatformIO.

The library follows the same non-owning I2C transport and `Status`-returning API
pattern used by the other device libraries in this workspace:

- no direct `Wire` dependency in library code
- deterministic control flow with bounded polling in `tick()`
- health tracking with `READY`, `DEGRADED`, and `OFFLINE` states
- no heap allocation in steady-state driver operation

## Features

- OPT4001 package support:
  - PicoStar variant with fixed address `0x45`
  - SOT-5X3 variant with selectable addresses `0x44`, `0x45`, `0x46`
- Operating modes:
  - power-down
  - continuous conversion
  - one-shot conversion
  - one-shot forced auto-range conversion
- Measurement support:
  - decoded exponent / mantissa sample format
  - lux, milli-lux, and micro-lux helpers
  - 4-sample burst read (`RESULT` + FIFO shadows)
  - per-sample CRC verification
- Configuration support:
  - range selection or auto-range
  - conversion-time selection
  - quick wake
  - threshold registers
  - interrupt polarity, latch, fault count, direction, function, and burst mode
- Diagnostics:
  - raw register access
  - probe without health side effects
  - tracked recover path
  - cached settings snapshot

## Installation

### PlatformIO

Add to `platformio.ini`:

```ini
lib_deps =
  OPT4001
```

### Manual

Copy `include/OPT4001/` and `src/` into your project.

## Quick Start

```cpp
#include <Wire.h>
#include "OPT4001/OPT4001.h"

OPT4001::Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                         uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  (void)timeoutMs;
  wire->beginTransmission(addr);
  wire->write(data, len);
  switch (wire->endTransmission(true)) {
    case 0: return OPT4001::Status::Ok();
    case 2: return OPT4001::Status::Error(OPT4001::Err::I2C_NACK_ADDR, "Address NACK");
    case 3: return OPT4001::Status::Error(OPT4001::Err::I2C_NACK_DATA, "Data NACK");
    case 5: return OPT4001::Status::Error(OPT4001::Err::I2C_TIMEOUT, "I2C timeout");
    case 4: return OPT4001::Status::Error(OPT4001::Err::I2C_BUS, "I2C bus error");
    default: return OPT4001::Status::Error(OPT4001::Err::I2C_ERROR, "Write failed");
  }
}

OPT4001::Status i2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                             uint8_t* rx, size_t rxLen,
                             uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  (void)timeoutMs;
  wire->beginTransmission(addr);
  wire->write(tx, txLen);
  const uint8_t result = wire->endTransmission(false);
  if (result != 0) {
    return OPT4001::Status::Error(OPT4001::Err::I2C_ERROR, "Write phase failed", result);
  }
  if (wire->requestFrom(addr, rxLen) != rxLen) {
    return OPT4001::Status::Error(OPT4001::Err::I2C_ERROR, "Read failed");
  }
  for (size_t i = 0; i < rxLen; ++i) {
    rx[i] = wire->read();
  }
  return OPT4001::Status::Ok();
}

OPT4001::OPT4001 sensor;

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);

  OPT4001::Config cfg;
  cfg.i2cWrite = i2cWrite;
  cfg.i2cWriteRead = i2cWriteRead;
  cfg.i2cUser = &Wire;
  cfg.packageVariant = OPT4001::PackageVariant::SOT_5X3;
  cfg.i2cAddress = 0x45;
  cfg.mode = OPT4001::Mode::POWER_DOWN;

  OPT4001::Status st = sensor.begin(cfg);
  if (!st.ok()) {
    Serial.printf("begin() failed: %s\n", st.msg);
    return;
  }

  float lux = 0.0f;
  st = sensor.readBlockingLux(lux, 1500);
  if (!st.ok() && st.code != OPT4001::Err::CRC_ERROR) {
    Serial.printf("readBlockingLux() failed: %s\n", st.msg);
    return;
  }

  Serial.printf("Lux: %.6f\n", lux);
}

void loop() {
  sensor.tick(millis());
}
```

## API Notes

- `begin()` validates the transport, address, package variant, and device ID, then
  applies the cached configuration.
- `Config.mode` accepts only `POWER_DOWN` or `CONTINUOUS`.
  One-shot measurements are started explicitly with `startConversion()` or
  `readBlocking()`.
- `readSample()`, `readBurst()`, and the blocking helpers may return `CRC_ERROR`
  while still populating the decoded sample data.
- `readFlags()` reads register `0x0C`; that register is clear-on-read on the device.
- `writeIntConfiguration()` verifies the fixed register pattern documented for `0x0B`
  before writing.

## Public Surface

### Lifecycle And Diagnostics

- `begin(const Config&)`
- `tick(uint32_t nowMs)`
- `end()`
- `probe()`
- `recover()`
- `getSettings(SettingsSnapshot&)`

### Measurements

- `startConversion()`
- `startConversion(Mode mode)`
- `conversionReady()`
- `readSample(Sample&)`
- `readBurst(BurstFrame&)`
- `readLux(float&)`
- `readMilliLux(uint32_t&)`
- `readMicroLux(uint64_t&)`
- `readBlocking(...)`
- `readBlockingLux(...)`

### Configuration And Raw Access

- `setRange()`, `setConversionTime()`, `setMode()`, `setQuickWake()`
- `setInterruptLatch()`, `setInterruptPolarity()`, `setFaultCount()`
- `setIntDirection()`, `setIntConfig()`, `setBurstMode()`
- `setThresholds()`, `getThresholds()`, `setThresholdsLux()`
- `readConfiguration()`, `writeConfiguration()`
- `readIntConfiguration()`, `writeIntConfiguration()`
- `readFlags()`, `clearFlags()`
- `readRegister16()`, `writeRegister16()`

## Examples

- `examples/01_basic_bringup_cli/`
  - interactive bring-up shell
  - scan, probe, recover, settings dump, raw register access
  - one-shot reads, stress reads, threshold setup
- `examples/common/`
  - board config and serial logging helpers
  - I2C transport adapter and bus scanner
  - reusable CLI parsing / diagnostics glue

## Documentation

- `docs/OPT4001_datasheet.md` - register map, timing notes, formulas, and behavior summary
- `docs/AN_light_detection.md` - application notes relevant to threshold use
- `docs/AN_high_speed_resolution.md` - high-speed / resolution trade-offs
- `docs/AN_picostar_package.md` - PicoStar-specific package differences
- `include/OPT4001/CommandTable.h` - public register constants and masks
- `ASSUMPTIONS.md` - implementation choices made where the device notes needed interpretation

## Validation

```bash
pio test -e native
python tools/check_cli_contract.py
python tools/check_core_timing_guard.py
pio run -e esp32s3dev
pio run -e esp32s2dev
```

## License

MIT License. See `LICENSE`.
