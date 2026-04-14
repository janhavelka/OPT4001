# Application Note: Advantages of Implementing a Light Sensor in TI's PicoStar Package

> **Source:** Texas Instruments SBOA598 (June 2024)  
> **Relevance:** PicoStar-specific design details for OPT4001 driver and hardware integration.

---

## Key Takeaways

- PicoStar™ is the **thinnest light sensor package in the industry** at 0.226 mm height.
- **Bottom-facing** light-sensitive area — unique assembly requiring flex PCB (FPCB) with cutout.
- OPT4001 PicoStar has **better resolution** (312.5 µlux) than SOT-5X3 (437.5 µlux) and older QFN packages (400 µlux).
- No ADDR or INT pins on PicoStar — I²C address is **fixed at 0x45**, no hardware interrupt available.

## Package Comparison (OPT4001 Variants)

| Feature | DNP (QFN) | DTS (SOT-5X3) | YMN (PicoStar™) |
|---|---|---|---|
| Size | 2×2×0.65 mm | 2.1×1.9×0.6 mm | 0.84×1.05×0.226 mm |
| Pin count | 6 | 8 | 4 |
| Sensor facing | Top | Top | Bottom |
| I²C address | — | Configurable | Fixed (0x45) |
| INT pin | — | Yes | No |
| Lux resolution (800 ms) | 400 µlux | 437.5 µlux | 312.5 µlux |
| Resolution (100 ms) | 2.5 mlux | 3.5 mlux | 2.5 mlux |
| Resolution (1.8 ms) | 160 mlux | 224 mlux | 160 mlux |
| Full-scale saturation | 107 klux | 117 klux | 83 klux |
| Temperature range | –40 to 105 °C | –40 to 105 °C | –40 to 125 °C |

## Relevance to Implementation

- **Driver must handle absence of INT pin** for PicoStar variant — use CONVERSION_READY_FLAG polling instead of hardware interrupt.
- **Fixed I²C address (0x45)** means only one PicoStar device per I²C bus. SOT-5X3 allows up to 3 unique addresses (0x44, 0x45, 0x46).
- **Lux LSB constant differs** between variants — driver must use the correct constant based on configured package type.
- PicoStar's extended temperature range (–40 to **125 °C**) vs SOT-5X3 (–40 to 85 °C) may matter for automotive underhood applications.
- The **FPCB cutout design** affects field of view and light collection — this is a hardware design concern, not a driver concern, but the driver should document recommended angular response considerations.
