# OPT4001 FIFO, INT, and Partial State Report

Date: 2026-06-02

Branch: `hardening/opt4001-industry-readiness`

Prompt: Prompt 7 - Fix FIFO/Burst CRC Semantics, INT/Flags Clarity, and Partial
Hardware-State Dirty Tracking

## Root Cause

- `readBurst()` decoded later FIFO slots only while the aggregate status stayed
  `OK`. A CRC warning in an earlier slot could hide later slot data even though
  CRC mismatch is a per-sample condition and the remaining register fields are
  still useful.
- FIFO ordering and freshness were not documented clearly enough for users to
  distinguish the current `RESULT` registers from the three FIFO shadow pairs.
- FLAGS and INT behavior had source support, but public documentation did not
  make the clear-on-read/write side effects, SOT-5X3-only open-drain ownership,
  and remaining hardware-validation boundary explicit enough.
- Multi-register configuration writes could partially update device hardware
  while the cached configuration was rolled back. The driver had no visible
  state telling callers that hardware and cached software state might diverge.

## Chosen FIFO/CRC Design

- `readBurst()` now decodes all four slots after a successful register transfer:
  `newest`, `fifo0`, `fifo1`, and `fifo2`.
- Slot order is documented as `newest` from `RESULT`, `fifo0` as the prior
  conversion, `fifo1` as two conversions ago, and `fifo2` as three conversions
  ago.
- CRC mismatch remains a warning-style status. The affected `Sample` keeps its
  decoded fields and records `crcValid = false`.
- The aggregate `readBurst()` status is `CRC_ERROR` when any slot has a CRC
  mismatch and no more serious error occurs.
- Fatal decode or transport failures still stop the operation because they mean
  the affected data cannot be trusted or was not read.
- No FIFO-empty detection claim was added. The device exposes history shadows;
  the driver documents ordering and CRC state, not empty/full inference.

## Chosen INT/Flags Documentation Stance

- Public headers and README now state that FLAGS register `0x0C` is
  clear-on-read, and that explicit nonzero writes also clear the addressed
  latched flag bits.
- The docs now distinguish decoded FLAGS reads from raw reads and note that both
  consume the same hardware latched view.
- INT is documented as SOT-5X3-only and open-drain. The application owns pullup,
  GPIO configuration, ISR attachment, debouncing, and pin lifetime.
- INT output modes are documented as threshold/SMBus alert, pulse after every
  conversion, and pulse every four conversions for FIFO-full indication.
- Threshold register packing and INT configuration remain fake-bus tested, while
  threshold comparator behavior, SMBus alert arbitration, INT pulse timing, and
  ISR integration remain hardware-validation items.

## Dirty-State Design

- Added `hardwareConfigDirty()` and `hardwareConfigDirtyError()` public accessors.
- Added `SettingsSnapshot::hardwareConfigDirty` and
  `SettingsSnapshot::hardwareConfigDirtyError`.
- `_applyConfig()` marks dirty after failures in write positions 2-4 because at
  least one earlier hardware register write has already succeeded.
- A first `_applyConfig()` write failure leaves dirty false when no hardware
  mutation occurred, unless dirty was already set from an earlier operation.
- `setThresholds()` marks dirty when the low threshold write succeeds and the
  high threshold write fails.
- `resetAndReapply()` marks dirty if the general-call reset succeeds and cached
  configuration re-apply then fails, including first re-apply write failure.
- Dirty state persists across unrelated reads and failed recovery attempts.
- Dirty state clears only after a full successful configuration re-apply through
  `_applyConfig()`, `recover()`, or `resetAndReapply()`.

## Files Changed

