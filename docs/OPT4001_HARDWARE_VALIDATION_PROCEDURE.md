# OPT4001 Hardware Validation Procedure

Date: 2026-06-02

Status: procedure only. No hardware evidence is captured by this document.

## Purpose

Use this procedure to capture Prompt 9 evidence for real OPT4001 boards. A test
passes only when the board, package variant, firmware commit, command sequence,
expected result, observed result, and any instrument captures are recorded.

## Required Metadata

- Git commit and branch.
- Board model, ESP32 target, power source, and I2C pullups.
- OPT4001 package variant: PicoStar or SOT-5X3.
- I2C address and, for SOT-5X3, ADDR pin wiring.
- INT pin wiring, pullup voltage, GPIO number, and logic-analyzer channel when
  INT tests are run.
- Optical setup: reference lux meter, light source, distance, cover glass or
  window material, and ambient conditions.
- Tool versions: `python -m platformio --version`, `idf.py --version` when
  available, and ESP-IDF version used by CI or local builds.

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

Remove package artifacts after validation.

## Hardware Matrix

Record pass/fail evidence for each applicable package/target pair:

| Area | Required evidence |
| --- | --- |
| PicoStar address | `0x45` ACKs and unsupported addresses fail package validation. |
| SOT-5X3 addresses | ADDR wiring produces expected `0x44`, `0x45`, or `0x46` behavior. |
| Device ID | `probe()` and `begin()` read the expected full `DEVICE_ID` pattern. |
| Lux path | Measured lux tracks a reference meter within the application-defined tolerance. |
| Fresh/latest semantics | Fresh reads require flag, INT, or counter evidence; latest reads may duplicate. |
| FIFO order | `RESULT`, `FIFO0`, `FIFO1`, and `FIFO2` counters form a plausible newest-to-oldest history. |
| CRC behavior | CRC fields are recorded; forced or observed CRC warnings preserve decoded fields. |
| Threshold flags | Low/high threshold flags match light stimulus and latch/transparent settings. |
| INT threshold output | SOT-5X3 open-drain INT behavior matches polarity, latch, and fault count settings. |
| INT every-conversion pulse | Logic analyzer captures one pulse per conversion when configured. |
| INT FIFO-full pulse | Logic analyzer captures one pulse every four conversions when configured. |
| INT input trigger | Board layer can trigger one-shot through INT input mode without driver-owned GPIO output. |
| Recovery | `recover()` restores `READY` after an induced transport fault when the bus is healthy. |
| Reset/reapply | General-call reset side effect is acceptable on the test bus and cached config is re-applied. |

## Reporting

Create a Prompt 9 report that includes command transcripts, photos or wiring
notes, logic-analyzer captures when relevant, optical reference data, failures,
and the final readiness classification. Do not convert configured tests into
validation claims until the observed evidence is committed.
