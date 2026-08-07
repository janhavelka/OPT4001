# OPT4001 Driver Library

Production-oriented, source-level hardened OPT4001 ambient light sensor
driver with a framework-neutral core. Current evidence covers native
fake-transport tests and Arduino/PlatformIO ESP32-S2/S3 builds. A native ESP-IDF
component and diagnostic example are present, and CI is configured to build the
pure ESP-IDF example. Local pure ESP-IDF, hardware, optical, INT, FIFO
timing/order, address-pin, and fault/recovery validation remain pending unless
captured separately.

The library follows the same non-owning I2C transport and `Status`-returning API
pattern used by the other device libraries in this workspace:

- no direct `Wire` dependency in library code
- framework-neutral core with no Arduino or ESP-IDF driver headers in `include/` or `src/`
- deterministic control flow with bounded polling in `tick()`
- health tracking with `READY`, `DEGRADED`, and `OFFLINE` states
- stable `errorName()` / `driverStateName()` strings for diagnostics shared by
  Arduino and native ESP-IDF integrations
- no heap allocation in steady-state driver operation

## Current Readiness

Classification: source-level hardened and diagnostic-build tested. The core is
designed for production integration, but this repository does not currently
claim real-device hardware validation, optical accuracy validation,
INT/FIFO/address-pin/fault-recovery validation, or completed pure ESP-IDF
target-build evidence.

### Validation Evidence

| Area | Evidence currently captured |
| --- | --- |
| Core portability | `tools/check_core_timing_guard.py` enforces no Arduino/ESP-IDF framework APIs in `include/` or `src/`. |
| Public contracts | `tools/check_public_api_docs.py` checks lifecycle, freshness, FIFO CRC, dirty-state, blocking, INT/FLAGS, and transport documentation tokens. |
| Claims/metadata | `tools/check_readiness_claims.py` checks unsupported readiness claims, CI guard coverage, and supported-version metadata. |
| Native behavior | `python -m platformio test -e native` runs fake-transport unit coverage. |
| Arduino ESP32 builds | `python -m platformio run -e esp32s3dev` and `python -m platformio run -e esp32s2dev` build the Arduino diagnostic example. |
| Packaging | `python -m platformio pkg pack` verifies PlatformIO package metadata. |
| ESP-IDF static contract | `tools/check_idf_example_contract.py` verifies the native IDF example boundary and command coverage. |
| Pure ESP-IDF builds | CI job configured with Espressif's ESP-IDF action; local attempts failed because `idf.py` was not available on `PATH`. Treat as pending until a completed CI/local build log is captured. |

### Pending Validation Matrix

| Area | Current status |
| --- | --- |
| Hardware bring-up on real OPT4001 | Pending hardware-validation log with board, package, wiring, firmware, and measurement evidence. |
| Optical accuracy / cover-glass correction | Pending application-specific validation. |
| SOT-5X3 address-pin combinations | Pending hardware matrix. |
| INT threshold, every-conversion, and FIFO-full pulses | Pending logic-analyzer or board-captured evidence. |
| FIFO physical timing/order under real conversions | Pending hardware matrix. |
| SMBus alert arbitration | Pending controller-level validation. |
| Fault/recovery paths | Pending controlled hardware/HIL validation for NACK, timeout, unplug/replug, brownout, stuck bus, OFFLINE latch, and manual `recover()`. |
| Pure ESP-IDF `idf.py` builds | CI configured; local and captured workflow evidence pending. |

Hardware validation procedure: `docs/validation/hardware-validation-procedure.md`.

### Package, Address, And Electrical Matrix

| Package variant | Valid addresses | Lux LSB | INT / ADDR pins |
| --- | --- | --- | --- |
| PicoStar | `0x45` only | `312.5e-6 lux/code` | No INT pin, no ADDR pin |
| SOT-5X3 | `0x44`, `0x45`, `0x46` | `437.5e-6 lux/code` | ADDR pin plus optional open-drain INT |

Power OPT4001 from the datasheet VDD range, 1.6 V to 3.6 V. Digital I/O pins
are 5.5 V tolerant; that does not make the device a 5 V powered part.

## Features