| File | Change |
| --- | --- |
| `include/OPT4001/OPT4001.h` | Added dirty-state snapshot/accessor contract and documented burst slot ordering, per-slot CRC visibility, FLAGS side effects, and INT ownership. |
| `include/OPT4001/Config.h` | Clarified optional SOT-5X3 INT hook ownership and PicoStar absence. |
| `src/OPT4001.cpp` | Aggregates per-slot burst decode status without hiding later CRC slots, tracks partial hardware-config dirty state, and clears dirty state only after full re-apply. |
| `test/test_basic.cpp` | Added FIFO CRC aggregation tests and dirty-state tests for apply positions, threshold pair failure, read persistence, recover, and reset/reapply. |
| `README.md` | Documented FIFO ordering/CRC behavior, FLAGS side effects, INT ownership/modes, hardware-validation caveats, and dirty recovery guidance. |
| `docs/IDF_PORT_IMPLEMENTATION.md` | Clarified IDF application ownership of SOT-5X3 INT GPIO/ISR behavior. |
| `docs/extracted-md/02_pinout_and_signals.md` | Clarified SOT-5X3 INT open-drain/application ownership. |
| `docs/extracted-md/05_register_map.md` | Clarified INT config fields and FLAGS clear behavior. |
| `docs/extracted-md/06_modes_interrupts_status_and_faults.md` | Clarified FIFO, FLAGS, threshold, and INT behavior plus hardware-validation limits. |
| `docs/extracted-md/07_initialization_reset_and_operational_notes.md` | Added operational notes for dirty-state recovery and FLAGS side effects. |
| `docs/extracted-md/08_variant_differences_and_open_questions.md` | Clarified remaining INT and threshold hardware-validation questions. |
| `docs/OPT4001_HARDENING_FINDING_TO_PROMPT_PLAN.md` | Marked Prompt 7 source-level work complete with evidence and left hardware validation for Prompt 9. |
| `docs/OPT4001_FIFO_INT_PARTIAL_STATE_REPORT.md` | Added this report. |

## CI Changes

No workflow change was required for Prompt 7. The new behavior is covered by the
native PlatformIO test suite and the existing guard scripts remain part of the
required validation set.

## Exact Command Results

| Command | Result |
| --- | --- |
| `git status --short` | Clean before Prompt 7 edits. |
| `git branch --show-current` | `hardening/opt4001-industry-readiness`. |
| `python tools/check_core_timing_guard.py` | `Core framework guard PASSED`. |
| `python tools/check_cli_contract.py` | `CLI contract PASSED`. |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED`. |
| `python tools/check_version_header_contract.py` | `Version header contract PASSED`; also reported `Version.h` up to date. |
| `python scripts/generate_version.py check` | `Version.h` up to date. |
| `python -m platformio test -e native` | Passed, `96 test cases: 96 succeeded in 00:00:00.948`. |
| `python -m platformio run -e esp32s3dev` | Passed, `esp32s3dev SUCCESS` in `00:00:05.068`; RAM `6.9%`, flash `33.2%`. |
| `python -m platformio run -e esp32s2dev` | Passed, `esp32s2dev SUCCESS` in `00:00:04.623`; RAM `11.3%`, flash `32.7%`. |
| `python -m platformio pkg pack` | Passed; wrote `OPT4001-1.0.0.tar.gz`. The package artifact was removed after validation. |
| `idf.py --version` | Failed locally because `idf.py` is not available on PATH: `The term 'idf.py' is not recognized as the name of a cmdlet, function, script file, or operable program.` |
| `idf.py -C examples/esp_idf/basic set-target esp32s3 build` | Failed locally for the same missing-`idf.py` PATH reason. |
| `idf.py -C examples/esp_idf/basic set-target esp32s2 build` | Failed locally for the same missing-`idf.py` PATH reason. |

PlatformIO printed an obsolete-core warning during PlatformIO commands:
`Obsolete PIO Core v6.1.18 is used (previous was 6.1.19)`.

## Remaining Limitations

- No target hardware validation was run in this prompt.
- FIFO physical timing/order, FIFO-full INT pulse behavior, threshold comparator
  behavior, SMBus alert arbitration, INT polarity/pulse timing, address-pin
  wiring, optical response, and pure local ESP-IDF target builds remain pending.
- Local pure ESP-IDF validation could not be executed because `idf.py` is not
  installed or not available on PATH in this environment.
- Prompt 8 remains responsible for docs/examples/CI honesty. Prompt 9 remains
  responsible for hardware and pure ESP-IDF validation evidence.
