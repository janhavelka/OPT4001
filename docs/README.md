# OPT4001 Documentation Index

This directory contains maintained integration notes, validation procedures,
reference extracts, and historical hardening reports for the OPT4001 library.

## Release-Facing Documents

Use these documents for normal integration and release review:

- [README.md](../README.md) - library overview, install paths, API model, and
  validation status.
- [CHANGELOG.md](../CHANGELOG.md) - release notes and known limitations.
- [OPT4001_RELEASE_CHECKLIST.md](OPT4001_RELEASE_CHECKLIST.md) - merge,
  package, CI, validation, wording, and tag checklist.
- [IDF_PORT.md](IDF_PORT.md) - ESP-IDF integration boundary and validation
  commands.
- [IDF_PORT_IMPLEMENTATION.md](IDF_PORT_IMPLEMENTATION.md) - native ESP-IDF
  example implementation notes.
- [OPT4001_HARDWARE_VALIDATION_PROCEDURE.md](OPT4001_HARDWARE_VALIDATION_PROCEDURE.md)
  - repeatable hardware/HIL validation procedure.

## Reference Material

The compact Markdown files summarize device behavior used by the driver:

- [OPT4001_datasheet.md](OPT4001_datasheet.md)
- [AN_light_detection.md](AN_light_detection.md)
- [AN_high_speed_resolution.md](AN_high_speed_resolution.md)
- [AN_picostar_package.md](AN_picostar_package.md)

The corresponding PDFs are retained as source reference material. The
`extracted-md/` and `pdf-extracted-md/` directories contain extraction outputs
used while reconciling datasheet and application-note details. Treat extracted
files as provenance material, not as API documentation.

## Validation Evidence

- `OPT4001_HARDWARE_VALIDATION_LOG_20260602.md` records that hardware
  validation was not run because required board,
  operator, wiring, serial, optical, INT capture, and fault-test metadata were
  not provided.
- Hardware, optical, INT, FIFO timing/order, address-pin, fault/recovery, and
  local pure ESP-IDF evidence remain open until captured in a filled validation
  log with raw transcripts or traceable artifacts.

## Historical Hardening Reports

Files named `OPT4001_H*_..._REPORT.md`,
`OPT4001_FIFO_INT_PARTIAL_STATE_REPORT.md`,
`OPT4001_NUMERIC_VECTOR_CRC_REPORT.md`,
`OPT4001_I2C_LIBRARY_AUDIT_REPORT.md`,
`OPT4001_POLL_CHUNKING_REPORT.md`,
`OPT4001_HARDENING_FINAL_REPORT.md`,
`OPT4001_HARDENING_FINDING_TO_PROMPT_PLAN.md`, and
`OPT4001_INDUSTRY_READINESS_EXPLORATION_REPORT.md` are historical audit and
closure records. They are useful for traceability, but they are intentionally
not included as primary generated Doxygen pages.

## Generated Documentation

`Doxyfile` builds generated API documentation into `docs/doxygen/`. That output
is generated content and should not be committed.