- OPT4001 package support:
  - PicoStar variant with fixed address `0x45`, package-specific lux scale,
    no ADDR pin, and no INT pin
  - SOT-5X3 variant with selectable addresses `0x44`, `0x45`, `0x46`,
    package-specific lux scale, ADDR pin support, and optional INT support
- Operating modes:
  - power-down
  - continuous conversion
  - one-shot conversion
  - one-shot forced auto-range conversion
- Measurement support:
  - decoded exponent / mantissa sample format
  - lux, milli-lux, and micro-lux helpers
  - 4-sample burst read (`RESULT` newest + FIFO0..FIFO2 shadows)
  - per-slot history reads (`slot 0` = newest, `1-3` = FIFO shadows)
  - per-sample CRC fields with aggregate read status
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

This repository pins its Arduino builds to pioarduino `55.03.311` and native
tests to `platformio/native@1.2.1`. On Windows, repository validation uses the
checked-in `scripts\pio.cmd` wrapper so it selects the existing VS Code-managed
PlatformIO Core.

Add to `platformio.ini`:

The examples below intentionally use the latest published tag (`v1.0.0`). The
audited source metadata is `1.1.0`, but this hardening change is not a formal
release and does not create a tag.

```ini
lib_deps =
  https://github.com/janhavelka/OPT4001.git#v1.0.0
```

After registry publication, the equivalent registry form is:

```ini
lib_deps =
  OPT4001@^1.0.0
```

### Manual

Copy `include/OPT4001/` and `src/` into your project. The generated
`include/OPT4001/Version.h` header is intentionally tracked in this repository,
so `#include "OPT4001/OPT4001.h"` works from a clean checkout without running a
pre-generation step.

### ESP-IDF

The repository root is an ESP-IDF component. A local project can add it through
`EXTRA_COMPONENT_DIRS`:

```cmake
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../OPT4001")
```

Then provide `Config::i2cWrite`, `Config::i2cWriteRead`, `Config::nowMs`,
optional `Config::cooperativeYield`, and optional `Config::gpioRead` from your
application-owned adapter. The `examples/esp_idf/basic` project is a diagnostic
bring-up CLI with comparable command coverage to the Arduino example. It owns
its example I2C bus, uses ESP-IDF-native GPIO, timer, VFS console, and new I2C
master-driver glue, and does not demonstrate production shared-bus locking.

### Version Header

`library.json` is the version source of truth. `include/OPT4001/Version.h` is
generated by `scripts/generate_version.py` and committed so manual, CMake, and
ESP-IDF consumers can include the public header directly from a clean checkout.
Do not edit it manually.

```bash
python scripts/generate_version.py check
python tools/check_version_header_contract.py
```

PlatformIO builds still inject build date, time, git commit, and git status
through compiler defines. Non-PlatformIO builds use stable `"unknown"` defaults
for those optional metadata fields unless the application supplies its own
defines.

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
  cfg.nowMs = [](void*) { return millis(); };
  cfg.cooperativeYield = [](void*) { yield(); };
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
- The driver is not internally synchronized or ISR-safe. Call public APIs from one
  task or protect the shared driver instance and transport with an application
  lock. ISRs should only signal work to a task; they should not call driver methods.
- Transport callbacks must not call back into the same driver instance. On a
  shared bus, the application lock must cover both the driver call and the
  callback transaction.
- Public APIs that can perform I2C require a successful `begin()` unless their
  header contract explicitly says otherwise. While `UNINIT`, public raw-register
  APIs return `NOT_INITIALIZED` without touching the bus.
- `Config.mode` accepts only `POWER_DOWN` or `CONTINUOUS`.
  One-shot measurements are started explicitly with `startConversion()` or
  `readBlocking()`.
- Forced auto-range one-shots have a longer wait budget than normal one-shots:
  conversion time plus standby wake time when quick-wake is disabled plus the
  forced-auto-range extra time. Use `getOneShotBudget*()`, not
  `getConversionTime*()`, when sizing one-shot deadlines.
- `softReset()` uses the documented general-call reset (`0x00` / `0x06`).
  That reset is bus-wide; use it only when the bus topology allows it.
- ESP-IDF reset support requires the application adapter to support the
  general-call write address `0x00`; the IDF CLI exposes `reset` and
  `resetreapply`, but the address-`0x00` path still needs target-IDF and
  target-hardware validation before production use.
