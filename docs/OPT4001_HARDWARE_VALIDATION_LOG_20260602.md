# OPT4001 Hardware Validation Log - 2026-06-02

## Session Status

Status: not run; hardware validation remains pending.

Reason: hardware validation requires a connected OPT4001 board and operator
metadata before any device, optical, INT, FIFO, address, or fault validation is
run. No board/package/operator/serial metadata was provided in this session, so
no hardware commands were executed and no results are claimed.

## Required Operator Metadata

| Field | Value |
| --- | --- |
| Date/time | 2026-06-02 22:10:17 +02:00 |
| Operator | Not provided |
| Board name/revision | Not provided |
| MCU board/target | Not provided |
| Firmware commit | Not provided |
| Library commit | `c7f15d7365f4278c9768e85af42479dafb2e188f` |
| OPT4001 package variant | Not provided |
| I2C address wiring | Not provided |
| INT pin connected | Not provided |
| Reference lux meter available | Not provided |
| Optical setup | Not provided |
| Serial port and baud | Not provided |

## Tests Run

No tests were run.

| Area | Result |
| --- | --- |
| Safe smoke sequence | Not run; missing hardware/operator metadata. |
| Freshness / conversion-time sequence | Not run; missing hardware/operator metadata. |
| Continuous freshness sequence | Not run; missing hardware/operator metadata. |
| FIFO / burst sequence | Not run; missing hardware/operator metadata. |
| Address/package matrix | Not run; package/address wiring not provided. |
| Optical validation | Not run; board and reference-meter setup not provided. |
| INT validation | Not run; package variant, INT wiring, and capture setup not provided. |
| Fault/recovery validation | Not run; no operator approval for fault tests. |

## Raw Logs

No serial/HIL logs were captured.

No files under `hil_logs/` were generated or committed.

## Verdict

No hardware validation evidence was captured. Hardware, optical, INT, FIFO
timing/order, address-pin, and fault/recovery validation remain open.

## Required Inputs For Next Hardware Run

- Operator name.
- Board name/revision and MCU target.
- Firmware commit flashed to the board.
- OPT4001 package variant: PicoStar/YMN or SOT-5X3/DTS.
- I2C address wiring and expected address.
- Whether INT is connected and how it will be captured.
- Whether a calibrated or reference lux meter is available.
- Optical setup details.
- Serial port and baud.
- Explicit approval before any fault/recovery tests beyond safe smoke checks.
