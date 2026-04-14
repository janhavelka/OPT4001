# OPT4001 — High Speed, High Precision Digital Ambient Light Sensor — Implementation Manual

> **Source:** Texas Instruments OPT4001 Datasheet (SBOS993A, Rev. December 2022)  
> **Relevance:** Complete register-level, timing, electrical, and algorithmic reference for building a production-grade OPT4001 driver.

---

## Key Takeaways

- Two package variants: **PicoStar™ (4-pin, fixed addr 0x45)** and **SOT-5X3 (8-pin, configurable addr)**.
- Semi-logarithmic output: **4-bit EXPONENT + 20-bit MANTISSA** = 28-bit effective dynamic range.
- **12 conversion times** from 600 µs to 800 ms; **9 full-scale ranges** plus auto-range mode.
- Lux calculated as: `ADC_CODES = MANTISSA << EXPONENT; lux = ADC_CODES × LSB_SIZE`.
- 4-deep output **FIFO** with I²C burst read support for efficient multi-sample readout.
- **CRC** on every output register for transmission error detection.
- **28 ms I²C bus timeout** built-in — prevents bus lockup.
- Operating modes: power-down, continuous, one-shot forced auto-range, one-shot regular.
- INT pin (SOT-5X3 only): configurable as input (hardware trigger) or output (interrupt/SMBUS alert).
- Device starts in **power-down mode** after reset — must be explicitly configured and started.
- General-call reset: address 0x00, data 0x06 — resets device to power-on defaults.
- Register 0x0B has a fixed pattern: bits [15:5] **must read/write 1024 (0x400)**.
- Device ID register 0x11: DIDH = 0x121 (bits[11:0]), DIDL = 0x0 (bits[13:12]).

---

## 1. Device Overview

| Parameter | Value |
|---|---|
| Sensor type | Ambient light sensor (photopic, human-eye matched) |
| Output | Digital I²C, semi-logarithmic lux |
| Supply voltage (VDD) | 1.6 V – 3.6 V |
| I²C pull-up voltage | 1.6 V – 5.5 V (5V tolerant I/O) |
| Active current | 30 µA (typ, full-scale lux) |
| Standby current | 2 µA (typ) |
| Operating temperature | –40 °C to +85 °C |
| Peak spectral responsivity | 550 nm |
| IR rejection (850 nm) | 0.2% |
| POR threshold | 0.8 V |

### Package Variants

| Feature | PicoStar™ (YMN) | SOT-5X3 (DTS) |
|---|---|---|
| Pins | 4 | 8 |
| Size | 0.84 × 1.05 × 0.226 mm | 2.1 × 1.9 × 0.6 mm |
| Sensor facing | Bottom | Top |
| I²C address | Fixed: 0x45 | Configurable (ADDR pin) |
| INT pin | No | Yes |
| ADDR pin | No | Yes |
| Resolution (800 ms) | 312.5 µlux | 437.5 µlux |
| Full-scale lux | 83,886 lux | 117,441 lux |
| Angular response (FWHM) | 96° | 120° |
| Lux LSB | 312.5 × 10⁻⁶ | 437.5 × 10⁻⁶ |
| Drift across temperature | 0.01 %/°C | 0.015 %/°C |

---

## 2. Electrical Specifications

### Absolute Maximum Ratings

| Parameter | Min | Max | Unit |
|---|---|---|---|
| VDD to GND | –0.5 | 6 | V |
| SDA/SCL to GND | –0.5 | 6 | V |
| Current into any pin | — | 10 | mA |
| Junction temperature | — | 150 | °C |
| Storage temperature | –65 | 150 | °C |

**Warning:** Long exposure to temperatures above 105 °C can cause package discoloration, spectral distortion, and measurement inaccuracy.

### Recommended Operating Conditions

| Parameter | Min | Nom | Max | Unit |
|---|---|---|---|---|
| VDD | 1.6 | — | 3.6 | V |
| Junction temperature | –40 | — | 85 | °C |

### ESD Ratings

| Model | Value |
|---|---|
| HBM (all pins) | ±2000 V |
| CDM (all pins) | ±500 V |

### Thermal Information

| Metric | PicoStar™ (YMN) | SOT-5X3 (DTS) | Unit |
|---|---|---|---|
| RθJA (junction-to-ambient) | 122.8 | 112.2 | °C/W |
| RθJC(top) (junction-to-case top) | 1.4 | 28.4 | °C/W |
| RθJB (junction-to-board) | 34.9 | 22.1 | °C/W |
| ΨJT (junction-to-top char.) | 0.8 | 1.2 | °C/W |
| ΨJB (junction-to-board char.) | 35.3 | 22.0 | °C/W |

