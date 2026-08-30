# OPT4001 Driver Library

A framework-neutral C++ driver for the Texas Instruments OPT4001 ambient light
sensor (PicoStar/YMN and SOT-5X3/DTS packages).

The core in `include/` and `src/` contains no Arduino, ESP-IDF, FreeRTOS or
logging dependency. I2C is injected through application-owned callbacks, so the
same driver works in a small bring-up sketch and as one device inside a larger
firmware that owns a shared I2C bus.

- non-owning I2C transport: the driver never touches `Wire`, pins, or bus timeouts
- every fallible call returns a structured `Status` — no exceptions, no silent failure
- no heap allocation, no `String`, no unbounded loops or `delay()` in the core
- health tracking with `READY` / `DEGRADED` / `OFFLINE` and an explicit `recover()`
- bus-silent `bind()` / `unbind()` plus instruction-budgeted poll jobs for a
  single-owner I2C task
- stable `errorName()` / `driverStateName()` strings for shared diagnostics

## Package, Address, And Electrical Matrix

| Package variant | Valid addresses | Lux LSB | INT / ADDR pins |
| --- | --- | --- | --- |
| PicoStar | `0x45` only | `312.5e-6 lux/code` | No INT pin, no ADDR pin |
| SOT-5X3 | `0x44`, `0x45`, `0x46` | `437.5e-6 lux/code` | ADDR pin plus optional open-drain INT |

Power the OPT4001 from the datasheet VDD range, 1.6 V to 3.6 V. The digital I/O
pins are 5.5 V tolerant; that tolerance is not permission to power the device
from a 5 V rail.

## Features

- **Modes** — power-down, continuous, one-shot, one-shot forced auto-range, quick wake.
- **Measurement** — decoded exponent/mantissa samples, lux / milli-lux / micro-lux,
  4-deep burst read (`RESULT` newest plus FIFO0..FIFO2), per-slot history reads,
  per-sample CRC with aggregate read status.
- **Configuration** — range or auto-range, all 12 conversion times, thresholds
  (raw and lux), interrupt polarity / latch / fault count / direction / function,
  I2C burst mode.
- **Diagnostics** — raw register and register-block access, health-neutral
  `probe()`, tracked `recover()`, decoded device-ID / configuration /
  INT-configuration helpers, cached settings snapshot, full-scale / effective-bit
  / resolution / counter-delta helpers.
- **Integration** — synchronous blocking helpers, poll-friendly `tryRead*()`
  helpers, and instruction-budgeted poll jobs for a sole bus owner.

## Installation

### PlatformIO

```ini
lib_deps =
  https://github.com/janhavelka/OPT4001.git
```

The repository pins its Arduino builds to pioarduino `55.03.311` and native
tests to `platformio/native@1.2.1`. On Windows, repository validation uses the
checked-in `scripts\pio.cmd` wrapper so it selects the existing VS Code-managed
PlatformIO Core.

### Manual

Copy `include/OPT4001/` and `src/` into your project. `include/OPT4001/Version.h`
is generated from `library.json` but is committed, so `#include "OPT4001/OPT4001.h"`
works from a clean checkout without a pre-generation step. Do not edit it by hand.

### ESP-IDF

The repository root is an ESP-IDF component. A local project can add it through
`EXTRA_COMPONENT_DIRS`:

```cmake
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../OPT4001")
```

ESP-IDF derives the component name from the directory name, so the checkout must
be named `OPT4001` for `REQUIRES OPT4001` to resolve.

See [docs/integration/esp-idf.md](docs/integration/esp-idf.md) for the transport
adapter, status mapping, and example boundary.

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
    case 4: return OPT4001::Status::Error(OPT4001::Err::I2C_BUS, "I2C bus error");
    case 5: return OPT4001::Status::Error(OPT4001::Err::I2C_TIMEOUT, "I2C timeout");
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
  // The return type must be spelled uint32_t: millis() returns unsigned long,
  // which is a different type from uint32_t on ESP32.
  cfg.nowMs = [](void*) -> uint32_t { return static_cast<uint32_t>(millis()); };
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

### Shared-bus integration

When one task owns the I2C bus, use the bus-silent bind plus the
instruction-budgeted poll engine instead of the synchronous helpers:

```cpp
sensor.bind(cfg);                 // validates and caches, touches no I2C
sensor.startAttach();             // schedules 1 read + 4 writes

// in the bus owner's loop:
sensor.poll(nowMs, 1);            // at most one transport callback per poll
if (!sensor.pollBusy()) {
  sensor.startReadBurst();        // newest + FIFO0..FIFO2 in one block read
}
```

