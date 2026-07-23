# OPT4001 Documentation

This directory is intentionally small. It mirrors common Arduino/I2C sensor
library practice: the root README is the user-facing entry point, examples show
usage, generated API docs come from headers, and `docs/` keeps only integration,
reference, and validation material.

## Integration

- [ESP-IDF integration](integration/esp-idf.md) - native ESP-IDF component and
  example boundary.
- [Driver contracts](integration/driver-contracts.md) - lifecycle, health,
  freshness, poll-job, dirty-state, numeric, and CRC contracts.

## Reference

- [OPT4001 datasheet summary](reference/OPT4001_datasheet.md)
- [Light detection application note](reference/AN_light_detection.md)
- [High-speed and resolution application note](reference/AN_high_speed_resolution.md)
- [PicoStar package application note](reference/AN_picostar_package.md)

The matching PDFs in `reference/` are retained as vendor source material.

## Validation

- [Validation status](validation/validation-status.md) - current evidence and
  pending hardware/ESP-IDF evidence.
- [Hardware validation procedure](validation/hardware-validation-procedure.md)
  - repeatable hardware/HIL procedure and command sequences.
- [Release checklist](validation/release-checklist.md) - local checks, CI,
  packaging, wording, and tag checklist.

## Generated API Docs

`Doxyfile` builds generated API documentation into `docs/doxygen/`. That output
is generated content and should not be committed.