### Power Consumption

| Mode | Condition | Current |
|---|---|---|
| Active | Dark | 22 µA |
| Active | Full-scale lux | 30 µA |
| Standby | Dark | 1.6 µA |
| Standby | Full-scale lux | 2 µA |

### Digital I/O Levels

| Parameter | Min | Max | Unit |
|---|---|---|---|
| VIL (SDA, SCL, ADDR) | 0 | 0.3 × VDD | V |
| VIH (SDA, SCL, ADDR) | 0.7 × VDD | 5.5 | V |
| VOL (SDA, INT) @ IOL=3mA | — | 0.32 | V |
| IIL (low-level input current, SDA/SCL/ADDR) | 0.01 | 0.25 | µA |
| IZH (high-Z leakage, SDA/INT) | 0.01 | 0.25 | µA |
| I/O pin capacitance | — | 3 | pF |
| Trigger-to-sample start (Tss) | — | 0.5 | ms |

**Note:** Specified leakage current maxima (IIL, IZH) are dominated by test equipment limitations; typical values are much smaller.

### Key Optical Performance

| Parameter | Value |
|---|---|
| PSRR | 0.1 %/V |
| Light source variation (incandescent/halogen/fluorescent) | 4% |
| Relative accuracy between gain ranges | 0.4% |
| Dark measurement | 0–10 mlux |
| Linearity (>328/459 lux, 100 ms) | 2% |
| Linearity (<328/459 lux, 100 ms) | 5% |

---

## 3. Pin Configuration

### PicoStar™ (YMN) — 4-Pin, Bottom View

| Pin | Name | Type | Description |
|---|---|---|---|
| A1 | GND | Power | Ground |
| B1 | VDD | Power | 1.6–3.6 V supply |
| A2 | SCL | Digital input | I²C clock |
| B2 | SDA | Digital I/O | I²C data |

### SOT-5X3 (DTS) — 8-Pin, Top View

| Pin | Name | Type | Description |
|---|---|---|---|
| 1 | VDD | Power | 1.6–3.6 V supply |
| 2 | ADDR | Digital input | Address select pin |
| 3 | NC | — | No connection |
| 4 | GND | Power | Ground |
| 5 | SCL | Digital input | I²C clock |
| 6 | NC | — | No connection |
| 7 | INT | Digital I/O | Interrupt (open-drain) |
| 8 | SDA | Digital I/O | I²C data |

---

## 4. I²C Interface

### Bus Configuration

- Modes: Standard (100 kHz), Fast (400 kHz), High-Speed (2.6 MHz)
- Pull-ups: 10 kΩ typical to VIO (1.6–5.5 V)
- Bus timeout: **28 ms** (if SCL held low, bus state machine resets)
- All registers: 16-bit, MSB-first
- Open-drain outputs (SDA, INT)

### I²C Addresses

**PicoStar™:** Fixed at **0x45** (1000101b)

**SOT-5X3:**

| ADDR Pin | I²C Address (7-bit) | Hex |
|---|---|---|
| GND | 1000100 | 0x44 |
| VDD | 1000101 | 0x45 |
| SDA | 1000110 | 0x46 |
| SCL | 1000101 | 0x45 |

**Note:** ADDR=SCL produces the same address (0x45) as ADDR=VDD, so only **3 unique addresses** (0x44, 0x45, 0x46) are available on the SOT-5X3 variant.

**Note:** ADDR pin state is sampled on every bus communication. It must be stable before I²C activity begins.

### Write Sequence

```
[S] [addr<<1|0] [ACK] [reg_addr] [ACK] [data_MSB] [ACK] [data_LSB] [ACK] [P]
```

### Read Sequence

```
// Set register pointer
[S] [addr<<1|0] [ACK] [reg_addr] [ACK] [P]
// Read data
[S] [addr<<1|1] [ACK] [data_MSB] [ACK] [data_LSB] [NACK] [P]
```

- Register pointer is retained until changed by a write.
- Repeated reads from the same register do not require re-sending the register address.

### Burst Read Mode

When I2C_BURST (register 0x0B, bit 0) is set to 1 (default), the register pointer auto-increments by 1 after each register read. This allows sequential reading of output + FIFO registers in a single I²C transaction:

```
// Set pointer to 0x00, then read 8 consecutive registers (0x00-0x07)
[S] [addr<<1|0] [ACK] [0x00] [ACK] [P]
[S] [addr<<1|1] [ACK] [reg00_MSB] [ACK] [reg00_LSB] [ACK]
                      [reg01_MSB] [ACK] [reg01_LSB] [ACK]
                      ...
                      [reg07_MSB] [NACK] [reg07_LSB] [P]
```

