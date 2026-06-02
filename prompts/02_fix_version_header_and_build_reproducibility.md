# Prompt 2 — Fix H1: Version.h and Clean Checkout / Manual / ESP-IDF Build Reproducibility

Continue on `hardening/opt4001-industry-readiness`. Complete only this prompt, commit, sync, and stop.

## Finding from exploration report

H1: Fresh checkout/manual/ESP-IDF builds are not reproducible.

Evidence from the report:

- `include/OPT4001/OPT4001.h` includes `OPT4001/Version.h`.
- `.gitignore` ignores `include/OPT4001/Version.h`.
- `git ls-files include/OPT4001` did not include `Version.h`.
- `platformio.ini` runs `scripts/generate_version.py`.
- Root `CMakeLists.txt` had no equivalent generation step.
- README claims manual and ESP-IDF install paths.

## Goal

Make public-header inclusion and all documented build paths reproducible from a clean checkout.

## Required first commands

```bash
git status --short
git branch --show-current
```

Stop if dirty.

## Subagents

Spawn:

1. **version-header subagent**
   - Inspect `include/OPT4001/OPT4001.h`, `.gitignore`, `scripts/generate_version.py`, `library.json`.
   - Decide whether to track `Version.h` or generate it in every build path.

2. **cmake-idf subagent**
   - Inspect root `CMakeLists.txt`, `idf_component.yml`, and `examples/esp_idf/basic`.
   - Design the clean ESP-IDF solution.

3. **ci-clean-checkout subagent**
   - Inspect `.github/workflows/ci.yml`.
   - Add a CI job or step that would catch the H1 bug.

## Required implementation

Choose one robust approach:

### Preferred A — track deterministic `Version.h`

Track `include/OPT4001/Version.h` if it is deterministic from `library.json`.

Requirements:

- Remove the `.gitignore` rule that prevents tracking it.
- `scripts/generate_version.py check` must verify it is current.
- CI must run that check.
- Manual and ESP-IDF users can include `OPT4001/OPT4001.h` without a pre-generation step.

### Acceptable B — generate in CMake and generic path

If not tracking the header:

- Root CMake must generate it before compiling the component.
- ESP-IDF example must generate it before build.
- Manual/native users must have a documented one-command generation path.
- CI must prove the generation path.

### Not acceptable

- PlatformIO-only generation.
- Public header includes a file absent from a clean checkout.
- Documentation claims ESP-IDF readiness while `idf.py` cannot build from clean checkout.

## Add/adjust guard script

Add or update a guard, for example:

```text
tools/check_version_header_contract.py
```

It should verify:

- `include/OPT4001/OPT4001.h` can resolve `OPT4001/Version.h`.
- `Version.h` is tracked or generated in documented non-PlatformIO paths.
- `library.json` version matches `Version.h`.
- Running `scripts/generate_version.py check` passes.

Add this guard to CI if feasible.

## Documentation updates

Update README and any ESP-IDF doc:

- Describe how `Version.h` is handled.
- Remove any false manual/ESP-IDF build claim.
- Add exact verification commands.
- State whether local `idf.py` validation was run or only CI is configured.

## Required validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_version_header_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
```

If `tools/check_version_header_contract.py` is not added, explain what equivalent check was used.

Attempt:

```bash
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Record exact results.

Remove package artifacts after validation.

## Report

Create:

```text
docs/OPT4001_H1_VERSION_BUILD_REPRODUCIBILITY_REPORT.md
```

Include:

- root cause,
- chosen design,
- clean-checkout impact,
- files changed,
- CI changes,
- exact command results,
- remaining limitations.

Update the finding-to-prompt plan marking H1 complete/pending with evidence.

## Commit and sync

```bash
git diff --check
git status --short
git add .
git commit -m "build: fix OPT4001 version header reproducibility"
git push
```

## Final response

Report exact implementation, tests, commit hash, push result, and that Prompt 3 will address H2/lifecycle and object contracts.
