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
  - per-slot history reads (`slot 0` = newest, `1-3` = FIFO shadows)
  - per-sample CRC verification
- Configuration support:
  - range selection or auto-range
  - conversion-time selection
  - quick wake
  - threshold registers
  - interrupt polarity, latch, fault count, direction, function, and burst mode
- Diagnostics:
  - raw register access
  - raw contiguous register-block reads
  - probe without health side effects
  - tracked recover path
  - decoded device-ID / configuration / INT-configuration helpers
  - cached settings snapshot
  - full-scale, effective-bit, resolution, and counter-delta utility helpers

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
- `softReset()` uses the documented general-call reset (`0x00` / `0x06`).
  That reset is bus-wide; use it only when the bus topology allows it.
- `resetAndReapply()` exists for the same workflow used in the stronger sibling
  libraries: reset the device, then restore the cached configuration.
- `readSample()`, `readBurst()`, and the blocking helpers may return `CRC_ERROR`
  while still populating the decoded sample data.
- `readSampleSlot(0..3)` provides direct access to the newest sample plus the
  three FIFO shadow samples without forcing a full burst decode.
- `getLastSample()` / `sampleTimestampMs()` / `sampleAgeMs()` provide RAM-only
  access to the last successfully decoded sample.
- `readFlags()` reads register `0x0C`; that register is clear-on-read on the device.
- `clearConversionReadyFlag()` performs the datasheet's write-nonzero clear of
  only `CONVERSION_READY_FLAG`, while `clearFlags()` uses the clear-on-read path
  to clear the full sticky status view.
- `writeIntConfiguration()` verifies the fixed register pattern documented for `0x0B`
  before writing.
- Decoded register helpers (`DeviceIdInfo`, `ConfigurationInfo`,
  `IntConfigurationInfo`) are available so bring-up code does not need to unpack
  bit fields manually.

## Public Surface

### Lifecycle And Diagnostics

- `begin(const Config&)`
- `tick(uint32_t nowMs)`
- `end()`
- `probe()`
- `recover()`
- `softReset()`
- `resetAndReapply()`
- `readDeviceId(uint16_t&)`
- `readDeviceId(DeviceIdInfo&)`
- `getSettings(SettingsSnapshot&)`

### Measurements

- `startConversion()`
- `startConversion(Mode mode)`
- `conversionReady()`
- `readSample(Sample&)`
- `readBurst(BurstFrame&)`
- `readSampleSlot(slot, Sample&)`
- `getLastSample(Sample&)`
- `sampleTimestampMs()`
- `sampleAgeMs(nowMs)`
- `readLux(float&)`
- `readMilliLux(uint32_t&)`
- `readMicroLux(uint64_t&)`
- `readBlocking(...)`
- `readBlockingLux(...)`

### Configuration And Raw Access

- `setRange()`, `setConversionTime()`, `setMode()`, `setQuickWake()`, `setVerifyCrc()`
- `setInterruptLatch()`, `setInterruptPolarity()`, `setFaultCount()`
- `setIntDirection()`, `setIntConfig()`, `setBurstMode()`
- `setThresholds()`, `getThresholds()`, `setThresholdsLux()`
- `getThresholdsLux()`
- `readConfiguration(...)`, `writeConfiguration()`
- `readIntConfiguration(...)`, `writeIntConfiguration()`
- `readFlags()`, `readFlagsRaw()`, `clearConversionReadyFlag()`, `clearFlags()`
- `readRegisters()`, `readRegister16()`, `writeRegister16()`

### Decode And Scaling Helpers

- `decodeDeviceId()`, `decodeConfiguration()`, `decodeIntConfiguration()`
- `thresholdToLux()`, `thresholdToAdcCodes()`
- `getRangeFullScaleLux()`, `getCurrentFullScaleLux()`, `getSampleFullScaleLux()`
- `getEffectiveBits()`
- `getRangeResolutionLux()`, `getCurrentResolutionLux()`, `getSampleResolutionLux()`
- `sampleCounterDelta()`

## Examples

- `examples/01_basic_bringup_cli/`
  - interactive bring-up shell
  - scan, probe, recover, reset, reset-and-reapply, and runtime address selection
  - decoded config / intcfg / flags / device-ID readback
  - one-shot reads, burst FIFO reads, single-slot history reads, cached-sample inspection, stress/selftest
  - lux / milli-lux / micro-lux commands plus scale / timing diagnostics
  - threshold and interrupt configuration, plus raw register and raw block access
- `examples/common/`
  - board config and serial logging helpers
  - I2C transport adapter and bus scanner
  - reusable CLI parsing / diagnostics glue

### CLI Notes

- `reset` performs the datasheet's general-call reset and is therefore bus-wide.
- `config`, `intcfg`, `flags`, `reg`, and `wreg` are intended for bring-up and
  diagnostics; raw writes can desynchronize the cached config until `recover()`
  or `resetAndReapply()` is used.
- `flags readyclear` uses the write-to-clear-ready path, while `flags` and
  `flags clear` use the register read path that also clears latched threshold flags.
- The example defaults to the SOT-5X3 package path. For PicoStar, switch the
  package variant and leave the INT hook disabled.

## Documentation

- `docs/OPT4001_datasheet.md` - register map, timing notes, formulas, and behavior summary
- `docs/AN_light_detection.md` - application notes relevant to threshold use
- `docs/AN_high_speed_resolution.md` - high-speed / resolution trade-offs
- `docs/AN_picostar_package.md` - PicoStar-specific package differences
- `include/OPT4001/CommandTable.h` - public register constants and masks
- `ASSUMPTIONS.md` - implementation choices made where the device notes needed interpretation

## Limits

- High-speed I2C entry sequencing is transport-owned and not modeled in the driver.
- SMBus alert response arbitration is controller-level bus behavior and is not
  wrapped as a dedicated driver API.
- INT-input hardware triggering is left to the board/application layer; the
  driver exposes the configuration bits but does not generate GPIO trigger pulses.
- Window transmission compensation and similar application-note calibration
  factors are intentionally left at the application layer rather than baked into
  the core lux conversion path.

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