- `resetAndReapply()` exists for the same workflow used in the stronger sibling
  libraries: reset the device, then restore the cached configuration.
- `probe()` reads `DEVICE_ID` register `0x11` through the raw configured
  transport without updating health counters. Success means the register pattern
  is exactly `0x0121`: fixed bits `[15:14] = 0`, `DIDL[13:12] = 0`, and
  `DIDH[11:0] = 0x121`. This is identity-register validation, not optical or
  measurement-path validation.
- `probe()` and `begin()` preserve transport diagnostics instead of collapsing
  them to `DEVICE_NOT_FOUND`. A successful I2C transaction with the wrong full
  ID pattern returns `DEVICE_ID_MISMATCH` with the raw register value in
  `Status::detail`.
- `readLatestSample()` reads the current output registers without freshness proof.
  It is diagnostic/latest-register access and may return duplicate samples.
- `readSample()`, `readBurst()`, lux reads, `tryReadSample()` /
  `tryReadFreshSample()`, and blocking helpers are fresh-sample APIs. Fresh
  means conversion-ready flag evidence, configured SOT-5X3 INT assertion, or a
  sample-counter advance from the previous accepted fresh sample.
- The sample counter is modulo 16. `sampleCounterDelta()` is wrap-aware, but
  counter-only freshness cannot detect a missed full modulo-16 wrap; poll faster
  than 16 conversions when every conversion matters.
- Fresh sample APIs may return `CRC_ERROR` while still populating the decoded
  sample data; that sample is consumed as fresh and cached.
- Blocking read helpers require `Config::nowMs`; it must be monotonic and
  advance in milliseconds. `timeoutMs` must cover `getOneShotBudgetMs(mode)`
  plus transport and scheduler margin. The finite poll cap prevents an infinite
  loop, but it is not a replacement clock.
- `readLux()`, `readMilliLux()`, `readMicroLux()`, and `tryReadLux()` follow the
  same rule: on `CRC_ERROR`, the scaled output value is still written.
- `readBurst()` returns `BurstFrame::newest` from `RESULT`, followed by
  `fifo0`, `fifo1`, and `fifo2` from the FIFO shadow register pairs. Treat that
  order as newest to oldest within the four-deep history window.
- Each burst/history `Sample` carries its own received CRC nibble and
  `crcValid` flag. The returned `Status` is aggregate: `OK` means the decoded
  slots passed the selected CRC policy, while `CRC_ERROR` means at least one
  decoded slot failed verification. After a successful register transfer,
  `readBurst()` decodes all four slots even when one or more slots fail CRC.
- `readSampleSlot(0..3)` provides direct access to the newest sample plus the
  three FIFO shadow samples without forcing a full burst decode. Slot 0 consumes
  freshness like `readSample()`; slots 1-3 are direct history reads.
- For TunnelMonitor-style shared `I2cTask` integration, prefer `readBurst()` as
  the low-level sample primitive. It exposes newest plus FIFO history in one
  fixed RESULT/FIFO transfer when burst mode is enabled and preserves raw
  exponent, mantissa, ADC code, counter, CRC, and lux fields for owner-side
  policy.
- Poll-chunked jobs are available through `startReadBurst()`,
  `startReadSample()`, `startConfigureMeasurement()`, and
  `startResetAndReapply()`, then `poll(nowMs, maxInstructions)`. One register
  read/write or one burst RESULT/FIFO block read is one instruction; CRC decode,
  lux conversion, cache updates, and delay gates do not count. `FLAGS` reads
  are explicit instructions because the register is clear-on-read.
- `getLastSample()` / `sampleTimestampMs()` / `sampleAgeMs()` provide RAM-only
  access to the last successfully decoded sample.
- `getSettings(SettingsSnapshot&)` is also RAM/cache-only. It does not probe,
  read registers, or hide live I2C behind a settings snapshot.
- `tryReadSample()` / `tryReadFreshSample()` / `tryReadLux()` are intended for
  cooperative polling loops: they return `OK` with `didRead=false` when no fresh
  sample is ready yet, so the application does not have to treat
  `MEASUREMENT_NOT_READY` as control flow. I2C or register-poll failures are
  still returned as errors.
