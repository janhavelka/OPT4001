# OPT4001 Hardware Validation Procedure

Status: procedure only. This document does not claim that hardware validation
has been completed.

## Session Record

Fill these fields before running any hardware or hardware-in-loop test.

| Field | Value |
| --- | --- |
| Date |  |
| Operator |  |
| Board |  |
| MCU target |  |
| Firmware commit |  |
| Library commit |  |
| Sensor package variant | PicoStar / SOT-5X3 |
| Sensor address wiring |  |
| Optical setup |  |
| Reference lux meter model/calibration |  |
| Cover glass/window details |  |
| Serial port / baud |  |
| I2C pullups / bus speed |  |
| INT wiring / pullup / capture channel |  |
| Logic analyzer model/sample rate |  |
| Pass/fail notes |  |

Record command transcripts, photos or wiring notes, optical reference readings,
logic-analyzer captures, and failures. Do not convert configured checks into
validation claims until observed evidence is committed.

## Build And Static Checks

Run and record:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_version_header_contract.py
python tools/check_readiness_claims.py
python tools/check_public_api_docs.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Remove `OPT4001-*.tar.gz` after package validation.

## CLI Command Notes

The Arduino bring-up CLI and native ESP-IDF diagnostic CLI are similar but not
identical.

| Purpose | Arduino CLI | ESP-IDF CLI |
| --- | --- | --- |
| Driver state | `state`, `drv`, `health` | `drv`, `online` |
| Device ID | `id` or `identify` | `id` |
| Conversion time | `ctime 0` through `ctime 11` | `ctime 600`, `ctime 1`, `ctime 2`, `ctime 3`, `ctime 6`, `ctime 12`, `ctime 25`, `ctime 50`, `ctime 100`, `ctime 200`, `ctime 400`, `ctime 800` |
| FIFO/burst read | `burst 1`, then `readburst`; optional `slot 0..3` | `burst` or `fifo` |
| Address/package selection | `pkg ...`, `addr ...` | fixed by example constants; rebuild or modify board config for address/package matrix |
| Freshness poll | `poll` or `drdy` | `ready` |
| INT pin sample | `intpin` | `int pin` |

Use the command names above in logs so the procedure is reproducible.

## Safe Smoke

Run after flashing and opening the serial CLI. Expected result: no command hangs,
`probe`/`id` identify OPT4001, and `read`/`lux` return `OK` or a documented
`CRC_ERROR` warning with decoded output.

Arduino CLI:

```text
version
scan
probe
id
cfg
state
read
lux
flags
selftest
```

ESP-IDF CLI:

```text
version
scan
probe
id
cfg
drv
read
lux
flags
selftest
```

## Conversion-Time Sweep

Verify all conversion-time settings, sample counter movement, and freshness. For
each setting, record the configured time, measured latency, sample counter, CRC
state, and whether a duplicate counter was rejected as not fresh.

Arduino CLI:

```text
ctime 0
read
ctime 1
read
ctime 2
read
ctime 3
read
ctime 4
read
ctime 5
read
ctime 6
read
ctime 7
read
ctime 8
read
ctime 9
read
ctime 10
read
ctime 11
read
```

ESP-IDF CLI:

```text
ctime 600
read
ctime 1
read
ctime 2
read
ctime 3
read
ctime 6
read
ctime 12
read
ctime 25
read
ctime 50
read
ctime 100
read
ctime 200
read
ctime 400
read
ctime 800
read
```

## Freshness And Counter Behavior

Objective: prove fresh reads are tied to conversion-ready flag, configured
SOT-5X3 INT assertion, or counter advance.

Arduino CLI sequence:

```text
measure auto 8 cont 1
watch 32 250
stop
tryread
tryread
sample
sampleage
```

ESP-IDF CLI sequence:

```text
range 12
mode 3
watch
watch
sample
sampleage
ready
```

Record whether counters advance modulo 16, whether any gaps appear, and whether
two immediate fresh reads of the same counter avoid reporting duplicate data as
fresh.

## Address And Package Matrix

PicoStar/YMN:

- Expected I2C address is fixed `0x45`.
- No ADDR pin and no INT pin are available.
- Arduino CLI: set `pkg pico`, then verify `addr 0x45`, `init`, `probe`, `id`.
- Unsupported addresses must fail package validation or probe, not be treated as
  successful devices.

