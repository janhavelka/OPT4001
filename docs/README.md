# OPT4001 Documentation

This directory is intentionally small. The root README is the user-facing entry
point, examples show usage, generated API docs come from the headers, and `docs/`
keeps only integration, reference, and validation material.

## Integration

- [Driver contracts](integration/driver-contracts.md) — lifecycle, health,
  freshness, poll-job, dirty-state, numeric, and CRC contracts.
- [ESP-IDF integration](integration/esp-idf.md) — native ESP-IDF component and
  example boundary.

## Reference

- [OPT4001 datasheet summary](reference/OPT4001_datasheet.md) — register map,
  timing, formulas, and behaviour, condensed from SBOS993A.
- [Light detection application note](reference/AN_light_detection.md)
- [High-speed and resolution application note](reference/AN_high_speed_resolution.md)
- [PicoStar package application note](reference/AN_picostar_package.md)

The matching PDFs in `reference/` are the authoritative vendor sources. When a
summary and a PDF disagree, the PDF wins — extract its text with PyMuPDF
(`python -c "import fitz; print(fitz.open('docs/reference/OPT4001_datasheet.pdf')[25].get_text())"`)
rather than trusting the summary. The PDFs are excluded from the packaged
library artifact via `library.json`.

## Validation

- [Hardware validation procedure](validation/hardware-validation-procedure.md) —
  repeatable board / HIL bring-up procedure and command sequences.
- [Release checklist](validation/release-checklist.md) — local checks, CI,
  packaging, and tag checklist.

## Generated API Docs

`Doxyfile` builds generated API documentation into `docs/doxygen/`. That output
is generated content and is not committed.