- `OFFLINE` is latched. After the health threshold is reached, normal public I2C
  operations return `OFFLINE` with `"Driver is offline; call recover()"` without
  touching the bus. Use `recover()` to probe and re-apply cached configuration,
  or use the explicit reset / reset-and-reapply diagnostics when a bus-wide
  reset is acceptable.
- A dirty hardware/cache state means the driver's cached settings may no longer
  describe the sensor registers. Typical causes are raw register writes,
  external/general-call resets, brownout, or a failed multi-register sequence
  after some earlier writes reached the device. The recovery recipe is to stop
  relying on threshold, INT, and freshness assumptions, call `recover()` to
  probe and re-apply cached configuration, then read back configuration,
  INT-configuration, and thresholds if the application needs confirmation. If a
  bus-wide reset is acceptable, `resetAndReapply()` is the stronger recipe.
- `end()` is best-effort: when initialized and online it attempts a raw
  power-down write, ignores that write status, clears runtime state, and leaves
  the driver `UNINIT`.
- `configureMeasurement()` applies range, conversion time, quick-wake, and the
  stable operating mode in one coherent update while still using the same cached
  config model and injected transport.
- Cached configuration setters roll back the cached state if the required I2C
  write sequence fails.
- `readFlags()` and `readFlagsRaw()` read register `0x0C`, which is clear-on-read.
  Use them only when consuming/clearing the latched FLAGS view is intended. Raw
  register reads of `0x0C`, or raw register blocks that include `0x0C`, have the
  same hardware side effect.
- `clearConversionReadyFlag()` performs the datasheet's write-nonzero clear of
  only `CONVERSION_READY_FLAG`, while `clearFlags()` intentionally uses the
  destructive read path to clear the full sticky status view.
- `writeIntConfiguration()` verifies the fixed register pattern documented for `0x0B`
  before writing.
- Decoded register helpers (`DeviceIdInfo`, `ConfigurationInfo`,
  `IntConfigurationInfo`) are available so bring-up code does not need to unpack
  bit fields manually.
- Interrupt preset helpers (`enableThresholdInterrupt*()`,
  `enableConversionReadyInterrupt()`, `enableFifoFullInterrupt()`) are
  convenience wrappers over the existing register model; they do not take
  ownership of GPIOs, alert handling, ISRs, or the I2C transport.
- INT is SOT-5X3-only and is an open-drain signal. The driver can read a
  configured GPIO hook and apply interrupt polarity, but it never configures,
  reserves, attaches interrupts to, debounces, drives, or owns the pin. The
  application owns pullups, GPIO mode, ISR attachment, ISR-to-task signaling,
  and pin lifetime. PicoStar users should leave INT disabled.
- INT output modes are threshold/SMBus alert, pulse after every conversion, and
  pulse after every four conversions for FIFO-full indication. The reserved
  `INT_CFG=2` value is rejected. `INT_DIR=0` makes INT a one-shot trigger input,
  so it cannot simultaneously be used as an interrupt output.
- The threshold-interrupt convenience helpers reject inverted windows
  (`low > high`) up front; the lower-level raw threshold setters remain available
  when an application really needs exact register control.
- Threshold and INT register packing are covered by native tests, but end-to-end
  threshold flag, SMBus alert, INT pulse, and ISR behavior still require
  target-hardware validation before production reliance.
- Threshold lux helpers reject negative, NaN, and infinite inputs before packing
  threshold registers.
- Raw register access is bounded to documented public registers; blocks that span
  reserved gaps are rejected before touching the bus.

### Numeric And Electrical Contract

Package selection controls both lux scaling and valid I2C addresses:

| Package variant | Valid addresses | Lux LSB | INT |
| --- | --- | --- | --- |
| PicoStar | `0x45` only | `312.5e-6 lux/code` | Not available |
| SOT-5X3 | `0x44`, `0x45`, `0x46` | `437.5e-6 lux/code` | Optional application-owned GPIO |

Power the device from the datasheet VDD range, 1.6 V to 3.6 V. The digital I/O
pins are 5.5 V tolerant, but that tolerance is not permission to power the
device from a 5 V rail.