`readBurst()` is the preferred low-level sample primitive for shared-bus
integrations: one fixed RESULT/FIFO transfer yields four samples with raw
exponent, mantissa, ADC codes, counter, CRC and lux preserved for owner-side
policy.

## Contracts

The durable behavioural contracts — lifecycle, health, freshness, poll jobs,
dirty hardware/cache state, numeric and CRC policy — live in
[docs/integration/driver-contracts.md](docs/integration/driver-contracts.md).
The per-method contracts are Doxygen comments in
[include/OPT4001/OPT4001.h](include/OPT4001/OPT4001.h). The points most often
missed:

- **Not thread-safe, not ISR-safe.** One task per instance, or an application
  lock covering both the driver call and the transport callback. Transport
  callbacks must not re-enter the same instance.
- **`Config.mode` accepts only `POWER_DOWN` or `CONTINUOUS`.** One-shot
  measurements start explicitly through `startConversion()` or `readBlocking()`.
- **Size one-shot deadlines with `getOneShotBudget*()`, not `getConversionTime*()`.**
  A forced auto-range one-shot adds standby wake time and the auto-range penalty.
- **`FLAGS` (`0x0C`) is clear-on-read.** `readFlags()`, `readFlagsRaw()`, raw
  reads of `0x0C`, and raw blocks spanning it all consume the device's latched
  status view, including `FLAG_H` / `FLAG_L`.
- **Fresh reads need hardware evidence** — conversion-ready flag, configured INT
  assertion, or a sample-counter advance. `readLatestSample()` is the explicit
  "no freshness proof" escape hatch.
- **`CRC_ERROR` is data-bearing.** Fresh-sample and lux APIs still populate the
  decoded output and consume freshness on `CRC_ERROR`.
- **`OFFLINE` is latched.** Normal public I2C APIs then return `OFFLINE` without
  touching the bus. `probe()`, `recover()`, `softReset()`, `resetAndReapply()`,
  `startResetAndReapply()` and `poll()` driving a reset job are the exceptions.
- **`softReset()` / `resetAndReapply()` use the general-call reset (`0x00`/`0x06`),
  which is bus-wide.** Use them only when the bus topology allows it.
- **Raw register writes can desynchronise the cached configuration.** Check
  `hardwareConfigDirty()` / `hardwareConfigDirtyError()` and re-apply with
  `recover()`.

### Sample freshness

| Term / API | Meaning |
| --- | --- |
| Latest sample | Current `RESULT` contents; may repeat a previous sample. `readLatestSample()`. |
| Fresh sample | A newly observed conversion. Evidence is `CONVERSION_READY_FLAG`, configured INT assertion, or a counter advance. |
| Cached sample | RAM copy of the last successful decode: `getLastSample()`, `hasSample()`, `sampleTimestampMs()`, `sampleAgeMs()`. |
| Not ready | Fresh reads return `MEASUREMENT_NOT_READY`; `tryRead*()` return `OK` with `didRead = false`. |

The sample counter is modulo 16, so counter-only freshness cannot detect a
missed full wrap. Poll faster than 16 conversions when every conversion matters.

Elapsed conversion time is only a poll gate, never proof of completion: in
auto-range an overflow can abort a measurement, raise the range, and push
completion past the nominal conversion time.

### Numeric contract

Samples use a 4-bit exponent and 20-bit mantissa. Public raw conversion helpers
accept exponent `0..8` and mantissa `0x00000..0xFFFFF`; status-returning helpers
return `INVALID_PARAM` for anything else, and the compatibility float helpers
return quiet NaN. ADC codes are computed with 64-bit intermediates as
`mantissa << exponent` before scaling to lux.

Threshold registers use a 4-bit exponent and 12-bit result. The exact linear code
is `result << (8 + exponent)` and can exceed `uint32_t`, so
`thresholdToAdcCodes(threshold, uint64_t&)` is the lossless API; the legacy
`thresholdToAdcCodes(threshold)` saturates at `UINT32_MAX`. `luxToThreshold()`
rounds to the nearest representable value and rejects negative, non-finite, and
out-of-range lux.

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

### Probe diagnostics

`probe()` reads `DEVICE_ID` (`0x11`) through the raw transport without touching
health counters. Success means the full pattern is `0x0121`: fixed bits
`[15:14] = 0`, `DIDL[13:12] = 0`, `DIDH[11:0] = 0x121`. That is identity
validation, not optical or measurement-path validation.

| Condition | Returned status | `Status::detail` |
| --- | --- | --- |
| Address phase NACK, when the transport can distinguish it | `I2C_NACK_ADDR` | transport code |
| Data phase NACK, when the transport can distinguish it | `I2C_NACK_DATA` | transport code |
| Transaction timeout | `I2C_TIMEOUT` | transport code |
| Bus / arbitration / driver-state fault | `I2C_BUS` | transport code |
| Generic or unknown-phase transport failure | `I2C_ERROR` | transport code |
| Successful read, wrong `DEVICE_ID` pattern | `DEVICE_ID_MISMATCH` | raw register value |

