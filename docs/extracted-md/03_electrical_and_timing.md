# OPT4001 electrical and timing notes

## Operating limits relevant to software

| Parameter | Value | Source |
| --- | --- | --- |
| Recommended VDD | 1.6 V to 3.6 V | Datasheet, pp. 4-5 |
| I2C pullup supply | Up to 5.5 V when VDD <= 1.6 V condition applies in table | Datasheet, p. 6 |
| Power-on-reset threshold | 0.8 V | Datasheet, p. 6 |
| Standard-mode I2C | Up to 100 kHz | Datasheet, p. 20 |
| Fast-mode I2C | Up to 400 kHz | Datasheet, p. 20 |
| High-speed I2C | Up to 2.6 MHz | Datasheet, p. 20 |
| I2C timeout | 28 ms when SCL is held low | Datasheet, p. 20 |

## Lux range and resolution

Source: OPT4001 datasheet, pp. 1, 5, 16, 29.

| Variant | Effective dynamic range | Lux/code for linear ADC codes |
| --- | --- | --- |
| PicoStar | 312.5 ulux to 83 klux | 312.5e-6 lux/code |
| SOT-5X3 | 437.5 ulux to 117 klux | 437.5e-6 lux/code |

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

## Conversion time settings

Source: OPT4001 datasheet, pp. 17, 30.

| `CONVERSION_TIME` | Typical time |
| --- | --- |
| 0 | 0.6 ms |
| 1 | 1 ms |
| 2 | 1.8 ms |
| 3 | 3.4 ms |
| 4 | 6.5 ms |
| 5 | 12.7 ms |
| 6 | 25 ms |
| 7 | 50 ms |
| 8 | 100 ms |
| 9 | 200 ms |
| 10 | 400 ms |
| 11 | 800 ms |
