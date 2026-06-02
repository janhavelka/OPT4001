# OPT4001 initialization, reset, and operational notes

## Reset behavior

The device supports I2C general-call reset using address `0x00` followed by command `0x06`, returning registers to power-on-reset defaults.

Source: OPT4001 datasheet, p. 24.

## Practical initialization sequence

1. Select package variant in driver configuration: PicoStar or SOT-5X3.
2. Select I2C address. PicoStar uses fixed `0x45`; SOT-5X3 uses the ADDR table with the source caveat for the SCL row.
3. Optionally read ID register `0x11`.
4. Program thresholds `0x08` and `0x09` if threshold flags or INT are used.
5. Program `0x0B` for required reserved pattern, INT direction/mode, and burst behavior.
6. Program `0x0A` for range, conversion time, operating mode, latch/polarity, and fault count.
7. For one-shot mode, trigger a conversion and poll `CONVERSION_READY_FLAG`.
8. Read `0x00` and `0x01` together; optionally burst-read FIFO registers
   `0x02` through `0x07` for the older shadow samples.

Source: OPT4001 datasheet, pp. 20-32.

## Result conversion

Main output:

- `EXPONENT` is bits 15:12 of register `0x00`.
- `RESULT_MSB` is bits 11:0 of register `0x00`.
- `RESULT_LSB` is bits 15:8 of register `0x01`.
- `MANTISSA = (RESULT_MSB << 8) + RESULT_LSB`.

The datasheet represents output as exponent plus 20-bit mantissa and also defines linear ADC-code conversion:

- PicoStar: `lux = ADC_CODES * 312.5e-6`.
- SOT-5X3: `lux = ADC_CODES * 437.5e-6`.

Source: OPT4001 datasheet, pp. 25-26, 29.

## Operational cautions

- Register `0x0B` bits 15:5 have the required write pattern `0x400`, and bit 1 has the required write value 0.
- `INT` exists only on SOT-5X3.
- `INT` is open-drain and application-owned. The driver core does not configure
  GPIO, attach ISRs, debounce, drive INT, or own the pin.
- If `INT_DIR=0`, INT is an input trigger and cannot simultaneously report interrupts.
- `I2C_BURST` defaults enabled per the register extract; the same datasheet section also documents normal non-burst reads.
- Keep package-specific lux scaling visible in API configuration.
- Reading FLAGS register `0x0C` clears the latched flags view; writing nonzero
  to `0x0C` clears `CONVERSION_READY_FLAG`. Use status reads and raw block reads
  that include `0x0C` only when that side effect is intended.
- A dirty hardware/cache state means cached driver settings may not match
  registers after raw writes, brownout, external/general-call reset, or partial
  multi-register configuration failure. Recover by stopping dependent
  threshold/INT/freshness workflows, calling `recover()` to re-probe and
  re-apply cached settings, then reading back configuration, INT configuration,
  and thresholds when confirmation is needed. Use `resetAndReapply()` when a
  bus-wide reset is acceptable.
- Register-level threshold/INT packing is testable without hardware, but final
  threshold comparator, SMBus alert, INT pulse, and ISR behavior need
  target-board validation.