### High-Speed I²C Mode

- Up to **2.6 MHz** SCL clock
- Enter HS mode: controller sends HS master code **0000 1XXXb** (e.g., 0x08) at F/S speed; OPT4001 does **not** ACK but switches internal filters to HS speed
- After HS code + repeated START, F/S protocol is used but at HS clock speeds (up to 2.6 MHz)
- HS mode exits on next STOP condition — device reverts to F/S mode
- SDA/SCL have integrated spike suppression and Schmitt triggers

### General-Call Reset

- Address: **0x00**, Data: **0x06**
- Resets device to power-on defaults
- All registers revert to reset values
- Device enters power-down mode

### SMBus Alert Response

- OPT4001 is compatible with I²C and SMBus protocols
- Device responds to SMBus alert response address when in **latched window-style** comparison mode (LATCH=1)
- Does **NOT** respond to SMBus alert in transparent mode
- Protocol: controller broadcasts alert response target address (0x0C, R/W=1); device responds with its 7-bit address + FLAG_H as the LSB
- If multiple devices assert, bus arbitration applies — lowest address wins
- Losing device keeps INT active for subsequent alert response
- Winning device ACKs and de-asserts INT
- FLAG_H and FLAG_L are **not** cleared by SMBus alert response — must read register 0x0C to clear
- Controller can issue multiple alert responses to clear all devices

---

## 5. Register Map

### Register Summary

| Address | Name | Reset | R/W | Description |
|---|---|---|---|---|
| 0x00 | RESULT | 0x0000 | R | Output: EXPONENT[15:12] + RESULT_MSB[11:0] |
| 0x01 | RESULT_LSB_CRC | 0x0000 | R | RESULT_LSB[15:8] + COUNTER[7:4] + CRC[3:0] |
| 0x02 | FIFO0_MSB | 0x0000 | R | FIFO 0: EXPONENT + RESULT_MSB |
| 0x03 | FIFO0_LSB_CRC | 0x0000 | R | FIFO 0: RESULT_LSB + COUNTER + CRC |
| 0x04 | FIFO1_MSB | 0x0000 | R | FIFO 1: EXPONENT + RESULT_MSB |
| 0x05 | FIFO1_LSB_CRC | 0x0000 | R | FIFO 1: RESULT_LSB + COUNTER + CRC |
| 0x06 | FIFO2_MSB | 0x0000 | R | FIFO 2: EXPONENT + RESULT_MSB |
| 0x07 | FIFO2_LSB_CRC | 0x0000 | R | FIFO 2: RESULT_LSB + COUNTER + CRC |
| 0x08 | THRESHOLD_L | 0x0000 | R/W | Low threshold: EXPONENT[15:12] + RESULT[11:0] |
| 0x09 | THRESHOLD_H | 0xBFFF | R/W | High threshold: EXPONENT[15:12] + RESULT[11:0] |
| 0x0A | CONFIGURATION | 0x3208 | R/W | Main configuration register |
| 0x0B | INT_CONFIGURATION | 0x8011 | R/W | Interrupt and I²C configuration |
| 0x0C | FLAGS | 0x0000 | R/R/W | Status flags |
| 0x11 | DEVICE_ID | 0x0121 | R/R/W | Device identification |

### Register 0x00 — RESULT (Output MSB)

| Bits | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:12 | EXPONENT | R | 0x0 | Full-scale range index (0–8) |
| 11:0 | RESULT_MSB | R | 0x000 | Upper 12 bits of MANTISSA |

### Register 0x01 — RESULT_LSB_CRC

| Bits | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:8 | RESULT_LSB | R | 0x00 | Lower 8 bits of MANTISSA |
| 7:4 | COUNTER | R | 0x0 | Sample counter (0–15, wraps) |
| 3:0 | CRC | R | 0x0 | 4-bit CRC of output |

### Registers 0x02–0x07 — FIFO (3 Shadow Register Pairs)

Same layout as registers 0x00/0x01, containing the previous 3 measurements:
- 0x02/0x03: FIFO 0 (oldest of the three shadows)
- 0x04/0x05: FIFO 1
- 0x06/0x07: FIFO 2

### Register 0x08 — THRESHOLD_L

| Bits | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:12 | THRESHOLD_L_EXPONENT | R/W | 0x0 | Low threshold exponent |
| 11:0 | THRESHOLD_L_RESULT | R/W | 0x000 | Low threshold result (12-bit) |

### Register 0x09 — THRESHOLD_H

| Bits | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:12 | THRESHOLD_H_EXPONENT | R/W | 0xB | High threshold exponent |
| 11:0 | THRESHOLD_H_RESULT | R/W | 0xFFF | High threshold result (12-bit) |

