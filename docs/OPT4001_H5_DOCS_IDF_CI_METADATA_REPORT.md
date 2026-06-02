# OPT4001 H5 Docs, IDF, CI, and Metadata Report

Date: 2026-06-02

Branch: `hardening/opt4001-industry-readiness`

Finding: H5 - Production and validation claims exceed available evidence

## Root Cause

The repository presented the driver as `production-grade` in user-facing
metadata while the recorded evidence only covered native fake-transport tests,
PlatformIO Arduino builds, static IDF example checks, and package generation.
Hardware, optical, interrupt, FIFO timing, address-pin, bus-fault, and local
pure ESP-IDF target validation were still pending. The ESP-IDF example also used
console input in a way that could make the diagnostic `tick()` loop depend on
whether `getchar()` returned promptly.

## Claims Changed

- Replaced unsupported `production-grade` wording in `README.md`,
  `library.json`, `idf_component.yml`, `CHANGELOG.md`, and
  `docs/OPT4001_datasheet.md`.
- Current README classification is source-level hardened and diagnostic-build
  tested, with hardware/optical/INT/FIFO/address/fault validation still pending.
- README now distinguishes native fake-transport tests, Arduino ESP32-S2/S3
  PlatformIO builds, static ESP-IDF contract checks, configured pure ESP-IDF CI,
  and local pure ESP-IDF validation that could not run because `idf.py` is not
  on `PATH`.
- README now includes a validation evidence table, pending validation matrix,
  package/address/electrical matrix, build commands, CI commands, latest/fresh
  semantics, FIFO CRC semantics, dirty hardware/cache state, and a hardware
  validation procedure link.

## ESP-IDF Example Changes

- The example is documented as diagnostic bring-up with example-owned I2C bus
  and device handles, not as a production shared-bus manager.
- `examples/esp_idf/basic/main/main.cpp` configures stdin with `O_NONBLOCK` and
  refuses to enter the diagnostic CLI if nonblocking setup fails. That avoids
  silently tying `device.tick()` cadence to blocking console input.
- `examples/esp_idf/basic/main/CMakeLists.txt` now uses `PRIV_INCLUDE_DIRS "."`
  and depends on the root `OPT4001` component for public headers instead of
  exposing repository parent paths.
- `Opt4001IdfI2cTransport.cpp` comments now make the status mapping boundary
  explicit for `ESP_ERR_INVALID_ARG`, `ESP_ERR_INVALID_STATE`, and
  `ESP_ERR_INVALID_RESPONSE`.

## CI Changes

- `.github/workflows/ci.yml` now uses `python -m platformio` for native tests,
  ESP32-S2/S3 builds, and package packing.
- CI runs:
  - `python tools/check_core_timing_guard.py`
  - `python tools/check_cli_contract.py`
  - `python tools/check_idf_example_contract.py`
  - `python tools/check_version_header_contract.py`
  - `python tools/check_readiness_claims.py`
  - `python tools/check_public_api_docs.py`
  - `python scripts/generate_version.py check`
  - `python -m platformio test -e native`
  - `python -m platformio run -e esp32s3dev`
  - `python -m platformio run -e esp32s2dev`
  - `python -m platformio pkg pack`
- CI now configures a pure ESP-IDF matrix build for `esp32s3` and `esp32s2`
  using `espressif/esp-idf-ci-action@v1` with ESP-IDF `v6.0.1`.
- `tools/check_idf_example_contract.py` now enforces nonblocking CLI setup,
  private include scope, parent-include rejection, and IDF status mapping tokens.
- `tools/check_readiness_claims.py` fails on unsupported readiness claims, stale
  SECURITY support, missing README readiness sections, and missing CI commands.
- `tools/check_public_api_docs.py` fails when public headers lose the lifecycle,
  freshness, FIFO CRC, dirty-state, blocking, FLAGS/INT, transport, or health
  contract documentation tokens.

## Metadata Changes

