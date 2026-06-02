# OPT4001 Numeric, Vector, and CRC Report

Date: 2026-06-02

Branch: `hardening/opt4001-industry-readiness`

Prompt: Prompt 6 - Fix Numeric Safety, Threshold Overflow, CRC, and Independent
Test Vectors

## Issues Fixed

- `rawToLux()` previously shifted caller-controlled `mantissa << exponent`
  without validating exponent or mantissa bounds. Exponents `>= 32` could invoke
  undefined behavior, and result exponents outside the supported `0..8` range
  could silently produce meaningless lux.
- `_decodeSampleRegisters()` accepted the 4-bit hardware exponent without
  rejecting impossible result exponents `9..15`, then cached scaled values from
  the unchecked shift.
- `thresholdToAdcCodes()` returned `uint32_t` from
  `result << (8 + exponent)`, but valid threshold registers can exceed
  `UINT32_MAX`.
- Threshold interrupt ordering used the legacy 32-bit conversion and could miss
  reversed high-exponent windows after saturation or wrap.
- CRC tests used `_computeCrcNibble()` to seed expected good CRCs, which made the
  CRC coverage circular instead of independent.

## Chosen Design

- Added `Status rawToAdcCodes(uint8_t, uint32_t, uint64_t&)` and
  `Status rawToLux(uint8_t, uint32_t, float&)` as status-returning conversion
  APIs.
- Kept the legacy `float rawToLux(uint8_t, uint32_t)` API for compatibility.
  It now returns quiet NaN for invalid result fields instead of shifting.
- Added `Status thresholdToAdcCodes(const Threshold&, uint64_t&)` as the lossless
  threshold decode API.
- Kept the legacy `uint32_t thresholdToAdcCodes(const Threshold&)` API for
  compatibility. It now saturates at `UINT32_MAX` for invalid or too-large
  thresholds instead of wrapping.
- Updated internal threshold ordering to compare 64-bit ADC codes.
- Kept CRC algorithm behavior, but added independent tests that compute expected
  CRC nibbles from the datasheet XOR equations in test code.

## Invalid-Input Policy

- Raw result exponent must be `0..8`.
- Raw result mantissa must fit 20 bits, `0x00000..0xFFFFF`.
- Invalid raw result fields return `INVALID_PARAM` from status-returning helpers.
  The compatibility float helper returns quiet NaN.
- Decoded register samples with invalid numeric fields return `INVALID_PARAM`,
  write NaN to the transient sample lux field, and do not update the cached
  sample.
- Threshold exponent must be `0..15`; threshold result must be `0..0x0FFF`.
- `luxToThreshold()` rejects negative, NaN, infinite, and out-of-register-range
  lux values. Valid lux requests are rounded to the nearest ADC code before
  threshold register quantization.

## Vector Sources

- Result/lux formula: `docs/OPT4001_datasheet.md`, local section
  "Lux Calculation".
- Threshold formula: `docs/OPT4001_datasheet.md`, local section
  "Threshold Detection".
- CRC equations: `docs/OPT4001_datasheet.md`, local section
  "Output CRC Verification".
- Conversion-time/effective-bit tables and package lux LSBs:
  `include/OPT4001/CommandTable.h`.
- Package/address enums and valid public ranges:
  `include/OPT4001/Config.h`.

## Test Coverage Added

- `test_package_address_matrix`
- `test_raw_lux_vectors_use_64_bit_intermediates`
- `test_raw_lux_rejects_invalid_result_fields_without_shift_ub`
- `test_decode_rejects_invalid_result_exponent_without_cache_update`
- `test_crc_vectors_use_datasheet_oracle`
- `test_crc_mismatch_preserves_received_crc_and_decode_fields`
- `test_threshold_adc_vectors_use_64_bit_and_legacy_saturates`
- `test_threshold_interrupt_ordering_uses_64_bit_codes`
- `test_all_conversion_time_vectors_and_invalid_values`
- `test_range_vectors_and_invalid_values`
- `test_lux_to_threshold_rounding_and_out_of_range_policy`

The CRC oracle in `test/test_basic.cpp` does not call `_computeCrcNibble()`.
Existing sample seeding also uses the test-side oracle.

## Files Changed

- `include/OPT4001/OPT4001.h`
- `include/OPT4001/Config.h`
- `src/OPT4001.cpp`
- `test/test_basic.cpp`
- `README.md`
- `docs/IDF_PORT_IMPLEMENTATION.md`
- `docs/extracted-md/03_electrical_and_timing.md`
- `docs/OPT4001_HARDENING_FINDING_TO_PROMPT_PLAN.md`
- `docs/OPT4001_NUMERIC_VECTOR_CRC_REPORT.md`

## Validation Results

| Command | Exact result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | Passed: `Core framework guard PASSED` |
| `python tools/check_cli_contract.py` | Passed: `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | Passed: `IDF example contract PASSED` |
| `python tools/check_version_header_contract.py` | Passed: `Version header contract PASSED`; `Up to date: C:\Users\Honza\Documents\Projects\OPT4001\include\OPT4001\Version.h` |
| `python scripts/generate_version.py check` | Passed: `Up to date: C:\Users\Honza\Documents\Projects\OPT4001\include\OPT4001\Version.h` |
| `python -m platformio test -e native` | Passed: `83 test cases: 83 succeeded in 00:00:00.989`; PlatformIO printed the existing obsolete-core warning for v6.1.18. |
| `python -m platformio run -e esp32s3dev` | Passed: `esp32s3dev SUCCESS` in `00:00:05.320`; RAM `6.9%`, Flash `33.2%`; PlatformIO printed the existing obsolete-core warning for v6.1.18. |
| `python -m platformio run -e esp32s2dev` | Passed: `esp32s2dev SUCCESS` in `00:00:04.961`; RAM `11.3%`, Flash `32.7%`; PlatformIO printed the existing obsolete-core warning for v6.1.18. |
| `python -m platformio pkg pack` | Passed: wrote `OPT4001-1.0.0.tar.gz`; artifact removed after validation. |
| `idf.py --version` | Failed: `idf.py : The term 'idf.py' is not recognized as the name of a cmdlet, function, script file, or operable program.` |
| `idf.py -C examples/esp_idf/basic set-target esp32s3 build` | Failed with the same `idf.py` not recognized PowerShell error. |
| `idf.py -C examples/esp_idf/basic set-target esp32s2 build` | Failed with the same `idf.py` not recognized PowerShell error. |

## Remaining Limitations

- Pure ESP-IDF target builds were not run locally because `idf.py` is not
  installed or not on `PATH` in this shell.
- FIFO per-slot CRC/ordering behavior, INT behavior, and multi-register
  partial-state consistency are intentionally left for Prompt 7.
- Hardware, optical, INT-pin, FIFO, address-pin, and pure target-IDF validation
  remain part of the later hardware validation prompt.
