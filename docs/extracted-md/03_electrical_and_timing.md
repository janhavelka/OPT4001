# OPT4001 electrical and timing notes

## Operating limits relevant to software

| Parameter | Value | Source |
| --- | --- | --- |
| Recommended VDD | 1.6 V to 3.6 V | Datasheet, pp. 4-5 |
| Digital I/O tolerance / pullups | I/O pins tolerate up to 5.5 V pullups; this does not make the sensor a 5 V powered part | Datasheet, p. 6 |
| Power-on-reset threshold | 0.8 V | Datasheet, p. 6 |
| Standard-mode I2C | Up to 100 kHz | Datasheet, p. 20 |
| Fast-mode I2C | Up to 400 kHz | Datasheet, p. 20 |
| High-speed I2C | Up to 2.6 MHz | Datasheet, p. 20 |
| I2C timeout | 28 ms when SCL is held low | Datasheet, p. 20 |

Software policy: power OPT4001 from the recommended VDD range. Treat 5.5 V I/O
tolerance as a pullup/interface limit only, not as permission to connect VDD to
a 5 V rail.

## Package, address, and scale matrix

| Package variant | Valid I2C addresses | Lux/code for linear ADC codes | INT pin |
| --- | --- | --- | --- |
| PicoStar | `0x45` only | 312.5e-6 lux/code | Not available |
| SOT-5X3 | `0x44`, `0x45`, `0x46` | 437.5e-6 lux/code | Optional, application-owned GPIO |

## Lux range and resolution

Source: OPT4001 datasheet, pp. 1, 5, 16, 29.

| Variant | Effective dynamic range | Lux/code for linear ADC codes |
| --- | --- | --- |
| PicoStar | 312.5 ulux to 83 klux | 312.5e-6 lux/code |
| SOT-5X3 | 437.5 ulux to 117 klux | 437.5e-6 lux/code |

Result register software contract:

- Samples use a 4-bit exponent and a 20-bit mantissa from `RESULT` and
  `RESULT_LSB_CRC`.
- Public raw conversion helpers accept exponent `0..8` and mantissa
  `0x00000..0xFFFFF`.
- Linear ADC codes are `mantissa << exponent`, computed with `uint64_t`
  intermediates before lux scaling.
- Status-returning helpers reject invalid fields with `INVALID_PARAM`.
  Compatibility float helpers return quiet NaN for invalid raw fields.

## Fixed range settings

Source: OPT4001 datasheet, pp. 16, 30.

| `RANGE` | PicoStar full scale | SOT-5X3 full scale |
| --- | --- | --- |
| 0 | 328 lux | 459 lux |
| 1 | 655 lux | 918 lux |
| 2 | 1311 lux | 1835 lux |
| 3 | 2621 lux | 3670 lux |
| 4 | 5243 lux | 7340 lux |
| 5 | 10486 lux | 14680 lux |
| 6 | 20972 lux | 29360 lux |
| 7 | 41943 lux | 58720 lux |
| 8 | 83886 lux | 117441 lux |
| 12 | Automatic | Automatic |

## Threshold conversion

Source: OPT4001 datasheet, pp. 23, 29.

Threshold registers use a 4-bit exponent and 12-bit result. The exact linear
threshold ADC-code value is:

```text
adc_codes = result << (8 + exponent)
```

The maximum register encoding is exponent `15`, result `0x0FFF`, which equals
`34351349760` ADC codes. This exceeds `uint32_t`, so the lossless API returns
threshold ADC codes through a `uint64_t&` output. The legacy `uint32_t`
compatibility helper saturates at `UINT32_MAX` instead of wrapping.

`luxToThreshold()` rounds requested lux to the nearest ADC code before
quantizing to the threshold register format. It rejects negative, non-finite,
and out-of-range lux values.

| Threshold case | Exponent | Result | ADC codes | PicoStar lux | SOT-5X3 lux |
| --- | ---: | ---: | ---: | ---: | ---: |
| Reset low | 0 | `0x000` | 0 | 0 | 0 |
| Smallest nonzero | 0 | `0x001` | 256 | 0.08 | 0.112 |
| Exp0 max | 0 | `0xFFF` | 1048320 | 327.6 | 458.64 |
| Exp1 mid | 1 | `0x800` | 1048576 | 327.68 | 458.752 |
| Reset high | 11 | `0xFFF` | 2146959360 | 670924.8 | 939294.72 |
| Max register | 15 | `0xFFF` | 34351349760 | 10734796.8 | 15028715.52 |

## CRC behavior

Source: OPT4001 datasheet, pp. 22-23.

CRC verification uses the datasheet XOR equations over `EXPONENT`, 20-bit
`MANTISSA`, and 4-bit `COUNTER`. With verification enabled, a mismatch returns
`CRC_ERROR` while preserving decoded sample fields and the received CRC nibble
for diagnostics. With verification disabled, the sample status is `OK` and
`Sample::crcValid` remains false.

## Conversion time settings

Source: OPT4001 datasheet, pp. 17, 30.

| `CONVERSION_TIME` | Typical time us | Ceil ms used by helpers | Effective bits |
| --- | ---: | ---: | ---: |
| 0 | 600 | 1 | 9 |
| 1 | 1000 | 1 | 10 |
| 2 | 1800 | 2 | 11 |
| 3 | 3400 | 4 | 12 |
| 4 | 6500 | 7 | 13 |
| 5 | 12700 | 13 | 14 |
| 6 | 25000 | 25 | 15 |
| 7 | 50000 | 50 | 16 |
| 8 | 100000 | 100 | 17 |
| 9 | 200000 | 200 | 18 |
| 10 | 400000 | 400 | 19 |
| 11 | 800000 | 800 | 20 |