Result samples use a 4-bit exponent and 20-bit mantissa from `RESULT` /
`RESULT_LSB_CRC`. Public raw conversion helpers accept only exponent `0..8` and
mantissa `0x00000..0xFFFFF`; invalid fields return `INVALID_PARAM` in the
status-returning helpers. Compatibility float helpers return quiet NaN for
invalid raw or threshold inputs. Valid result ADC codes are computed with
64-bit intermediates as `mantissa << exponent` before scaling to lux.

Threshold registers use 4-bit exponent and 12-bit result fields. The exact
linear threshold code is `result << (8 + exponent)` and can exceed `uint32_t`,
so `thresholdToAdcCodes(threshold, uint64_t&)` is the lossless API. The legacy
`thresholdToAdcCodes(threshold)` helper saturates at `UINT32_MAX` instead of
wrapping. `luxToThreshold()` rounds the requested lux to the nearest ADC code,
then quantizes to the register format; it rejects negative, non-finite, and
out-of-range lux values. Packed thresholds are therefore lower-resolution than
raw sample results at small exponents.

CRC verification uses the datasheet XOR equations over `EXPONENT`, 20-bit
`MANTISSA`, and 4-bit `COUNTER`. When verification is enabled, a mismatch
returns `CRC_ERROR` while preserving decoded sample fields and the received CRC
nibble. When verification is disabled, the sample status is `OK` and
`Sample::crcValid` is false.

| Conversion setting | Time us | Ceil ms | Effective bits |
| --- | ---: | ---: | ---: |
| `US_600` | 600 | 1 | 9 |
| `MS_1` | 1000 | 1 | 10 |
| `MS_1_8` | 1800 | 2 | 11 |
| `MS_3_4` | 3400 | 4 | 12 |
| `MS_6_5` | 6500 | 7 | 13 |
| `MS_12_7` | 12700 | 13 | 14 |
| `MS_25` | 25000 | 25 | 15 |
| `MS_50` | 50000 | 50 | 16 |
| `MS_100` | 100000 | 100 | 17 |
| `MS_200` | 200000 | 200 | 18 |
| `MS_400` | 400000 | 400 | 19 |
| `MS_800` | 800000 | 800 | 20 |

Numeric and CRC vector verification uses:

```bash
python -m platformio test -e native
```

### Sample Freshness

| Term / API | Meaning |
| --- | --- |
| Latest sample | Current `RESULT` register contents; may be the same counter/data as a previous read. |
| Fresh sample | Newly observed conversion since the previous accepted fresh sample. Evidence is `CONVERSION_READY_FLAG`, configured SOT-5X3 INT assertion, or counter advance. |
| Cached sample | RAM copy from the last successful latest/fresh decode; use `getLastSample()`, `hasSample()`, `sampleTimestampMs()`, and `sampleAgeMs()`. |
| Not ready | No fresh evidence is available; direct fresh reads return `MEASUREMENT_NOT_READY`, try APIs return `OK` with `didRead=false`, and readiness checks return `ready=false`. |
| `readLatestSample()` | Reads current output registers without freshness proof. |
| `readSample()`, `readBurst()`, `tryReadSample()`, `tryReadFreshSample()`, `readFreshBlocking()` | Fresh-sample APIs that consume readiness on `OK` or `CRC_ERROR`. |
| `readBurst()` | Preferred low-level shared-task sample primitive because it returns raw/integer newest and FIFO history in one fixed burst transfer when burst mode is enabled. |

`tick()` and `conversionReady()` may use elapsed conversion time as a poll gate,
but elapsed time alone is not reported as completion. This matters in auto-range:
overflow can abort a measurement, increase range, and make completion exceed the
nominal conversion-time setting.

### Probe Diagnostics

| Condition during `probe()` / startup | Returned status | Detail field |
| --- | --- | --- |
| Address phase NACK, when the transport can distinguish it | `I2C_NACK_ADDR` | transport code |
| Data phase NACK, when the transport can distinguish it | `I2C_NACK_DATA` | transport code |
| I2C transaction timeout | `I2C_TIMEOUT` | transport code |
| Bus/arbitration/driver-state fault | `I2C_BUS` | transport code |
| Generic transport failure or unknown NACK phase | `I2C_ERROR` | transport code |
| Successful read, but `DEVICE_ID` fixed bits/DIDL/DIDH are wrong | `DEVICE_ID_MISMATCH` | raw `DEVICE_ID` value |

