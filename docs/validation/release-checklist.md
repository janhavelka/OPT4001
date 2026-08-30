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

On Windows, all PlatformIO commands below must use `scripts\pio.cmd`; it selects
the existing VS Code-managed PlatformIO installation.

```powershell
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_ci_action_pins.py
python tools/hil_opt4001_runner.py --parser-self-test
python tools/test_hil_opt4001_runner_parser.py
python tools/check_idf_example_contract.py
python tools/check_version_header_contract.py
python tools/check_clean_consumer_package.py
python scripts/generate_version.py check
.\scripts\pio.cmd test -e native
.\scripts\pio.cmd run -e native_core_no_arduino
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s2dev
.\scripts\pio.cmd pkg pack
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
- Framework-neutral native core consumer compiles and links without Arduino or
  Wire include paths.
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
- `docs/validation/hardware-validation-procedure.md` matches the CLI examples.
- `CHANGELOG.md` has an entry for every user-visible change.

## Hardware

If the release changes measurement, freshness, INT, FIFO, or recovery behaviour,
run the board procedure in `docs/validation/hardware-validation-procedure.md` and
attach the transcript to the release.

## Tag

After merge and completed CI on the release commit:

```bash
git checkout main
git pull --ff-only
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin main vX.Y.Z
```
