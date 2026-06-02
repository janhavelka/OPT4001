# OPT4001 Release Checklist

Date: 2026-06-02

Branch: `hardening/opt4001-industry-readiness`

Prepared version: `1.0.0`

Recommended release wording:

```text
OPT4001 v1.0.0 is a production-oriented, industry-readiness-hardened, framework-neutral OPT4001 driver with native fake-transport tests, Arduino ESP32-S2/S3 build coverage, documented ESP-IDF component/example support, explicit lifecycle/freshness/FIFO/CRC/dirty-state contracts, and hardware-validation procedures.
```

## Version Status

| Item | Status |
| --- | --- |
| `library.json` | `1.0.0` |
| `idf_component.yml` | `1.0.0` |
| `include/OPT4001/Version.h` | `1.0.0`, current by `scripts/generate_version.py check` |
| Existing local tags | None found during release-prep check |
| Existing remote tags | None found during release-prep check |
| Existing GitHub releases | None found during release-prep check |
| Version recommendation | Prepare first public `v1.0.0`; do not tag until merge/CI policy is satisfied. |

## Local Checks

| Command | Result |
| --- | --- |
| `git status --short` | Clean before release-prep edits. |
| `git branch --show-current` | `hardening/opt4001-industry-readiness`. |
| `git log --oneline -10` | Headed by `3bb77a4 test: capture OPT4001 hardware validation evidence`. |
| `python tools/check_core_timing_guard.py` | Passed. |
| `python tools/check_cli_contract.py` | Passed. |
| `python tools/check_idf_example_contract.py` | Passed. |
| `python tools/check_version_header_contract.py` | Passed; `Version.h` up to date. |
| `python tools/check_readiness_claims.py` | Passed. |
| `python tools/check_public_api_docs.py` | Passed. |
| `python scripts/generate_version.py check` | Passed; `Version.h` up to date. |
| `python -m platformio test -e native` | Passed; 96/96 tests in `00:00:00.886`. |
| `python -m platformio run -e esp32s3dev` | Passed; `esp32s3dev SUCCESS` in `00:00:04.803`. |
| `python -m platformio run -e esp32s2dev` | Passed; `esp32s2dev SUCCESS` in `00:00:04.385`. |
| `python -m platformio pkg pack` | Passed; wrote `OPT4001-1.0.0.tar.gz`; artifact removed. |

PlatformIO printed the known obsolete-core warning for local Core `v6.1.18`
while a previous Core `v6.1.19` is also present.

## ESP-IDF Checks

| Command | Result |
| --- | --- |
| `idf.py -C examples/esp_idf/basic set-target esp32s3 build` | Not run successfully; `idf.py` is not recognized on PATH. |
| `idf.py -C examples/esp_idf/basic set-target esp32s2 build` | Not run successfully; `idf.py` is not recognized on PATH. |

CI contains a pure ESP-IDF matrix build using `espressif/esp-idf-ci-action@v1`,
but a completed branch/PR CI run was not captured during release preparation.

## CI Checklist

Before merging or tagging, verify a completed CI run for the final branch/PR:

- PlatformIO ESP32-S3 build passes.
- PlatformIO ESP32-S2 build passes.
- Native tests pass.
- Guard scripts pass.
- Package pack passes.
- Pure ESP-IDF `esp32s3` build passes.
- Pure ESP-IDF `esp32s2` build passes.

## Documentation Checklist

| Item | Status |
| --- | --- |
| README install instructions | Present for PlatformIO, manual install, and ESP-IDF component use. |
| Version-header handling | Documented; `Version.h` is tracked and checked. |
| Readiness wording | Conservative; current evidence and pending validation are separated. |
| Public API docs guard | Passed locally. |
| Readiness claims guard | Passed locally. |
| Changelog | Finalized with Added, Changed, Fixed, Validation, and Known Limitations. |

## Hardware Validation Status

Hardware validation remains deferred.

No completed real-device smoke, optical reference, address-pin matrix, INT
capture, FIFO physical timing/order, or fault/recovery logs are included. The
pending log is recorded in
`docs/OPT4001_HARDWARE_VALIDATION_LOG_20260602.md`.

## Release Wording Check

Use the recommended release wording above.

Do not claim real-device, optical, INT, FIFO timing/order, fault-path, address
matrix, or pure ESP-IDF build evidence until matching logs are captured.

## Merge Recommendation

The source-level hardening branch is locally ready for merge preparation, but
merge should wait for a completed branch/PR CI run with the checklist above.

## Tag Suggestions

Do not tag from this branch during this prompt.

After merge and completed CI on the release commit:

```bash
git checkout main
git pull --ff-only
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin main v1.0.0
```
