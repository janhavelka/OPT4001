# OPT4001 H1 Version and Build Reproducibility Report

Date: 2026-06-02
Branch: `hardening/opt4001-industry-readiness`
Finding: H1 - Fresh checkout/manual/ESP-IDF builds are not reproducible

## Root Cause

`include/OPT4001/OPT4001.h` unconditionally includes
`OPT4001/Version.h`, but `include/OPT4001/Version.h` was ignored by
`.gitignore` and was not tracked. PlatformIO generated the file through
`scripts/generate_version.py`, so PlatformIO builds could hide the problem, but
manual and ESP-IDF/CMake consumers could include a public header that referenced
a file absent from a clean checkout.

## Chosen Design

Preferred A was implemented: track deterministic `include/OPT4001/Version.h`.

Design details:

- `Version.h` remains generated from `library.json`.
- `Version.h` is committed so public headers resolve from a clean checkout.
- `.gitignore` no longer ignores `include/OPT4001/Version.h`.
- `scripts/generate_version.py check` remains the drift check.
- `tools/check_version_header_contract.py` verifies the public include,
  tracked-header contract, and `library.json` version consistency.
- Generated fallback build metadata now uses stable `"unknown"` defaults for
  non-PlatformIO builds. PlatformIO still injects date, time, git commit, and
  git status through compiler defines.

This avoids requiring manual, Arduino library-manager, or ESP-IDF users to run
Python before including `OPT4001/OPT4001.h`.

## Clean-Checkout Impact

Clean checkout behavior after this fix:

- `include/OPT4001/Version.h` is present immediately after clone/checkout.
- Manual users can copy `include/OPT4001/` and `src/` without a generation step.
- Root ESP-IDF/CMake consumers can resolve `OPT4001/OPT4001.h` without a
  PlatformIO pre-script.
- CI has a guard that fails if the version header is missing from the git index,
  ignored again, stale against `library.json`, or inconsistent internally.

## Files Changed

| File | Change |
| --- | --- |
| `.gitignore` | Removed the ignore rule for `include/OPT4001/Version.h` and documented why it is tracked. |
| `include/OPT4001/Version.h` | Added tracked generated header synchronized with `library.json` version `1.0.0`. |
| `scripts/generate_version.py` | Changed non-PlatformIO fallback build metadata from `__DATE__`/`__TIME__` to stable `"unknown"` strings. |
| `tools/check_version_header_contract.py` | Added guard for public include resolution, tracked header, version consistency, and generator check. |
| `.github/workflows/ci.yml` | Added the version-header guard to CI before PlatformIO can mask missing generated files; also added IDF example contract coverage to validation. |
| `README.md` | Documented tracked `Version.h`, verification commands, and local ESP-IDF validation limitation. |
| `docs/IDF_PORT.md` | Documented ESP-IDF version-header handling and real `idf.py` build commands. |
| `docs/IDF_PORT_IMPLEMENTATION.md` | Documented the committed version-header contract and guard. |
| `docs/OPT4001_HARDENING_FINDING_TO_PROMPT_PLAN.md` | Marked H1 handled by Prompt 2 with evidence and remaining limitation. |

## CI Changes

`.github/workflows/ci.yml` now runs:

- `python tools/check_version_header_contract.py` in `validate-library` before
  PlatformIO is installed or run.
- `python tools/check_version_header_contract.py` in `native-tests`.
- `python tools/check_idf_example_contract.py` in `validate-library`.

The early `validate-library` placement matters because PlatformIO can run the
version generator and hide a missing clean-checkout header.

## Exact Command Results

| Command | Result |
| --- | --- |
| `git status --short` | Clean before Prompt 2 edits. |
| `git branch --show-current` | `hardening/opt4001-industry-readiness`. |
| `python tools\check_core_timing_guard.py` | `Core framework guard PASSED`. |
| `python tools\check_cli_contract.py` | `CLI contract PASSED`. |
| `python tools\check_idf_example_contract.py` | `IDF example contract PASSED`. |
| `python tools\check_version_header_contract.py` | `Version header contract PASSED`; generator reported `Version.h` up to date. |
| `python scripts\generate_version.py check` | `Up to date: C:\Users\Honza\Documents\Projects\OPT4001\include\OPT4001\Version.h`. |
| `python -m platformio test -e native` | Passed; `50 test cases: 50 succeeded in 00:00:01.895`. |
| `python -m platformio run -e esp32s3dev` | Passed; `esp32s3dev SUCCESS`; RAM `6.9%`, Flash `33.1%`. |
| `python -m platformio run -e esp32s2dev` | Passed; `esp32s2dev SUCCESS`; RAM `11.3%`, Flash `32.5%`. |
| `python -m platformio pkg pack` | Passed; wrote `OPT4001-1.0.0.tar.gz`; artifact removed after validation. |
| `idf.py --version` | Failed: `idf.py` is not recognized as a command in this shell. |
| `idf.py -C examples/esp_idf/basic set-target esp32s3 build` | Failed: `idf.py` is not recognized as a command in this shell. |
| `idf.py -C examples/esp_idf/basic set-target esp32s2 build` | Failed: `idf.py` is not recognized as a command in this shell. |

PlatformIO printed the existing obsolete-core warning for local Core `6.1.18`;
the warning did not fail the builds.

## Remaining Limitations

- Pure ESP-IDF builds were not executed locally because `idf.py` is not
  installed or not on `PATH` in this shell.
- Prompt 2 fixes the clean-checkout source/header reproducibility issue, but
  real ESP-IDF build validation still needs an environment with ESP-IDF
  installed.
- Hardware validation remains outside Prompt 2 and is still planned for Prompt 9.
- Production-readiness, lifecycle, probe, sample freshness, numeric, FIFO, INT,
  and documentation-honesty findings remain for later prompts.

## H1 Status

H1 is fixed for source-level clean-checkout, manual include, and ESP-IDF/CMake
public-header reproducibility. The remaining evidence gap is execution of real
`idf.py` builds in an ESP-IDF-enabled environment.
