# OPT4001 Source, Datasheet, And CLI Audit — 2026-08-07

## Outcome And Evidence Boundary

The repository was audited against the current TI product page and data sheet,
the framework/health conventions used by mature I2C repositories in the local
`Projects/` workspace, and native fake-transport tests. Source-level issues
found during the audit were fixed in version metadata `1.1.0`.

This is **not hardware validation**. No OPT4001 board, package/address strap,
optical reference, INT capture, FIFO timing capture, or injected electrical
fault fixture was available. No release tag was created. Those HIL gates remain
open even where the source behavior is covered by deterministic tests.

## Authoritative Device Sources

- Texas Instruments, **OPT4001 High-Speed High-Precision Digital Ambient Light
  Sensor**, document `SBOS993A`, December 2021, revised December 2022, Revision
  A: <https://www.ti.com/lit/ds/symlink/opt4001.pdf>
- Current TI product page, checked 2026-08-07:
  <https://www.ti.com/product/OPT4001>

The TI product page still lists Revision A as the current data sheet at audit
time. Local summaries and historical reports were used only as navigation;
device conclusions below were checked against the TI source.

## Datasheet/Register Audit

| Area | TI contract checked | Repository result |
| --- | --- | --- |
| Addresses/packages | PicoStar/YMN fixed `0x45`; SOT-5X3/DTS supports `0x44`, `0x45`, `0x46`; only DTS exposes ADDR and INT | Address matrix was correct. `begin()` now also rejects an impossible PicoStar INT hook, and DTS-only INT-output presets reject PicoStar before I2C. |
| RESULT/FIFO | `0x00..0x07`; exponent, 20-bit mantissa, four-bit counter and CRC; RESULT newest plus FIFO0..FIFO2 | Fields, scaling, CRC oracle and order match. Burst-enabled single-sample reads now fetch RESULT and RESULT_LSB_CRC coherently in one four-byte transaction. |
| Thresholds | `0x08/0x09`; reset `0x0000`/`0xBFFF`; exponent/result packing and linear-code shift | Constants and helpers match. Invalid inputs and wide intermediate behavior are tested. |
| CONFIGURATION | `0x0A`, reset `0x3208`; QWAKE, RANGE, CONVERSION_TIME, MODE, LATCH, POL, FAULT_COUNT | Masks, reset and validation match. Native IDF `ctime` parsing was fixed to map displayed enum values `0..11` exactly. |
| INT_CONFIGURATION | `0x0B`, reset/fixed pattern `0x8011`; INT direction/function and I2C burst | Fixed pattern and fields match. Package-restricted presets are explicit. |
| FLAGS | `0x0C`; overload/ready/high/low; read clears sticky threshold flags; nonzero write clears conversion-ready | Decode/clear helpers match. Generic raw FLAGS reads/writes now invalidate cached readiness evidence without marking configuration dirty. |
| DEVICE_ID | `0x11`, expected fixed value/pattern `0x0121` | Full fixed-pattern validation is retained in begin/probe/recover. |
| Conversion timing | 600 us, 1 ms, 1.8 ms, 3.4 ms, 6.5 ms, 12.7 ms, 25 ms, 50 ms, 100 ms, 200 ms, 400 ms, 800 ms; standby and forced-auto overheads | Tables and one-shot budgets match. Invalid enum helpers now return their documented zero/NaN sentinels. |
| Modes | Power-down, forced-auto one-shot, history-based one-shot, continuous; quick wake | All are exposed by the API and diagnostic CLIs while stable startup remains power-down/continuous only. |
| Reset | General-call address `0x00`, command `0x06`, bus-wide effect | Correct and explicitly documented/CLI-visible; real shared-bus reset remains confirmation/fixture policy outside the core. |

Package lux LSBs and range full-scale tables were checked for both PicoStar and
SOT-5X3. CRC tests use an independent datasheet-equation oracle. Numeric paths
avoid undefined shifts and preserve 64-bit threshold intermediates.

## Software Contract And Straight-Bug Findings

### Fixed

1. **Split sample-register race.** With burst enabled, `readSample()` performed
   two independent register transactions. A conversion between the reads could
   combine an exponent/mantissa MSB from one sample with LSB/counter/CRC from
   another. The existing burst primitive is now reused for one coherent
   four-byte transfer. A fake-transport test asserts one transaction and correct
   decode.
2. **Raw FLAGS cache divergence.** Generic `readRegister16(REG_FLAGS)` and
   register blocks spanning FLAGS have the same clear-on-read hardware effect as
   `readFlags()`, but previously left cached readiness set. Nonzero generic FLAGS
   writes likewise clear ready in hardware. All paths now clear readiness
   evidence. FLAGS was removed from the configuration-dirty classification.
3. **Impossible PicoStar INT configuration.** PicoStar has no INT pin. Begin now
   rejects an INT hook for PicoStar, runtime package switching cannot create the
   same contradiction, and output interrupt presets return `INVALID_CONFIG`
   without bus traffic.
