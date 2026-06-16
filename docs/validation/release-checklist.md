# Release Checklist

Use this checklist before tagging a release. `library.json` is the version source
of truth; `include/OPT4001/Version.h` is generated from it and committed.

## Version

- Update `library.json`.
- Run `python scripts/generate_version.py sync`.
- Run `python scripts/generate_version.py check`.
- Update `CHANGELOG.md`.
- Update README/docs/examples for any API or behavior change.
- Confirm `idf_component.yml` version is consistent.

## Local Checks

Run and record:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_version_header_contract.py
python tools/check_clean_consumer_package.py
python tools/check_readiness_claims.py
python tools/check_public_api_docs.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
```

Remove any generated `OPT4001-*.tar.gz` artifact after package validation.

Run pure ESP-IDF builds when ESP-IDF is installed:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

## CI

Before merging or tagging, verify a completed CI run for the final branch or PR:

- Native tests pass.
- Guard scripts pass.
- PlatformIO ESP32-S3 build passes.
- PlatformIO ESP32-S2 build passes.
- Clean package consumer check passes.
- PlatformIO package pack passes.
- Pure ESP-IDF `esp32s3` build passes.
- Pure ESP-IDF `esp32s2` build passes.

## Documentation And Claims

- README install instructions are current for PlatformIO, manual use, and
  ESP-IDF component use.
- Readiness wording separates captured evidence from pending hardware evidence.
- `docs/validation/validation-status.md` reflects current evidence.
- `docs/validation/hardware-validation-procedure.md` matches the CLI examples.
- Public API docs and readiness guards pass.
- Do not claim real-device, optical, INT, FIFO timing/order, fault-path,
  address-matrix, or pure ESP-IDF build evidence without captured logs.

## Hardware Evidence

Hardware validation remains pending until a completed log is added with board,
package, wiring, firmware, serial, optical, INT, FIFO, address, and
fault/recovery evidence as applicable.

## Tag

After merge and completed CI on the release commit:

```bash
git checkout main
git pull --ff-only
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin main vX.Y.Z
```