- `library.json` description: production-oriented OPT4001 driver with
  framework-neutral core.
- `idf_component.yml` description: production-oriented OPT4001 driver with
  framework-neutral core.
- `SECURITY.md` supported version: `1.0.x`; `< 1.0` is unsupported.
- `SECURITY.md` no longer mentions NVS side effects.
- `CHANGELOG.md` Unreleased section records Prompt 8 claims, guard, metadata,
  and IDF diagnostic changes.
- `ASSUMPTIONS.md` now uses the hardware-evidence freshness contract rather
  than only time-bounded polling wording.

## Public Contracts Added

`include/OPT4001/OPT4001.h` now documents:

- partial multi-register `begin()` failures and dirty hardware/cache state,
- `tick()` tracked I2C polling and health/offline side effects,
- `probe()` cached-config/raw-I2C/no-health behavior,
- `recover()` success/failure state outcomes,
- health tracking scope and counter/timestamp behavior,
- all-slot burst decode and aggregate CRC policy,
- lux helpers returning populated outputs on `CRC_ERROR`,
- blocking helper timebase, transaction timeout, yield, and poll-cap bounds,
- FLAGS clear-on-read / write-to-clear side effects on driver readiness,
- threshold partial-write dirty behavior,
- SOT-5X3 INT ownership and hardware-validation boundary,
- invalid helper input behavior and modulo-16 sample counter delta.

## Exact Command Results

| Command | Result |
| --- | --- |
| `git status --short` | Clean before Prompt 8 edits. |
| `git branch --show-current` | `hardening/opt4001-industry-readiness`. |
| `python tools/check_core_timing_guard.py` | `Core framework guard PASSED`. |
| `python tools/check_cli_contract.py` | `CLI contract PASSED`. |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED`. |
| `python tools/check_version_header_contract.py` | `Version header contract PASSED`; also reported `Version.h` up to date. |
| `python tools/check_readiness_claims.py` | `Readiness claims check PASSED`. |
| `python tools/check_public_api_docs.py` | `Public API docs check PASSED`. |
| `python scripts/generate_version.py check` | `Version.h` up to date. |
| `python -m platformio test -e native` | Passed, `96 test cases: 96 succeeded in 00:00:00.897`. |
| `python -m platformio run -e esp32s3dev` | Passed, `esp32s3dev SUCCESS` in `00:00:04.939`; RAM `6.9%`, flash `33.2%`. |
| `python -m platformio run -e esp32s2dev` | Passed, `esp32s2dev SUCCESS` in `00:00:04.443`; RAM `11.3%`, flash `32.7%`. |
| `python -m platformio pkg pack` | Passed; wrote `OPT4001-1.0.0.tar.gz`. The package artifact was removed after validation. |
| `idf.py --version` | Failed locally because `idf.py` is not available on PATH: `The term 'idf.py' is not recognized as the name of a cmdlet, function, script file, or operable program.` |
| `idf.py -C examples/esp_idf/basic set-target esp32s3 build` | Failed locally for the same missing-`idf.py` PATH reason. |
| `idf.py -C examples/esp_idf/basic set-target esp32s2 build` | Failed locally for the same missing-`idf.py` PATH reason. |

PlatformIO printed an obsolete-core warning during PlatformIO commands:
`Obsolete PIO Core v6.1.18 is used (previous was 6.1.19)`.

## Remaining Unvalidated Claims Intentionally Avoided

- No local pure ESP-IDF build pass is claimed. CI is configured to attempt those
  builds, but a completed workflow log must be reviewed before treating that as
  captured evidence.
- No target hardware validation is claimed in this prompt.
- No optical accuracy, cover-glass compensation, INT pulse timing, FIFO physical
  timing/order, SOT-5X3 address-pin matrix, SMBus alert arbitration, or
  bus-fault behavior is claimed as hardware validated.
- Prompt 9 remains responsible for hardware-in-loop evidence and the final
  validation report.