4. **Numeric sentinel mismatch.** Invalid range/time/mode enum casts could return
   plausible scale or timing data contrary to public documentation. Range and
   resolution helpers now return NaN; invalid/non-one-shot budget requests
   return zero.
5. **Native IDF conversion-time ambiguity.** Values such as `ctime 6` were
   interpreted as rounded milliseconds (`6.5 ms`, enum 4) although the CLI
   advertised enum `0..11` (`6` means 25 ms). Parsing now follows the advertised
   enum contract.
6. **Duplicated diagnostic mappings.** Status/state switches in example code
   could drift. `errorName()`, `driverStateName()`, and `toString()` are now
   library-owned constexpr helpers with stable unknown fallbacks and invalid-cast
   tests.

### Deliberately retained

- Raw register writes remain diagnostic operations and can dirty the
  hardware/cache relation for configuration and threshold registers. The API
  makes the dirty state observable and requires recover/reapply.
- When I2C burst is explicitly disabled, a single sample necessarily uses the
  non-burst access path. Applications that require atomic RESULT fields should
  leave burst enabled (the default).
- Core transport remains injected and non-owning; bus locks, pins, device
  handles, scheduler policy, and recovery policy remain application-owned.

## Bounded-Control-Flow Audit

Every driver loop was reviewed, including the previously flagged loops near the
poll/read/config/reset helpers:

- The public `poll()` loop is limited by the caller-provided instruction budget
  and exits when a state step makes no progress.
- The read-job `while (true)` iterates a finite enum state machine; each branch
  consumes an instruction, advances state, or returns.
- Blocking read loops use a wrap-safe elapsed/deadline test **and** a hard poll
  limit, so a stalled clock hook cannot wait forever.
- Threshold-encoding search is bounded to exponent `0..15`.
- Configuration and reset/reapply loops consume a finite instruction budget and
  stop immediately on failure.
- CLI loops are application-owned. Stress/read/watch counts, intervals, burst
  polling, line buffers, and register dumps are explicitly bounded.

No unbounded loop or retry was found in library steady paths.

## Workspace Parity Review

Read-only comparison used the mature local LDC1614, INA228, INA3221, BME280,
RV3032-C7, MB85RC, PCA9555 and SSD1315 repositories. Device-specific behavior
was preserved, while OPT4001 was aligned on:

- structured `Status`, stable enum names, four-state health, tracked/raw
  transport separation, and manual recover ownership;
- framework-neutral core and native `app_main` ESP-IDF boundary;
- fixed-buffer, colorized, sectioned CLI diagnostics with bounded parsing;
- Arduino/native-IDF command coverage for lifecycle, selected address/package,
  data/FIFO, configuration, thresholds/interrupts, registers, scaling/timing,
  health/recovery, self-test and stress/watch workflows;
- repository command-contract checks; and
- reproducible PlatformIO pins: pioarduino `55.03.311` and
  `platformio/native@1.2.1`. CI additionally pins PlatformIO Core `6.1.19`,
  Ubuntu 24.04, and every third-party action to a full commit SHA.

## Validation Run

Fresh results captured on 2026-08-07:

- `python tools/check_core_timing_guard.py`
- `python tools/check_cli_contract.py`
- `python tools/check_idf_example_contract.py`
- `python tools/check_version_header_contract.py`
- `python tools/check_readiness_claims.py`
- `python tools/check_public_api_docs.py`
- `python scripts/generate_version.py check`
- `doxygen Doxyfile` (warning-free)
- `.\scripts\pio.cmd test -e native`
- `.\scripts\pio.cmd run -e esp32s3dev`
- `.\scripts\pio.cmd run -e esp32s2dev`
- `.\scripts\pio.cmd pkg pack`

All commands above passed. Native tests passed **118/118**. Arduino firmware
built successfully for `esp32s3dev` and `esp32s2dev` using pioarduino
`55.03.311`; the package tarball was created successfully and then removed from
the worktree. The HIL parser self-test and its 15 host parser tests also passed.

Native ESP-IDF `idf.py` target builds remain an open local gate when ESP-IDF is
not available on PATH; CI is configured for both targets, but the static
contract checker is not a substitute for a captured build result.

## Open HIL / Release Gates

- Real-device begin/read and DEVICE_ID evidence for both package profiles.
- DTS address strap matrix `0x44/0x45/0x46`.
- Optical comparison and enclosure/window compensation characterization.
- Conversion-time, quick-wake, one-shot, forced-auto and continuous timing.
- FIFO physical order/four-sample cadence and counter wrap.
- Threshold, ready-pulse and FIFO-full INT polarity/latch/pulse captures.
- NACK/timeout/unplug/brownout/stuck-bus/OFFLINE/recover evidence.
- Explicit approval and isolation before exercising general-call reset.
- Successful pure ESP-IDF `esp32s2` and `esp32s3` builds.
- Formal release review/tag after the above evidence policy is satisfied.