Default high threshold = maximum possible value (alerts effectively disabled at reset).

### Register 0x0A — CONFIGURATION

| Bits | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15 | QWAKE | R/W | 0 | Quick wake-up in one-shot mode (skip full standby) |
| 14 | RESERVED | R/W | 0 | Must read/write 0 |
| 13:10 | RANGE | R/W | 0xC | Full-scale range (0–8 manual, 12=auto-range) |
| 9:6 | CONVERSION_TIME | R/W | 0x8 | Conversion time select (0–11) |
| 5:4 | OPERATING_MODE | R/W | 0x0 | Operating mode (0–3) |
| 3 | LATCH | R/W | 0x1 | Interrupt mode: 0=transparent hysteresis, 1=latched window |
| 2 | INT_POL | R/W | 0x0 | INT polarity: 0=active low, 1=active high |
| 1:0 | FAULT_COUNT | R/W | 0x0 | Consecutive faults before flag: 0=1, 1=2, 2=4, 3=8 |

**Default configuration at reset:** Auto-range, 100 ms conversion, power-down mode, latched interrupts, active-low INT, 1 fault count.

### Register 0x0B — INT_CONFIGURATION

| Bits | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:5 | FIXED_PATTERN | R/W | 0x400 | **Must always read/write 1024 (0x400)** |
| 4 | INT_DIR | R/W | 0x1 | INT pin direction: 0=input, 1=output |
| 3:2 | INT_CFG | R/W | 0x0 | INT output mode (see table below) |
| 1 | RESERVED | R/W | 0x0 | Must read/write 0 |
| 0 | I2C_BURST | R/W | 0x1 | Enable I²C burst read (auto-increment pointer) |

**INT_CFG values:**

| Value | INT Pin Function |
|---|---|
| 0 | SMBUS Alert / threshold interrupt (per LATCH mode) |
| 1 | INT asserted with ~1 µs pulse after every conversion |
| 2 | Invalid — do not use |
| 3 | INT asserted with ~1 µs pulse every 4 conversions (FIFO full) |

### Register 0x0C — FLAGS

| Bits | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:4 | RESERVED | R/W | 0x000 | Must read/write 0 |
| 3 | OVERLOAD_FLAG | R | 0 | Light exceeds full-scale range |
| 2 | CONVERSION_READY_FLAG | R | 0 | Conversion complete; cleared on read of reg 0x0C or write of non-zero |
| 1 | FLAG_H | R | 0 | Measurement above high threshold |
| 0 | FLAG_L | R | 0 | Measurement below low threshold |

### Register 0x11 — DEVICE_ID

| Bits | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:14 | RESERVED | R/W | 0x0 | Must read/write 0 |
| 13:12 | DIDL | R | 0x0 | Device ID low bits |
| 11:0 | DIDH | R | 0x121 | Device ID high bits |

**Device ID verification:** Read register 0x11, mask and check DIDH[11:0] = **0x121**.

---

## 6. Operating Modes

### 6.1 Power-Down Mode (OPERATING_MODE = 0)

- Lowest power: ~2 µA standby
- No active conversions
- Device responds to I²C
- Default state after reset / power-on

### 6.2 Continuous Mode (OPERATING_MODE = 3)

- Conversions repeat continuously at configured CONVERSION_TIME interval
- Output register + FIFO updated after each conversion
- INT pin (SOT-5X3) asserted per INT_CFG setting
- Active circuits kept on continuously — no Tss delay between measurements
- Recommended: set INT_DIR = 1 (output)

### 6.3 One-Shot Forced Auto-Range (OPERATING_MODE = 1)

- Single measurement triggered, then returns to power-down
- Forces full reset of auto-range logic — ignores previous measurements
- Auto-range recovery takes **~500 µs** additional time
- OPERATING_MODE register resets to 0 after conversion completes
- Best for infrequent, unpredictable light measurements

### 6.4 One-Shot Regular (OPERATING_MODE = 2)

- Single measurement triggered, then returns to power-down
- Auto-range uses information from previous measurements
- Faster than forced mode — no range reset penalty
- OPERATING_MODE register resets to 0 after conversion completes
- Best for time-synchronized periodic measurements (application-driven interval)

### One-Shot Trigger Methods

**Register trigger:**
- Write OPERATING_MODE = 1 or 2 to register 0x0A
- Device resets the field to 0 after conversion completes
- INT pin (output mode) indicates conversion completion

**Hardware trigger (SOT-5X3 only):**
- Set INT_DIR = 0 (input mode)
- Pulse INT pin to trigger measurement
- No INT output for completion — host must time the conversion and poll

