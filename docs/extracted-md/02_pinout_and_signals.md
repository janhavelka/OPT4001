# OPT4001 pinout and signals

## PicoStar 4-pin package

Source: OPT4001 datasheet, p. 4.

| Pin | Name | Type | Driver relevance |
| --- | --- | --- | --- |
| A1 | GND | Power | Ground. |
| B1 | VDD | Power | Device supply, 1.6 V to 3.6 V. |
| A2 | SCL | Digital input | I2C clock; pull up to 1.6 V to 5.5 V rail. |
| B2 | SDA | Digital I/O | I2C data; pull up to 1.6 V to 5.5 V rail. |

PicoStar has no address-selection pin and no INT pin. The datasheet states the address is hard-coded to `0x45`.

Source: OPT4001 datasheet, p. 20.

## SOT-5X3 / DTS 8-pin package

Source: OPT4001 datasheet, p. 4.

| Pin | Name | Type | Driver relevance |
| --- | --- | --- | --- |
| 1 | VDD | Power | Device supply, 1.6 V to 3.6 V. |
| 2 | ADDR | Digital input | Selects low address bits. |
| 3 | NC | No connect | No driver role. |
| 4 | GND | Power | Ground. |
| 5 | SCL | Digital input | I2C clock; pull up to 1.6 V to 5.5 V rail. |
| 6 | NC | No connect | No driver role. |
| 7 | INT | Digital I/O | Open-drain interrupt input/output; can also trigger one-shot measurements. |
| 8 | SDA | Digital I/O | I2C data; pull up to 1.6 V to 5.5 V rail. |

## Address selection

Source: OPT4001 datasheet, p. 20.

| Variant | ADDR connection | 7-bit address |
| --- | --- | --- |
| PicoStar | Fixed | `0x45` |
| SOT-5X3 | GND | `0x44` |
| SOT-5X3 | VDD | `0x45` |
| SOT-5X3 | SDA | `0x46` |
| SOT-5X3 | SCL | `0x45` as printed in the PDF |

The SOT-5X3 table in the PDF lists the SCL row as `1000101`, duplicating VDD. This compact note records the printed value and leaves the ambiguity in `08_variant_differences_and_open_questions.md`.

## Signal notes

- `SDA`, `SCL`, and SOT-5X3 `INT` use pullups; typical application text uses 10 kOhm examples.
- `INT` direction is controlled by `INT_DIR` and only exists on SOT-5X3.
- The optical sensing direction and package mounting differ by variant; keep package selection visible in board examples.