Arduino `Wire` adapters can distinguish address and data NACK during
`endTransmission()`, but read-phase errors may still be generic because
`requestFrom()` reports only how many bytes were read. The ESP-IDF example maps
`ESP_ERR_TIMEOUT` to `I2C_TIMEOUT`, `ESP_ERR_INVALID_RESPONSE` to generic
`I2C_ERROR` because the transaction API does not expose NACK phase, and other
driver/bus state errors to `I2C_BUS` while preserving the raw `esp_err_t` in
`detail`.

## Public Surface

### Lifecycle And Diagnostics

- `begin(const Config&)`
- `tick(uint32_t nowMs)`
- `end()`
- `probe()`
- `recover()`
- `softReset()`
- `resetAndReapply()`
- `startResetAndReapply()`
- `readDeviceId(uint16_t&)`
- `readDeviceId(DeviceIdInfo&)`
- `getSettings(SettingsSnapshot&)`

### Measurements

- `startConversion()`
- `startConversion(Mode mode)`
- `poll(nowMs, maxInstructions)`
- `pollBusy()`
- `lastPollStatus()`
- `startReadSample()`
- `startReadBurst()`
- `getLastBurst(BurstFrame&)`
- `conversionReady()`
- `conversionReady(bool&)`
- `hasSample()`
- `readLatestSample(Sample&)`
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
- `readFreshBlocking(...)`
- `readBlockingLux(...)`
- `tryReadSample(Sample&, bool&)`
- `tryReadFreshSample(Sample&, bool&)`
- `tryReadLux(float&, bool&)`

### Configuration And Raw Access

- `setRange()`, `setConversionTime()`, `setMode()`, `setQuickWake()`, `setVerifyCrc()`
- `configureMeasurement(range, time, mode, quickWake)`
- `startConfigureMeasurement(range, time, mode, quickWake)`
- `setInterruptLatch()`, `setInterruptPolarity()`, `setFaultCount()`
- `setIntDirection()`, `setIntConfig()`, `setBurstMode()`
- `setThresholds()`, `getThresholds()`, `setThresholdsLux()`
- `getThresholdsLux()`, `restoreDefaultThresholds()`
- `enableThresholdInterrupt(...)`, `enableThresholdInterruptLux(...)`
- `enableConversionReadyInterrupt()`, `enableFifoFullInterrupt()`
- `readConfiguration(...)`, `writeConfiguration()`
- `readIntConfiguration(...)`, `writeIntConfiguration()`
- `readFlags()`, `readFlagsRaw()`, `clearConversionReadyFlag()`, `clearFlags()`
- `readIntPinAsserted(bool&)`
- `readRegisters()`, `readRegister16()`, `writeRegister16()`

### Decode And Scaling Helpers

- `decodeDeviceId()`, `decodeConfiguration()`, `decodeIntConfiguration()`
- `adcCodesToLux()`, `rawToAdcCodes()`, `rawToLux()`
- `thresholdToLux()`, `thresholdToAdcCodes()`
- `getRangeFullScaleLux()`, `getCurrentFullScaleLux()`, `getSampleFullScaleLux()`
- `getEffectiveBits()`
- `getRangeResolutionLux()`, `getCurrentResolutionLux()`, `getSampleResolutionLux()`
- `sampleCounterDelta()`

## Examples

- `examples/01_basic_bringup_cli/`
  - interactive bring-up shell
  - scan, probe, recover, reset, reset-and-reapply, compact state view, and runtime address selection
  - decoded config / intcfg / flags / status / device-ID readback with colored health reporting
  - one-shot reads, poll-friendly `tryread` / `trylux`, non-blocking `watch` / `stop`, burst FIFO reads, single-slot history reads, cached-sample inspection, stress, stress-mix, and selftest
  - lux / milli-lux / micro-lux commands plus `adc2lux`, `raw2lux`, scale / timing diagnostics, and the per-range scale table
  - threshold lux helpers, `thcalc`, `thdecode`, `threshold default`, raw threshold programming, interrupt configuration, and raw register / block access
  - measurement and interrupt convenience flows exposed directly in the shell via `measure`, `int ready`, `int fifo`, and `int th`
  - consolidated `diag` report and optional periodic `healthmon` output using the shared health diagnostic helper
