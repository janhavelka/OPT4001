# OPT4001 protocol, commands, and transactions

The OPT4001 is an I2C/SMBus target device. It supports standard mode up to 100 kHz, fast mode up to 400 kHz, and high-speed mode up to 2.6 MHz. It uses an 8-bit register address followed by 16-bit register data, transferred MSB first.

Source: OPT4001 datasheet, pp. 20-24.

## Address byte

PicoStar has a fixed 7-bit address of `0x45`. SOT-5X3 samples `ADDR` on every bus communication: GND selects `0x44`, VDD selects `0x45`, SDA selects `0x46`, and the PDF table prints SCL as `1000101b` (`0x45`) as noted in `08_variant_differences_and_open_questions.md`. The R/W bit follows the 7-bit address.

Source: OPT4001 datasheet, p. 20.

## Register access model

| Operation | Bus sequence |
| --- | --- |
| Set pointer | START, address+W, register address, STOP or repeated START |
| Write register | START, address+W, register address, data MSB, data LSB, STOP |
| Read current pointer | START, address+R, data MSB, data LSB, STOP |
| Read specific register | START, address+W, register address, repeated START, address+R, data MSB, data LSB, STOP |

## Burst read

When `I2C_BURST` is set, the register pointer auto-increments after each 16-bit register read. When STOP is issued, the pointer resets to the original register address before auto-increment. This is useful for reading output and FIFO registers with less bus overhead.

Source: OPT4001 datasheet, pp. 23, 31.

## High-speed I2C

High-speed mode entry is part of the OPT4001 bus protocol:

- While the bus is idle, the controller sends START plus high-speed controller code `00001XXXb`.
- That controller-code byte is sent in standard or fast mode at up to 400 kHz.
- OPT4001 does not acknowledge the controller code, but switches internal filters for 2.6 MHz operation after recognizing it.
- The controller sends a repeated START, then uses the same register format at up to 2.6 MHz.
- Repeated START conditions keep the device in high-speed mode; STOP ends high-speed mode and returns the filters to fast/standard mode.

Source: OPT4001 datasheet, pp. 22-23.

## General-call reset

The device responds to I2C general call address `0x00` followed by reset command `0x06`. This returns registers to the power-on-reset default condition.

Source: OPT4001 datasheet, p. 24.

## SMBus alert response

OPT4001 responds to SMBus Alert Response only in latched window-style comparison mode; it does not respond in transparent mode. When the controller broadcasts the alert-response address, alerting targets acknowledge and send their I2C address. The lowest address wins arbitration. If OPT4001 loses arbitration, INT remains active until a later alert-response transaction. When OPT4001 wins, it acknowledges and sets INT inactive. The datasheet notes that `FLAG_H` is sent as the final LSB of the address in this process.

Source: OPT4001 datasheet, p. 24.

## Register transaction facts

- Every register in the documented map is 16 bits.
- A read uses the last register address written; repeated reads from the same register can omit the register-address write until the address changes.
- Every register write includes the register address byte and two data bytes.
