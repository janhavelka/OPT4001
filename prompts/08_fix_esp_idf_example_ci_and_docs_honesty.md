# Prompt 8 — Fix H5: Docs Honesty, ESP-IDF Example/CI, Metadata, and Public Contracts

Continue on `hardening/opt4001-industry-readiness`. Complete only this prompt, commit, sync, and stop.

## Findings from exploration report

H5: Production and validation claims exceed available evidence.

Evidence:

- “Production-grade” appears in README/library metadata/component metadata.
- README references validation/build commands but no captured hardware matrix exists.
- IDF docs list implementation status, not measured validation status.
- `.github/workflows/ci.yml` does not build pure IDF or run IDF example contract.
- ESP-IDF CLI loop can block `tick()` because it uses blocking input.
- IDF example include boundary is broad.
- SECURITY.md supported version says `0.3.x` while package is `1.0.0`.
- Public headers lack operational contracts.

## Goal

Make all docs, examples, metadata, and CI honest and aligned with the actual validation evidence. Improve the ESP-IDF example so it is clearly diagnostic and does not block periodic driver `tick()` in a misleading way.

## Required first commands

```bash
git status --short
git branch --show-current
```

Stop if dirty.

## Subagents

Spawn:

1. **claims subagent**
   - Search for `production-grade`, `industry-grade`, `validated`, `ESP-IDF ready`, `hardware validated`, `optical validated`.
   - Produce replacement wording.

2. **idf-example subagent**
   - Inspect `examples/esp_idf/basic`.
   - Fix or document blocking CLI/tick behavior.
   - Narrow broad include boundaries if possible.

3. **ci subagent**
   - Inspect `.github/workflows/ci.yml`.
   - Add missing guards and pure IDF builds if feasible.

4. **metadata subagent**
   - Inspect README, CHANGELOG, SECURITY, `library.json`, `idf_component.yml`, docs.
   - Align versions and release/readiness status.

5. **api-doc subagent**
   - Ensure public headers contain contracts from Prompts 3–7.

## Documentation honesty requirements

Replace unsupported claims with wording like:

```text
production-oriented
industry-readiness hardened
framework-neutral core
native fake-transport tested
Arduino ESP32-S2/S3 build-tested
ESP-IDF component/example present
pure ESP-IDF build tested only if CI/local evidence exists
hardware/optical/interrupt validation pending unless captured
```

README must include:

- current readiness classification,
- validation evidence table,
- unvalidated/pending matrix,
- package matrix: PicoStar vs SOT-5X3,
- address matrix,
- VDD vs 5.5 V tolerant I/O distinction,
- latest/fresh sample semantics,
- FIFO CRC semantics,
- dirty config state,
- build commands,
- CI commands,
- hardware validation procedure link.

## ESP-IDF example requirements

Fix or explicitly document:

- The example owns its own I2C bus and is diagnostic/bring-up unless production locking is truly shown.
- Core remains free of ESP-IDF includes.
- Include dirs should not expose the repo root unnecessarily; prefer `include/` and local `main/`.
- `device.tick()` must not be stalled indefinitely by blocking CLI input if the example advertises tick-driven behavior.
  - Prefer separate periodic tick task.
  - Or use nonblocking console polling.
  - Or label the CLI as blocking diagnostic-only and avoid relying on tick during idle input.
- Transport status mapping comments must match actual mapping.

## CI requirements

Add/update CI to run:

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

If feasible, add pure ESP-IDF builds:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

If not feasible, clearly document them as pending and do not claim CI IDF coverage.

## Add/adjust guard scripts

If useful, add:

```text
tools/check_readiness_claims.py
tools/check_public_api_docs.py
```

These should fail on:

- unsupported “production-grade” claim,
- missing public API lifecycle/freshness docs,
- missing IDF contract run in CI,
- stale SECURITY supported version.

Keep scripts simple and maintainable.

## Metadata cleanup

Update:

- `SECURITY.md`: supported versions match current release plan.
- `CHANGELOG.md`: add `Unreleased` hardening section.
- `library.json`: description should not overclaim.
- `idf_component.yml`: description should not overclaim.
- `ASSUMPTIONS.md`: remove stale assumptions or update with fixed behavior.
- docs index / IDF docs as needed.

## Required validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_version_header_contract.py
python tools/check_readiness_claims.py
python tools/check_public_api_docs.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
```

If a guard script was not added, omit it and explain why.

Attempt IDF builds if available.

## Report

Create:

```text
docs/OPT4001_H5_DOCS_IDF_CI_METADATA_REPORT.md
```

Include:

- claims changed,
- examples changed,
- CI changes,
- metadata changes,
- public contracts added,
- tests/commands results,
- remaining unvalidated claims intentionally avoided.

Update finding-to-prompt plan.

## Commit and sync

```bash
git diff --check
git status --short
git add .
git commit -m "docs: align OPT4001 readiness claims and IDF integration"
git push
```

## Final response

Report exact changes, tests, CI changes, commit hash, push result, and that Prompt 9 will create HIL/hardware validation and final report.