- `examples/esp_idf/basic/`
  - native ESP-IDF diagnostic project with `app_main()`, fixed-buffer
    nonblocking CLI input, and `driver/i2c_master.h` transport callbacks
  - colorized, sectioned command coverage matching the Arduino diagnostic
    surface: address/package profiles, blocking/poll-friendly reads, FIFO/slot
    history, watch/stop, measurement and interrupt configuration, decoded
    status/config, threshold math, raw registers, scaling/timing, stress, and
    selftest workflows
  - owns its example I2C bus and intentionally does not show production
    shared-bus locking
  - no Arduino compatibility facade; parity is enforced by
    `tools/check_idf_example_contract.py`
- `examples/common/`
  - board config and serial logging helpers
  - I2C transport adapter and bus scanner
  - reusable CLI parsing / diagnostics glue, including `HealthView.h` and `HealthDiag.h`

### CLI Notes

- `reset` performs the datasheet's general-call reset and is therefore bus-wide.
- `begin` is an alias for `init` / reinitialization. `intpin` reads the
  configured INT GPIO hook and applies the configured interrupt polarity.
- `config`, `intcfg`, `flags`, `reg`, and `wreg` are intended for bring-up and
  diagnostics; raw writes can desynchronize the cached config until `recover()`
  or `resetAndReapply()` is used. Treat that as a dirty hardware/cache state:
  stop relying on cached settings, recover or reset-and-reapply, then read back
  the affected registers.
- `flags readyclear` uses the write-to-clear-ready path, while `flags` and
  `flags clear` use the register read path that also clears latched threshold
  flags. Raw `reg 0x0c` reads have the same clear-on-read side effect.
- `status` / `status_raw` are CLI aliases for the decoded and raw `FLAGS` views.
- `threshold raw <low> <high>` accepts packed 16-bit threshold register values for
  register-level bring-up, while `threshold <lowLux> <highLux>` uses the lux helpers.
- `threshold default` restores the datasheet reset window without requiring the user
  to know the packed reset values.
- `thcalc <lux>` and `thdecode <raw16>` expose the datasheet threshold packing math
  without touching the sensor registers.
- `measure <range|auto> <ctime0..11> <power|cont> [qwake0|1]` is a CLI wrapper over
  `configureMeasurement()` so bring-up sessions can update the coherent measurement
  tuple in one command instead of several register-like steps.
- `watch [count] [intervalMs]` streams decoded samples using the current driver mode.
  In `CONTINUOUS`, it polls the ready path and prints each sampled frame; in
  `POWER_DOWN`, it repeatedly starts one-shot conversions. `watch force ...` uses
  forced auto-range one-shots, and `stop` ends the active watch cleanly with a
  summary.
- `int ready`, `int fifo`, and `int th <lowLux> <highLux>` exercise the interrupt
  preset helpers while preserving the library's non-owning transport/GPIO model.
- `diag` intentionally skips `FLAGS` so the report does not clear the device's
  sticky status bits; use `status` or `status_raw` explicitly when you want that read.
- `healthmon 1 [intervalMs]` enables periodic colorized health reporting from the
  shared example diagnostics helper while the loop keeps running. Use interval `0`
  for change-only reporting.
- The example defaults to the SOT-5X3 package path. For PicoStar, switch the
  package variant and leave the INT hook disabled.

## Documentation

- `docs/README.md` - compact documentation index.
- `docs/integration/esp-idf.md` - ESP-IDF component and example boundary.
- `docs/integration/driver-contracts.md` - lifecycle, health, freshness,
  poll-job, dirty-state, numeric, and CRC contracts.
- `docs/reference/OPT4001_datasheet.md` - register map, timing notes, formulas,
  and behavior summary.
- `docs/reference/AN_light_detection.md` - threshold and light-detection notes.
- `docs/reference/AN_high_speed_resolution.md` - high-speed and resolution
  trade-offs.
- `docs/reference/AN_picostar_package.md` - PicoStar package differences.
- `docs/validation/validation-status.md` - current evidence and pending
  validation.
- `docs/validation/hardware-validation-procedure.md` - hardware evidence
  capture procedure.
