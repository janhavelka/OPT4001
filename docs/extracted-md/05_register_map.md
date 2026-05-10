# OPT4001 register map

Source: OPT4001 datasheet, pp. 25-32.

All documented registers are 16 bits.

| Address | Register content | Reset | Driver notes |
| --- | --- | --- | --- |
| `0x00` | `EXPONENT`, `RESULT_MSB` | `0x0000` | Main output high register. |
| `0x01` | `RESULT_LSB`, `COUNTER`, `CRC` | `0x0000` | Main output low register plus sample counter and CRC nibble. |
| `0x02` | `EXPONENT_FIFO0`, `RESULT_MSB_FIFO0` | `0x0000` | FIFO slot 0 high. |
| `0x03` | `RESULT_LSB_FIFO0`, `COUNTER_FIFO0`, `CRC_FIFO0` | `0x0000` | FIFO slot 0 low. |
| `0x04` | `EXPONENT_FIFO1`, `RESULT_MSB_FIFO1` | `0x0000` | FIFO slot 1 high. |
| `0x05` | `RESULT_LSB_FIFO1`, `COUNTER_FIFO1`, `CRC_FIFO1` | `0x0000` | FIFO slot 1 low. |
| `0x06` | `EXPONENT_FIFO2`, `RESULT_MSB_FIFO2` | `0x0000` | FIFO slot 2 high. |
| `0x07` | `RESULT_LSB_FIFO2`, `COUNTER_FIFO2`, `CRC_FIFO2` | `0x0000` | FIFO slot 2 low. |
| `0x08` | `THRESHOLD_L_EXPONENT`, `THRESHOLD_L_RESULT` | `0x0000` | Low threshold. |
| `0x09` | `THRESHOLD_H_EXPONENT`, `THRESHOLD_H_RESULT` | `0xBFFF` | High threshold. |
| `0x0A` | `QWAKE`, `RANGE`, `CONVERSION_TIME`, `OPERATING_MODE`, `LATCH`, `INT_POL`, `FAULT_COUNT` | `0x3208` | Main configuration register. |
| `0x0B` | Reserved pattern, `INT_DIR`, `INT_CFG`, `I2C_BURST` | `0x8011` | INT direction/mode and burst-read control. |
| `0x0C` | `OVERLOAD_FLAG`, `CONVERSION_READY_FLAG`, `FLAG_H`, `FLAG_L` | `0x0000` | Status flags. |
| `0x11` | Device ID fields | `0x0121` | Identity register per datasheet extraction. |

## Register 0x0A key fields

Source: OPT4001 datasheet, pp. 30-31.

| Bits | Field | Reset | Meaning |
| --- | --- | --- | --- |
| 15 | `QWAKE` | 0 | Faster one-shot wake from standby at higher power. |
| 13:10 | `RANGE` | 12 | Fixed range 0-8 or automatic range 12. |
| 9:6 | `CONVERSION_TIME` | 8 | 0.6 ms to 800 ms conversion time. |
| 5:4 | `OPERATING_MODE` | 0 | 0 power-down, 1 forced auto-range one-shot, 2 one-shot, 3 continuous. |
| 3 | `LATCH` | 1 | Threshold interrupt/flag latching behavior. |
| 2 | `INT_POL` | 0 | INT active-state polarity. |
| 1:0 | `FAULT_COUNT` | 0 | Consecutive threshold events required before flags/INT assert. |

## Register 0x0B key fields

Source: OPT4001 datasheet, p. 31.

| Bits | Field | Reset | Meaning |
| --- | --- | --- | --- |
| 15:5 | Required pattern | `0x400` field value | Must read/write decimal 1024 (`0x400`) in this field. |
| 4 | `INT_DIR` | 1 | 0 input, 1 output. SOT-5X3 only. |
| 3:2 | `INT_CFG` | 0 | 0 SMBus alert, 1 pulse every conversion, 2 invalid, 3 pulse every 4 conversions/FIFO full. |
| 1 | Reserved | 0 | Must read/write 0. |
| 0 | `I2C_BURST` | 1 | Enables pointer auto-increment during reads. |

## Status flags

Source: OPT4001 datasheet, pp. 31-32.

| Field | Meaning |
| --- | --- |
| `OVERLOAD_FLAG` | Conversion exceeded full-scale light range. |
| `CONVERSION_READY_FLAG` | Set when conversion completes; cleared when register `0x0C` is read or written with nonzero value. |
| `FLAG_H` | Result exceeded high threshold. |
| `FLAG_L` | Result fell below low threshold. |

## Documented reserved and required-write behavior

| Register | Bits | Datasheet behavior |
| --- | --- | --- |
| `0x0A` | Bit 14 | Must read or write 0. |
| `0x0B` | Bits 15:5 | Must read or write 1024 (`0x400` field value). |
| `0x0B` | Bit 1 | Must read or write 0. |
| `0x0C` | Bits 15:4 | Must read or write 0. |
| `0x11` | Bits 15:14 | Must read or write 0. |

Source: OPT4001 datasheet, pp. 30-32.