### One-Shot Timing Budget

Total time per measurement:
```
T_total = Tss + T_autorange + T_conversion
```
Where:
- Tss = 0.5 ms (standby recovery, eliminated if QWAKE=1)
- T_autorange = ~500 µs (forced mode only, MODE=1)
- T_conversion = per CONVERSION_TIME setting

### Quick Wake-Up (QWAKE)

- QWAKE = 1: active circuits stay powered between one-shots
- Eliminates Tss (0.5 ms) standby recovery delay
- Penalty: higher power between measurements (no true standby)
- Only applicable in one-shot modes

---

## 7. Range Selection

### Auto-Range (RANGE = 0xC, default)

- Device automatically selects optimal full-scale range each measurement
- If measurement is low in range → decreases by 1–2 steps
- If measurement is high in range → increases by 1 step
- If measurement **overflows** range → current measurement is aborted, range increases, new measurement taken
- During fast increasing transients, completion time may exceed CONVERSION_TIME

### Manual Range (RANGE = 0–8)

| RANGE | PicoStar™ Full-Scale | SOT-5X3 Full-Scale |
|---|---|---|
| 0 | 328 lux | 459 lux |
| 1 | 655 lux | 918 lux |
| 2 | 1,311 lux | 1,835 lux |
| 3 | 2,621 lux | 3,670 lux |
| 4 | 5,243 lux | 7,340 lux |
| 5 | 10,486 lux | 14,680 lux |
| 6 | 20,972 lux | 29,360 lux |
| 7 | 41,943 lux | 58,720 lux |
| 8 | 83,886 lux | 117,441 lux |
| 12 | Auto-range | Auto-range |

---

## 8. Conversion Time Selection

| CONVERSION_TIME | Typical Time | Effective MANTISSA Bits |
|---|---|---|
| 0 | 600 µs | 9 |
| 1 | 1 ms | 10 |
| 2 | 1.8 ms | 11 |
| 3 | 3.4 ms | 12 |
| 4 | 6.5 ms | 13 |
| 5 | 12.7 ms | 14 |
| 6 | 25 ms | 15 |
| 7 | 50 ms | 16 |
| 8 | 100 ms | 17 |
| 9 | 200 ms | 18 |
| 10 | 400 ms | 19 |
| 11 | 800 ms | 20 |

Conversion time = integration time + ADC conversion time.  
Higher conversion time → more effective bits → better resolution at low light.

---

## 9. Lux Calculation

### Step-by-Step

1. **Read registers 0x00 and 0x01:**
   - `EXPONENT = reg00[15:12]`
   - `RESULT_MSB = reg00[11:0]`
   - `RESULT_LSB = reg01[15:8]`
   - `COUNTER = reg01[7:4]`
   - `CRC = reg01[3:0]`

2. **Calculate MANTISSA (20 bits):**
   ```
   MANTISSA = (RESULT_MSB << 8) | RESULT_LSB
   ```

3. **Linearize to ADC_CODES (28 bits max):**
   ```
   ADC_CODES = MANTISSA << EXPONENT
   ```
   **WARNING:** MANTISSA is up to 20 bits, EXPONENT up to 8 → result needs **uint32_t** (28 bits). Cast to 32-bit before shifting.

4. **Convert to lux:**
   ```
   lux = ADC_CODES × 312.5e-6    // PicoStar™
   lux = ADC_CODES × 437.5e-6    // SOT-5X3
   ```

### Integer-Only Lux Calculation (Recommended)

To avoid floating-point:
```c
// Result in micro-lux for PicoStar
uint64_t micro_lux = (uint64_t)adc_codes * 3125ULL / 10ULL;

// Or in milli-lux:
uint32_t milli_lux = (uint32_t)((uint64_t)adc_codes * 3125ULL / 10000ULL);
```

For SOT-5X3, substitute 4375 for 3125.

---

## 10. Output CRC Verification

### CRC Calculation

The 4-bit CRC in register 0x01 (bits[3:0]) is verified using syndrome bits X[0]–X[3]. Define:
- `E[3:0]` = EXPONENT
- `R[19:0]` = MANTISSA = (RESULT_MSB << 8) + RESULT_LSB
- `C[3:0]` = COUNTER

Syndrome equations (all should equal 0 if no transmission error):

```
X[0] = XOR(E[3:0], R[19:0], C[3:0])       // XOR of all 28 bits (overall parity)
X[1] = XOR(C[1], C[3], R[1], R[3], R[5], R[7], R[9], R[11], R[13], R[15], R[17], R[19], E[1], E[3])
X[2] = XOR(C[3], R[3], R[7], R[11], R[15], R[19], E[3])
X[3] = XOR(R[3], R[11], R[19])
```

