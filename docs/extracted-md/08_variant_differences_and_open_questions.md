# OPT4001 variants and source caveats

## Variant differences

Source: OPT4001 datasheet, pp. 1, 4, 20, 29.

| Topic | PicoStar | SOT-5X3 |
| --- | --- | --- |
| Package pins | 4 pins: VDD, GND, SCL, SDA | 8 pins: VDD, GND, SCL, SDA, ADDR, INT, NC, NC |
| Address | Fixed `0x45` | ADDR-selected table in datasheet |
| INT pin | Not available | Available as open-drain input/output |
| Low-end resolution | 312.5 ulux | 437.5 ulux |
| Full-scale range | Up to 83 klux | Up to 117 klux |
| Lux/code | 312.5e-6 lux/code | 437.5e-6 lux/code |

## Address-table caveat

The datasheet PDF lists the SOT-5X3 `SCL` address row as `1000101` (`0x45`), which duplicates the `VDD` row. The same value appears in the raw extraction and in the rendered PDF page. Treat this as an unresolved source issue for hardware using `ADDR=SCL`.

Source: OPT4001 datasheet, p. 20.

## Datasheet facts used by this repo

| Topic | Datasheet fact |
| --- | --- |
| Register width | 16 bits. |
| Byte order | Big-endian, MSB first. |
| Package-address behavior | PicoStar fixed `0x45`; SOT-5X3 uses ADDR table with the `ADDR=SCL` caveat above. |
| Burst read | `I2C_BURST` reset value is 1; pointer auto-increments after every 16-bit register read and resets to the original register address on STOP. |
| INT support | SOT-5X3 only; open-drain, application-owned GPIO/ISR. |

## Not documented in PDFs

| Missing or ambiguous fact | Source status |
| --- | --- |
| SOT-5X3 `ADDR=SCL` unique address | The datasheet table prints `1000101b` (`0x45`), duplicating the VDD row; the checked-in PDFs contain no distinct fourth address. |
| A PEC byte or PEC enable register | The OPT4001 I2C transaction figures and register map contain no packet error checking field or byte. |
| CRC polynomial name for result CRC | Register `0x01` documents XOR equations for `CRC[3:0]`, but the checked-in datasheet text does not name a standard CRC polynomial. |
| PicoStar INT behavior | PicoStar has no INT pin in the pin table; INT behavior is SOT-5X3-only in the checked-in datasheet. |

## Validation caveats

| Topic | Status |
| --- | --- |
| Threshold / INT hardware validation | Register packing and driver contracts can be tested without hardware, but physical threshold comparator behavior, SMBus alert arbitration, open-drain pulse timing, and ISR integration remain board-validation items. |
| Dirty hardware/cache state | Raw writes, external resets, brownout, or partial multi-register failures can make cached driver settings differ from hardware registers. Use `recover()` to re-probe/re-apply cached settings, or `resetAndReapply()` when a bus-wide reset is acceptable. |