Transport statuses are never collapsed to `DEVICE_NOT_FOUND`.

## Examples

- **`examples/01_basic_bringup_cli/`** — Arduino/PlatformIO interactive bring-up
  shell: bus scan and DEVICE_ID-qualified discovery, bind/attach/unbind, probe,
  recover, reset/reapply, decoded config / intcfg / flags / device-ID readback,
  one-shot and continuous reads, `tryread` / `trylux`, non-blocking `watch`,
  full poll-job control, burst/FIFO and slot history, threshold and interrupt
  configuration, raw register and block access, health monitor, and `selfcheck`.
- **`examples/esp_idf/basic/`** — native ESP-IDF project with `app_main()`,
  `driver/i2c_master.h` transport callbacks, and comparable command coverage.
  It owns its example I2C bus and does not demonstrate production shared-bus
  locking. Parity is enforced by `tools/check_idf_example_contract.py`.
- **`examples/common/`** — example-only glue: board config, serial logging, I2C
  transport adapter and scanner, bounded CLI line buffer, health views. This
  directory is **not** part of the library.

Both CLIs use fixed 192-byte command storage; an overlong line is discarded
completely and cannot dispatch as a truncated command.

Some CLI commands have hardware side effects: `flags` / `status` consume
clear-on-read FLAGS, `reset` is bus-wide, `wreg` can desynchronise the cached
configuration, and `selfcheck` performs conversions and recovery. `diag`
deliberately skips `FLAGS` so the report does not clear sticky status bits.

## Development

```powershell
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_ci_action_pins.py
python tools/check_idf_example_contract.py
python tools/check_version_header_contract.py
python tools/check_clean_consumer_package.py
python scripts/generate_version.py check
python tools/hil_opt4001_runner.py --parser-self-test
python tools/test_hil_opt4001_runner_parser.py
.\scripts\pio.cmd test -e native
.\scripts\pio.cmd run -e native_core_no_arduino
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s2dev
.\scripts\pio.cmd pkg pack
```

CI runs the same set plus a pure ESP-IDF matrix build for `esp32s2` and
`esp32s3`. With ESP-IDF installed locally:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

`tools/hil_opt4001_runner.py` drives either diagnostic CLI over a serial port and
writes bounded Markdown/JSON transcripts under `hil_logs/`. It does not
manipulate power, GPIO fixtures, or stuck-bus hardware; INT and reset/recovery
groups stay disabled unless the operator requests them explicitly
(`--include-int`, and `--include-fault` with `--confirm-faults`).

```bash
python tools/hil_opt4001_runner.py --parser-self-test
python tools/hil_opt4001_runner.py --port COM6 --cli arduino --group smoke
python tools/hil_opt4001_runner.py --port /dev/ttyUSB0 --cli idf --group smoke --group fifo
```

Board bring-up procedure: [docs/validation/hardware-validation-procedure.md](docs/validation/hardware-validation-procedure.md).

## Limits

- High-speed I2C entry sequencing is a controller-level bus procedure and is not
  modelled in the driver.
- SMBus alert response arbitration is controller-level and is not wrapped as a
  device API.
- INT-input hardware triggering is left to the board layer. The driver can read a
  configured INT GPIO hook through `readIntPinAsserted()` and apply the configured
  polarity, but it never configures, drives, or owns the pin.
- Window-transmission and similar optical calibration factors stay at the
  application layer rather than being baked into the lux conversion path.
- Threshold and interrupt behaviour is contract-tested at the register level.
  Physical comparator behaviour, SMBus alert arbitration, open-drain pulse timing
  and ISR integration are board-validation items.

## Documentation

- [docs/README.md](docs/README.md) — documentation index
- [docs/integration/driver-contracts.md](docs/integration/driver-contracts.md) — behavioural contracts
- [docs/integration/esp-idf.md](docs/integration/esp-idf.md) — ESP-IDF component and example boundary
- [docs/reference/OPT4001_datasheet.md](docs/reference/OPT4001_datasheet.md) — register map, timing, formulas
- [docs/validation/hardware-validation-procedure.md](docs/validation/hardware-validation-procedure.md) — board bring-up procedure
- [docs/validation/release-checklist.md](docs/validation/release-checklist.md) — release checklist
- [ASSUMPTIONS.md](ASSUMPTIONS.md) — choices made where the device documentation needed interpretation

## License

MIT License. See [LICENSE](LICENSE).