- `docs/validation/release-checklist.md` - merge and release checklist.
- `include/OPT4001/CommandTable.h` - public register constants and masks
- `ASSUMPTIONS.md` - implementation choices made where the device notes needed interpretation

## Limits

- High-speed I2C entry sequencing is transport-owned and not modeled in the driver.
- SMBus alert response arbitration is controller-level bus behavior and is not
  wrapped as a dedicated driver API.
- INT-input hardware triggering is left to the board/application layer, but the
  driver can read a configured INT GPIO hook through `readIntPinAsserted()` and
  apply the configured polarity. It does not configure or own GPIO/INT hardware
  and does not generate GPIO trigger pulses.
- Threshold and interrupt helper behavior is contract-tested at the register
  level. Physical threshold comparator behavior, SMBus alert arbitration,
  open-drain pulse timing, and ISR integration remain board-validation items.
- Window transmission compensation and similar application-note calibration
  factors are intentionally left at the application layer rather than baked into
  the core lux conversion path.

## Validation

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/hil_opt4001_runner.py --parser-self-test
python tools/test_hil_opt4001_runner_parser.py
python tools/check_idf_example_contract.py
python tools/check_version_header_contract.py
python tools/check_readiness_claims.py
python tools/check_public_api_docs.py
python scripts/generate_version.py check
.\scripts\pio.cmd test -e native
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s2dev
.\scripts\pio.cmd pkg pack
```

CI is expected to run the same guard/test/build/package command set above. It
also configures a pure ESP-IDF matrix build for `esp32s3` and `esp32s2`; the
static IDF contract check still does not replace reviewing the completed IDF
workflow logs.

Run real ESP-IDF builds when ESP-IDF is installed:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

The static IDF contract check does not replace a real `idf.py` build. Local
release-preparation validation attempted pure ESP-IDF builds, but ESP-IDF was
not available on `PATH`, so local pure ESP-IDF builds were not run.

### Optional Hardware-In-Loop Runner

`tools/hil_opt4001_runner.py` can drive the Arduino or ESP-IDF diagnostic CLI
over a serial port and write bounded Markdown/JSON transcripts under
`hil_logs/`. It does not manipulate power, GPIO fixtures, or stuck-bus hardware.
INT and reset/recovery groups are disabled unless explicitly requested by the
operator.

The runner keeps its OPT4001-specific filename because it drives the repo's two
diagnostic CLI dialects directly. Its smoke plan covers the shared I2C HIL
minimum as OPT4001 commands: `version`, `scan`, `probe`, `id`, settings via
`cfg`, and health via Arduino `state` or ESP-IDF `drv`. `scan` is ACK evidence
only; OPT4001 identity requires the `probe`/`id` DEVICE_ID path.

Use `--parser-self-test` and `tools/test_hil_opt4001_runner_parser.py` before
live hardware. `--dry-run` lists commands only and does not claim hardware PASS.
Live runs can use `--strict-expected` to classify known commands with missing
evidence tokens as `UNKNOWN`, `--verbose` to echo captured transcripts, bounded
`--reconnect-attempts`, and `--group benchmark --benchmark-command <cmd>
--benchmark-count <n>` for simple repeated-command timing.
The default smoke/all-safe plans avoid `FLAGS` because that register is
clear-on-read; use `--group status` only when the session is ready to consume
sticky flag evidence.

Examples:

```bash
python tools/hil_opt4001_runner.py --parser-self-test
python tools/hil_opt4001_runner.py --port COM6 --cli arduino --group smoke
python tools/hil_opt4001_runner.py --port COM6 --cli arduino --group all-safe --strict-expected
python tools/hil_opt4001_runner.py --port /dev/ttyUSB0 --cli idf --group smoke --group fifo
python tools/hil_opt4001_runner.py --port COM6 --cli arduino --group benchmark --benchmark-command read --benchmark-count 50
python tools/hil_opt4001_runner.py --cli arduino --group all-safe --dry-run
```

Use `--include-int` only on SOT-5X3 fixtures with INT wired and instrumented.
Use `--include-fault` only when bus-wide reset and recovery commands are safe
for the connected hardware; it requires
`--confirm-faults I_ACCEPT_BUS_RESET_RISK`.

## License

MIT License. See `LICENSE`.