**Verification:** Compute X[0]–X[3] from the received EXPONENT, MANTISSA, COUNTER, and CRC. If all four syndrome bits are 0, the data is error-free. If any are non-zero, a transmission error occurred.

### Sample Counter

- 4-bit counter (0–15) in COUNTER field
- Increments on every successful measurement
- Wraps from 15 → 0
- Use to detect stale data or missed samples

---

## 11. Threshold Detection

### Threshold Register Format

Registers 0x08 (low) and 0x09 (high) each have:
- Bits [15:12]: threshold EXPONENT
- Bits [11:0]: threshold RESULT (12-bit)

### Threshold Comparison

The threshold is compared against linearized ADC_CODES:

```
ADC_CODES_TH = THRESHOLD_RESULT << (8 + THRESHOLD_EXPONENT)
ADC_CODES_TL = THRESHOLD_L_RESULT << (8 + THRESHOLD_L_EXPONENT)
```

Fault conditions:
- **Fault Low:** `ADC_CODES < ADC_CODES_TL`
- **Fault High:** `ADC_CODES > ADC_CODES_TH`

### Fault Count

FAULT_COUNT in register 0x0A[1:0] sets how many consecutive faults are needed before flags are set:

| FAULT_COUNT | Consecutive Faults |
|---|---|
| 0 | 1 |
| 1 | 2 |
| 2 | 4 |
| 3 | 8 |

### Setting a Lux Threshold

To set a threshold for a specific lux level:
```
ADC_CODES = lux / LSB_SIZE                        // LSB_SIZE = 312.5e-6 or 437.5e-6
// Find EXPONENT such that RESULT fits in 12 bits:
EXPONENT = max(0, ceil(log2(ADC_CODES)) - 20)     // simplification
RESULT = ADC_CODES >> (8 + EXPONENT)               // 12-bit threshold value
```

---

## 12. Interrupt Modes (SOT-5X3 Only)

### Latched Window Mode (LATCH = 1, default)

- INT becomes active if measurement is **outside** the threshold window (above high OR below low)
- INT, FLAG_H, and FLAG_L remain latched until register 0x0C is read
- FAULT_COUNT applies

### Transparent Hysteresis Mode (LATCH = 0)

- INT reflects current measurement vs thresholds in real-time
- If measurement > high threshold: INT active, FLAG_H = 1, FLAG_L = 0
- If measurement < low threshold: INT inactive, FLAG_H = 0, FLAG_L = 1
- If measurement between thresholds: previous INT state maintained (hysteresis)
- Flags update after each conversion — no latching
- FAULT_COUNT applies

### INT_CFG Modes

| INT_CFG | Behavior |
|---|---|
| 0 | Threshold interrupt (per LATCH setting) |
| 1 | ~1 µs pulse after every conversion |
| 2 | **Invalid — do not use** |
| 3 | ~1 µs pulse every 4 conversions (FIFO full) |

### INT Pin Configuration

- INT_DIR = 1 (output, default): INT pin drives interrupt signals
- INT_DIR = 0 (input): INT pin used as hardware one-shot trigger
- INT_POL = 0 (active low, default) or 1 (active high)
- INT pin is open-drain — requires external pull-up (10 kΩ typical)

---

## 13. FIFO Operation

### Structure

4-deep FIFO: output register (newest) + 3 shadow registers (previous 3):

| Register Pair | Data |
|---|---|
| 0x00/0x01 | Most recent measurement |
| 0x02/0x03 | Previous measurement (n-1) |
| 0x04/0x05 | Previous measurement (n-2) |
| 0x06/0x07 | Previous measurement (n-3) |

### Data Movement

After each new conversion:
- FIFO 2 ← FIFO 1
- FIFO 1 ← FIFO 0  
- FIFO 0 ← previous output
- Output ← new measurement

### Burst Readout

With I2C_BURST = 1 (default):
- Set register pointer to 0x00
- Read 16 bytes (8 registers × 2 bytes) sequentially
- Register pointer auto-increments
- Efficient: single I²C transaction for all 4 measurements

### FIFO Interrupt

Set INT_CFG = 3 to get an interrupt every 4 conversions (FIFO full). This reduces interrupt frequency by 4× while preserving all data.

---

## 14. Device Identification

### Reading Device ID

Read register 0x11:
- DIDH (bits [11:0]) = **0x121** — verify this for device presence
- DIDL (bits [13:12]) = **0x0**

### Probe Sequence

```
1. Read register 0x11
2. Extract DIDH = reg & 0x0FFF
3. Verify DIDH == 0x121
```

---

## 15. Reset

