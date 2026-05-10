# OPT4001 chip overview

The OPT4001 is a digital ambient-light sensor that reports light level over an I2C/SMBus-compatible interface. It supports continuous and one-shot measurements, automatic or fixed light-range selection, programmable conversion time, threshold flags, and output-register FIFO behavior. The SOT-5X3 package adds an `ADDR` pin and an `INT` pin.

Source: OPT4001 datasheet, pp. 1, 10-16.

## Driver-facing capabilities

| Capability | OPT4001 facts to model | Source |
| --- | --- | --- |
| Lux output from exponent plus mantissa | `EXPONENT`, `RESULT_MSB`, and `RESULT_LSB` form the sample value used for lux conversion. | Datasheet, pp. 25-26, 29 |
| Package-dependent lux scale | PicoStar uses 312.5e-6 lux/code; SOT-5X3 uses 437.5e-6 lux/code. | Datasheet, p. 29 |
| Automatic or fixed range | `RANGE=12` selects automatic full-scale range; 0-8 select fixed ranges. | Datasheet, pp. 16, 30 |
| Conversion time selection | 0.6 ms through 800 ms. | Datasheet, pp. 17, 30 |
| FIFO/shadow output registers | Output plus three FIFO slots allow slower host reads while preserving recent samples. | Datasheet, pp. 12, 25-28 |
| Threshold flags and INT | Threshold comparison drives `FLAG_H`, `FLAG_L`, and optionally SOT-5X3 `INT`. | Datasheet, pp. 13-15, 31-32 |
| I2C burst read | `I2C_BURST` enables register-pointer auto-increment during reads. | Datasheet, pp. 23, 31 |

## Implementation facts tied to the datasheet

- Every documented register is 16 bits and is transferred MSB first.
- PicoStar has fixed address `0x45` and no INT pin; SOT-5X3 has ADDR and INT pins.
- Register `0x0B` contains required write patterns in bits 15:5 and bit 1.
- The supplemental optical/package application notes contain no OPT4001 register defaults or I2C command sequences.
