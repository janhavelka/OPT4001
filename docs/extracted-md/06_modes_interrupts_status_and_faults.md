# OPT4001 modes, interrupts, status, and faults

## Operating modes

Source: OPT4001 datasheet, pp. 13-14, 30.

| `OPERATING_MODE` | Mode | Behavior |
| --- | --- | --- |
| 0 | Power-down | No active light sensing; device still responds to I2C. |
| 1 | Forced auto-range one-shot | One-shot trigger forces a full auto-range reset cycle; account for about 500 us extra auto-range time. |
| 2 | One-shot | One measurement per trigger without forced full auto-range reset. |
| 3 | Continuous | Device continuously measures and updates output registers based on conversion time. |

## Triggering one-shot measurements

- Register trigger: write `OPERATING_MODE` to 1 or 2.
- Hardware trigger: SOT-5X3 `INT` can be configured as input with `INT_DIR=0`; no hardware interrupt output is available while used as trigger input.
- `QWAKE` can reduce wake time in one-shot use at a power cost.

Source: OPT4001 datasheet, pp. 13-14, 30-31.

## Threshold flags and INT

Threshold logic compares measured light against low and high threshold registers. `FAULT_COUNT` selects how many consecutive fault events are required before `FLAG_H`, `FLAG_L`, and SOT-5X3 `INT` behavior assert.

Source: OPT4001 datasheet, pp. 13-15.

| Field | Behavior |
| --- | --- |
| `LATCH=0` | Flags/INT are transparent and can return inactive when measurements return inside thresholds. |
| `LATCH=1` | Flags/INT remain active until register `0x0C` is read. |
| `INT_CFG=0` | INT follows SMBus alert / threshold behavior. |
| `INT_CFG=1` | INT pulses after every conversion. |
| `INT_CFG=3` | INT pulses after every 4 conversions to indicate FIFO full. |

## FIFO behavior

The main output registers plus FIFO registers act like a four-sample history. FIFO slots let the controller read recent measurements more slowly than the sensor samples. `I2C_BURST` reduces overhead by auto-incrementing the pointer across contiguous output/FIFO registers.

Source: OPT4001 datasheet, pp. 12, 23, 25-28, 31.

## Status flags

| Flag | Driver use |
| --- | --- |
| `OVERLOAD_FLAG` | Report saturation or select a higher fixed range / auto-range. |
| `CONVERSION_READY_FLAG` | Poll one-shot or conversion completion. |
| `FLAG_H` | High-threshold event. |
| `FLAG_L` | Low-threshold event. |

`CONVERSION_READY_FLAG` clears when register `0x0C` is read or written with a nonzero value.

Source: OPT4001 datasheet, pp. 31-32.