### General-Call Reset

- Send to I²C address **0x00** (general call):
  - Data: **0x06**
- All registers reset to power-on defaults
- Device enters power-down mode
- No device-specific acknowledgment

### Power-On Reset (POR)

- Triggered when VDD crosses **0.8 V** threshold
- All registers reset to defaults
- Device enters power-down mode

---

## 16. Resolution Tables

### PicoStar™ Variant — Effective Resolution (lux)

Full-scale lux per EXPONENT: 328 | 655 | 1310 | 2621 | 5243 | 10486 | 20972 | 41943 | 83886

| CT Reg | Conv Time | Eff. Bits | EXP 0 | EXP 1 | EXP 2 | EXP 3 | EXP 4 | EXP 5 | EXP 6 | EXP 7 | EXP 8 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 600 µs | 9 | 640 m | 1.28 | 2.56 | 5.12 | 10.24 | 20.48 | 40.96 | 81.92 | 163.84 |
| 1 | 1 ms | 10 | 320 m | 640 m | 1.28 | 2.56 | 5.12 | 10.24 | 20.48 | 40.96 | 81.92 |
| 2 | 1.8 ms | 11 | 160 m | 320 m | 640 m | 1.28 | 2.56 | 5.12 | 10.24 | 20.48 | 40.96 |
| 3 | 3.4 ms | 12 | 80 m | 160 m | 320 m | 640 m | 1.28 | 2.56 | 5.12 | 10.24 | 20.48 |
| 4 | 6.5 ms | 13 | 40 m | 80 m | 160 m | 320 m | 640 m | 1.28 | 2.56 | 5.12 | 10.24 |
| 5 | 12.7 ms | 14 | 20 m | 40 m | 80 m | 160 m | 320 m | 640 m | 1.28 | 2.56 | 5.12 |
| 6 | 25 ms | 15 | 10 m | 20 m | 40 m | 80 m | 160 m | 320 m | 640 m | 1.28 | 2.56 |
| 7 | 50 ms | 16 | 5 m | 10 m | 20 m | 40 m | 80 m | 160 m | 320 m | 640 m | 1.28 |
| 8 | 100 ms | 17 | 2.5 m | 5 m | 10 m | 20 m | 40 m | 80 m | 160 m | 320 m | 640 m |
| 9 | 200 ms | 18 | 1.25 m | 2.5 m | 5 m | 10 m | 20 m | 40 m | 80 m | 160 m | 320 m |
| 10 | 400 ms | 19 | 0.625 m | 1.25 m | 2.5 m | 5 m | 10 m | 20 m | 40 m | 80 m | 160 m |
| 11 | 800 ms | 20 | 312.5 µ | 0.625 m | 1.25 m | 2.5 m | 5 m | 10 m | 20 m | 40 m | 80 m |

All resolution values are in **lux**. Suffix: µ = micro-lux, m = milli-lux, plain number = lux.

### SOT-5X3 Variant — Effective Resolution (lux)

Full-scale lux per EXPONENT: 459 | 918 | 1835 | 3670 | 7340 | 14680 | 29360 | 58720 | 117441

| CT Reg | Conv Time | Eff. Bits | EXP 0 | EXP 1 | EXP 2 | EXP 3 | EXP 4 | EXP 5 | EXP 6 | EXP 7 | EXP 8 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 600 µs | 9 | 896 m | 1.792 | 3.584 | 7.168 | 14.336 | 28.672 | 47.344 | 114.688 | 229.376 |
| 1 | 1 ms | 10 | 448 m | 896 m | 1.792 | 3.584 | 7.168 | 14.336 | 28.672 | 47.344 | 114.688 |
| 2 | 1.8 ms | 11 | 224 m | 448 m | 896 m | 1.792 | 3.584 | 7.168 | 14.336 | 28.672 | 47.344 |
| 3 | 3.4 ms | 12 | 112 m | 224 m | 448 m | 896 m | 1.792 | 3.584 | 7.168 | 14.336 | 28.672 |
| 4 | 6.5 ms | 13 | 56 m | 112 m | 224 m | 448 m | 896 m | 1.792 | 3.584 | 7.168 | 14.336 |
| 5 | 12.7 ms | 14 | 28 m | 56 m | 112 m | 224 m | 448 m | 896 m | 1.792 | 3.584 | 7.168 |
| 6 | 25 ms | 15 | 14 m | 28 m | 56 m | 112 m | 224 m | 448 m | 896 m | 1.792 | 3.584 |
| 7 | 50 ms | 16 | 7 m | 14 m | 28 m | 56 m | 112 m | 224 m | 448 m | 896 m | 1.792 |
| 8 | 100 ms | 17 | 3.5 m | 7 m | 14 m | 28 m | 56 m | 112 m | 224 m | 448 m | 896 m |
| 9 | 200 ms | 18 | 1.75 m | 3.5 m | 7 m | 14 m | 28 m | 56 m | 112 m | 224 m | 448 m |
| 10 | 400 ms | 19 | 0.875 m | 1.75 m | 3.5 m | 7 m | 14 m | 28 m | 56 m | 112 m | 224 m |
| 11 | 800 ms | 20 | 437.5 µ | 0.875 m | 1.75 m | 3.5 m | 7 m | 14 m | 28 m | 56 m | 112 m |

