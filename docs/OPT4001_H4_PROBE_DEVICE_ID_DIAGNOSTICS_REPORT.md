# OPT4001 H4 Probe and Device-ID Diagnostics Report

## Root Cause

`probe()` and `recover()` only checked the low `DIDH[11:0]` field of
`DEVICE_ID`, so values with correct `DIDH` but nonzero `DIDL[13:12]` or
fixed/reserved bits `[15:14]` could be accepted. `probe()` also converted most
transport failures to `DEVICE_NOT_FOUND`, which hid startup diagnostics such as
address NACK, data NACK, timeout, bus fault, or generic I2C failure.

## New ID Validation

The driver now validates the complete documented `DEVICE_ID` register pattern:

| Field | Bits | Expected |
| --- | --- | --- |
| Fixed/reserved | `[15:14]` | `0` |
| `DIDL` | `[13:12]` | `0` |
| `DIDH` | `[11:0]` | `0x121` |

Only raw value `0x0121` passes strict presence validation. The shared
`_validateDeviceId()` helper calls `decodeDeviceId()` and is used by `probe()`,
`begin()` through `probe()`, and `recover()`. Invalid examples such as `0x1121`,
`0x2121`, `0x4121`, and `0xC121` now return `DEVICE_ID_MISMATCH`.

## Status Mapping

`probe()` is still a raw diagnostic path and does not update health counters, but
it now preserves transport failures returned by the injected callback.

| Condition | Status |
| --- | --- |
| Address phase NACK if transport distinguishes it | `I2C_NACK_ADDR` |
| Data phase NACK if transport distinguishes it | `I2C_NACK_DATA` |
| I2C transaction timeout | `I2C_TIMEOUT` |
| Bus/arbitration/driver-state fault | `I2C_BUS` |
| Generic transport failure or unknown NACK phase | `I2C_ERROR` |
| Successful read with wrong full ID pattern | `DEVICE_ID_MISMATCH` with raw ID in `detail` |

The ESP-IDF example adapter keeps `ESP_ERR_TIMEOUT -> I2C_TIMEOUT` and keeps
`ESP_ERR_INVALID_RESPONSE -> I2C_ERROR` because the IDF transaction API used
there does not expose address-vs-data NACK phase. It now documents that
limitation in code and docs, maps `ESP_ERR_INVALID_ARG` to `INVALID_PARAM`,
maps `ESP_ERR_INVALID_STATE` to `I2C_BUS`, and preserves raw `esp_err_t` values
in `Status::detail`.

## Public API Changes

- `DeviceIdInfo` now exposes `reservedBitsClear`.
- `DeviceIdInfo::matchesExpected` now requires fixed/reserved bits clear,
  `DIDL == 0`, and `DIDH == 0x121`.
- `probe()` behavior changed intentionally: transport failures are returned as
  their original statuses instead of being collapsed to `DEVICE_NOT_FOUND`.

No `probeDetailed()` API was added because existing `probe()` plus
`readDeviceId(DeviceIdInfo&)` provide the required diagnostics without an extra
public entry point.

## Files Changed

| File | Change |
| --- | --- |
| `include/OPT4001/CommandTable.h` | Added full `DEVICE_ID` masks/expected constants. |
| `include/OPT4001/OPT4001.h` | Documented stricter probe contract and added `reservedBitsClear`. |
| `src/OPT4001.cpp` | Added shared strict ID validation and preserved probe transport statuses. |
| `test/test_basic.cpp` | Added valid/bad ID, begin/recover, probe status, decode, and health side-effect tests. |
| `examples/esp_idf/basic/main/Opt4001IdfI2cTransport.cpp` | Improved IDF status mapping and documented NACK phase limitation. |
| `examples/01_basic_bringup_cli/main.cpp` | Printed reserved-bit decode state in the ID command. |
| `examples/esp_idf/basic/main/main.cpp` | Printed reserved-bit decode state in the ID command. |
| `README.md` | Documented probe validation and status mapping. |
| `docs/IDF_PORT.md` | Documented ESP-IDF status mapping and limitation. |
| `docs/OPT4001_HARDENING_FINDING_TO_PROMPT_PLAN.md` | Marked Prompt 4/H4 complete with evidence. |

## Tests Added

- `test_probe_accepts_valid_full_device_id`
- `test_probe_rejects_matching_didh_with_nonzero_high_id_bits`
- `test_begin_rejects_matching_didh_with_nonzero_high_id_bits`
- `test_recover_rejects_matching_didh_with_nonzero_high_id_bits`
- `test_probe_preserves_transport_errors_and_detail`
- `test_probe_success_and_failure_do_not_update_health_or_state`
- Decode-helper assertions for nonzero `DIDL` and nonzero reserved bits.

## Validation Results

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | Passed: `Core framework guard PASSED`. |
| `python tools/check_cli_contract.py` | Passed: `CLI contract PASSED`. |
| `python tools/check_idf_example_contract.py` | Passed: `IDF example contract PASSED`. |
| `python tools/check_version_header_contract.py` | Passed: `Version header contract PASSED`; reported `Version.h` up to date. |
| `python scripts/generate_version.py check` | Passed: reported `Version.h` up to date. |
| `python -m platformio test -e native` | Passed: 61 tests succeeded. PlatformIO warned that obsolete Core v6.1.18 is active while v6.1.19 also exists. |
| `python -m platformio run -e esp32s3dev` | Passed: `esp32s3dev SUCCESS`; RAM 6.9%, flash 33.1%. Same PlatformIO Core warning. |
| `python -m platformio run -e esp32s2dev` | Passed: `esp32s2dev SUCCESS`; RAM 11.3%, flash 32.6%. Same PlatformIO Core warning. |
| `python -m platformio pkg pack` | Passed: wrote `OPT4001-1.0.0.tar.gz`; artifact removed after validation. |
| `idf.py --version` | Not run successfully: PowerShell reported `idf.py` is not recognized. |
| `idf.py -C examples/esp_idf/basic set-target esp32s3 build` | Not run successfully: PowerShell reported `idf.py` is not recognized. |
| `idf.py -C examples/esp_idf/basic set-target esp32s2 build` | Not run successfully: PowerShell reported `idf.py` is not recognized. |

## Remaining Limitations

- Local pure ESP-IDF builds were attempted but not executed because `idf.py` is
  not available on `PATH` in this shell.
- No target hardware was connected or validated in this prompt; Prompt 9 remains
  responsible for hardware/target-IDF validation evidence.
- Prompt 5 remains responsible for freshness/readiness behavior.
