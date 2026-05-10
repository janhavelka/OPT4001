# OPT4001 compact documentation inventory

This directory summarizes OPT4001 datasheet facts needed by the driver: PicoStar and SOT-5X3 pin differences, I2C addresses, register-address transactions, burst reads, general-call reset, 16-bit register defaults, lux conversion, FIFO fields, threshold flags, and INT behavior. The raw extraction archive remains in `docs/pdf-extracted-md/`.

| File | Purpose |
| --- | --- |
| `00_document_inventory.md` | Map of compact notes and source material. |
| `01_chip_overview.md` | Device role, measurement model, variants, and driver scope. |
| `02_pinout_and_signals.md` | PicoStar and SOT-5X3 pins, I2C, ADDR, and INT behavior. |
| `03_electrical_and_timing.md` | Supply limits, lux range, conversion timing, and I2C speeds. |
| `04_protocol_commands_and_transactions.md` | I2C register addressing, reads/writes, burst reads, general-call reset, and alert response. |
| `05_register_map.md` | Register summary and key configuration/status fields. |
| `06_modes_interrupts_status_and_faults.md` | Power-down, continuous, one-shot modes, thresholds, FIFO, INT, and flags. |
| `07_initialization_reset_and_operational_notes.md` | Startup sequence, result conversion, reset, and board notes. |
| `08_variant_differences_and_open_questions.md` | PicoStar versus SOT-5X3 differences plus facts not documented or ambiguous in the checked-in PDFs. |

## Source documents

| Source PDF | Raw extract | Pages used | Notes |
| --- | --- | --- | --- |
| `docs/OPT4001_datasheet.pdf` | `docs/pdf-extracted-md/OPT4001_datasheet.md` | 1, 4-5, 10-32, 33-41 | Primary source for compact driver notes. |
| `docs/AN_high_speed_resolution.pdf` | `docs/pdf-extracted-md/AN_high_speed_resolution.md` | Not used for register facts | Optical/application context only; no OPT4001 register defaults or I2C command facts were taken from it. |
| `docs/AN_light_detection.pdf` | `docs/pdf-extracted-md/AN_light_detection.md` | Not used for register facts | Optical/application context only; no OPT4001 register defaults or I2C command facts were taken from it. |
| `docs/AN_picostar_package.pdf` | `docs/pdf-extracted-md/AN_picostar_package.md` | Not used for register facts | Package/mechanical context only; no I2C transaction facts were taken from it. |