All resolution values are in **lux**. Suffix: µ = micro-lux, m = milli-lux, plain number = lux. LSBs below effective resolution are zero-padded.

---

## 17. Driver State Machine Guidance

### Power-Up Sequence

1. Wait for VDD to stabilize above 0.8 V (POR)
2. Read Device ID register (0x11) — verify DIDH = 0x121
3. Configure register 0x0B (preserve fixed pattern: bits[15:5] = 0x400)
4. Configure register 0x0A (range, conversion time, operating mode)
5. Optionally set thresholds (0x08, 0x09)
6. Write OPERATING_MODE to start measurements

### Continuous Mode Lifecycle

```
POWER_DOWN → write OPERATING_MODE=3 → CONTINUOUS
  ↓ (per conversion time)
  poll CONVERSION_READY_FLAG (reg 0x0C bit 2)
  OR wait for INT pin
  ↓ (ready)
  read registers 0x00-0x01 → compute lux
  ↓ (stop)
  write OPERATING_MODE=0 → POWER_DOWN
```

### One-Shot Mode Lifecycle

```
POWER_DOWN → write OPERATING_MODE=1 or 2 → CONVERTING
  ↓ (wait Tss + conversion time)
  poll CONVERSION_READY_FLAG or INT
  ↓ (ready)
  read registers 0x00-0x01 → compute lux
  → device auto-returns to POWER_DOWN (MODE resets to 0)
```

### Register Read/Write Safety

- Register 0x0B: always preserve bits[15:5] = 0x400 (1024). Read-modify-write recommended.
- Register 0x0C: writing non-zero to 0x0C clears CONVERSION_READY_FLAG. Reading 0x0C clears latched flags.
- Register 0x0A bit 14: must always be 0.

### CONVERSION_READY_FLAG Behavior

- Set to 1 when conversion completes
- Cleared when register 0x0C is read or written with non-zero value
- Can be used as polled alternative to INT pin
- In continuous mode: flag is set after each conversion

---

## 18. Implementation Notes

### 32-bit Data Types Required

The lux calculation involves:
- MANTISSA: up to 20 bits
- ADC_CODES: MANTISSA << EXPONENT → up to 28 bits
- **Must use uint32_t** (or larger) for ADC_CODES  
- Explicit cast before left-shift to prevent overflow

### Register 0x0B Fixed Pattern

Bits [15:5] of register 0x0B must always equal **1024 (0x400)**. When writing to this register:
```c
uint16_t reg0b = 0x8000 | (int_dir << 4) | (int_cfg << 2) | (0 << 1) | i2c_burst;
// 0x8000 = fixed bit 15 set, rest of fixed pattern = 0x400 << 5... 
// Actually the reset value is 0x8011, and the fixed pattern spans bits 15:5 = 0x400
```
Read-modify-write: read register, modify only bits 4:0, write back.

### Overload Detection

OVERLOAD_FLAG (reg 0x0C bit 3) indicates the light exceeds the current full-scale range. In auto-range mode, this should be transient. In manual range mode, consider switching to a higher range.

### Power-Supply Bypass

- 100 nF bypass capacitor close to VDD pin
- Solid ground plane under the device
- No hot components near the optical sensing area

### Dark Window Compensation

When placing OPT4001 behind a dark window/cover glass:
- Window attenuates visible light by a known factor
- Multiply measured lux by the reciprocal of the window transmission factor
- OPT4001's IR rejection makes it well-suited for dark window operation
- Recommend field-of-view ≥ ±35° (preferably ±45°)

### PicoStar™ Assembly Notes

- Sensor area faces **downward** (bottom of package)
- Must mount on **flex PCB (FPCB)** with cutout for light collection
- Cutout shapes: rectangular, plus-shaped, or circular
- FPCB + device height ≈ 0.426 mm
- Solder paste: type 4 or higher, no-clean, lead-free
- Pick-and-place nozzle: > 0.6 mm
- Reflow ramp rate: 1–1.2 °C/s (slow, to prevent solder splattering onto sensor)