SOT-5X3/DTS:

- Validate supported addresses `0x44`, `0x45`, and `0x46` according to ADDR
  wiring.
- Arduino CLI: set `pkg sot`, then `addr 0x44` / `addr 0x45` / `addr 0x46`,
  `init`, `probe`, and `id` for each physically wired address.
- ESP-IDF CLI: address/package are fixed by example constants; rebuild or adjust
  the example configuration for each address.
- ADDR=SCL is a board-wiring caveat. Test it only when the board explicitly
  routes ADDR to SCL and the setup can be inspected safely.

## FIFO / Burst

Objective: verify four-slot decode, newest-to-oldest counter order, and per-slot
CRC state.

Arduino CLI:

```text
measure auto 8 cont 1
burst 1
watch 8 100
readburst
slot 0
slot 1
slot 2
slot 3
```

ESP-IDF CLI:

```text
mode 3
watch
watch
burst
fifo
```

Record `RESULT`, `FIFO0`, `FIFO1`, and `FIFO2` counters, `crcValid` state, and
aggregate status. Do not infer FIFO empty/full unless the INT FIFO-full pulse or
other hardware evidence is captured.

## INT / Threshold Validation

SOT-5X3 only. Requires a pullup and logic analyzer or timestamped GPIO capture.
PicoStar has no INT pin; record INT tests as not applicable for PicoStar.

Arduino CLI examples:

```text
int latch 1
int pol low
int faults 1
threshold 1 1000
int th 1 1000
intpin
flags
int ready
int fifo
int dir out
int cfg fifo
```

ESP-IDF CLI examples:

```text
latch 1
pol 0
fault 0
threshold lux 1 1000
int threshold
int pin
flags
int ready
int fifo
int dir 1
```

Capture and record:

- threshold low/high flag behavior,
- fault-count behavior,
- latched persistence and clear-on-read behavior,
- every-conversion pulse timing,
- FIFO-full pulse every four conversions,
- INT polarity,
- hardware-trigger one-shot if the board supports controlled INT input pulses.

Do not claim INT validation without logic-analyzer or timestamped GPIO evidence.

## Optical Validation

Record reference lux meter model, calibration status, distance, light source,
sensor orientation, cover glass/window details, and expected tolerance before
testing.

Minimum points:

| Scene | Required record |
| --- | --- |
| Dark / covered sensor | Reference lux, OPT4001 lux, sample counter, CRC state. |
| Stable indoor light | Reference lux, OPT4001 lux, tolerance calculation. |
| Bright light | Reference lux, range/exponent, saturation/overload flags. |
| Under glass/window | Glass/window material, transmission assumption, before/after lux. |
| IR-rich source if available | Source type, reference reading, OPT4001 reading, notes. |

Application-specific optical compensation belongs outside the driver unless a
separate calibration policy is documented.

## Fault And Recovery

Run only safe, controlled fault tests. Do not short pins or force stuck-bus
conditions unless the fixture is designed for it.

| Fault | Required evidence |
| --- | --- |
| Address NACK | Probe/read status, detail code when available, no false success. |
| Unplug/replug | Transition to `DEGRADED`/`OFFLINE`, no unbounded wait, manual `recover()` behavior. |
| Power cycle/brownout | Dirty state or re-probe/recover behavior after device reset. |
| Reset/reapply | `resetreapply` result and config readback; confirm bus-wide reset is safe on this fixture. |
| Stuck bus | Only with a controlled fixture; record timeout and recovery behavior. |
| Offline latch | Normal I2C APIs return offline/busy without bus access until `recover()`. |

Useful commands:

```text
drv
state
probe
recover
resetreapply
flags
cfg
selftest
```

## Framework Matrix

Record result and log link for each:

| Framework / target | Build evidence | Hardware evidence |
| --- | --- | --- |
| Arduino ESP32-S2 |  |  |
| Arduino ESP32-S3 |  |  |
| Pure ESP-IDF ESP32-S2 |  |  |
| Pure ESP-IDF ESP32-S3 |  |  |

Static checks and CI configuration are useful, but completed build logs and
hardware transcripts are required before claiming validation for a target.
