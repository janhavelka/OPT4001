# Prompt 6 — Documentation Honesty, Public API Contracts, Examples, ESP-IDF CI, and Release Metadata

Continue on `hardening/opt4001-industry-readiness`. Complete only this prompt, commit, sync, and stop.

## Goal of this prompt

Fix documentation and integration readiness issues:

1. Downgrade premature "production-grade" claims until validation evidence exists.
2. Move operational contracts into public Doxygen/header comments.
3. Improve ESP-IDF example honesty and prevent CLI/tick blocking.
4. Add/verify CI coverage for IDF contract and pure ESP-IDF builds where feasible.
5. Update stale release/security metadata.

## Required first steps

```bash
git status --short
git branch --show-current
```

Stop if dirty.

## Subagents

Spawn:

1. **docs-honesty subagent**
   - Find "production-grade", "validated", "ESP-IDF ready", and similar claims.
   - Recommend honest replacements.
2. **public-api-doc subagent**
   - Inspect `include/OPT4001/*.h`.
   - Add Doxygen for lifecycle, blocking, freshness, CRC, flags, thread/ISR, units.
3. **idf-example subagent**
   - Inspect ESP-IDF example for bus ownership, error mapping, console blocking, tick scheduling, include boundaries.
4. **ci subagent**
   - Inspect workflow and propose CI additions.
5. **metadata subagent**
   - Inspect `library.json`, `idf_component.yml`, `CHANGELOG.md`, `SECURITY.md`.

## Documentation honesty

Replace claims like:

```text
production-grade
industry-grade
fully validated
ESP-IDF validated
```

with accurate wording unless backed by captured evidence.

Preferred wording:

```text
framework-neutral OPT4001 driver under industry-readiness hardening
production-oriented architecture
Arduino ESP32-S2/S3 build-tested
native fake-transport tested
ESP-IDF component/example present; pure IDF build coverage depends on CI/environment
hardware and optical validation pending
```

README must include:

- readiness status,
- what is validated,
- what is not validated,
- package feature matrix,
- hardware/optical validation matrix,
- latest vs fresh semantics,
- `Version.h` build reproducibility,
- ESP-IDF status,
- example scope.

## Public Doxygen/header contracts

Add comments to public APIs covering:

- initialization required,
- which APIs can perform I2C,
- worst-case I2C transaction count where practical,
- blocking behavior and `nowMs`/timeout requirements,
- latest vs fresh sample semantics,
- CRC status and populated sample behavior,
- flags clear-on-read/write behavior,
- `readFlagsRaw()` clear-on-read warning,
- units for `Sample::lux`, milli-lux, micro-lux helpers,
- thread-safety, ISR-safety, callback reentrancy,
- package-specific behavior: PicoStar vs SOT-5X3,
- INT pin availability and limitations,
- general-call reset bus-wide side effect,
- `end()` best-effort or fallible shutdown behavior.

Add a grep/contract script if useful, for example:

```text
tools/check_public_api_docs.py
```

## ESP-IDF example

Fix or document:

- IDF CLI should not stall periodic `device.tick()` on blocking `getchar()` in the same loop. Prefer nonblocking console input, a separate CLI task, or a separate periodic tick task.
- Narrow broad include directories if the example exposes the repo root unnecessarily.
- Clarify the example owns its bus and is a diagnostic/bring-up adapter, not a production shared-bus manager.
- If shared-bus locking is demonstrated, use a mutex around the transport callback and document ownership.
- Keep core framework-neutral.

Do not overbuild an application framework. Keep it small and clear.

## CI

Update CI to run, if feasible:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_public_api_docs.py   # if added
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

If CI cannot realistically run `idf.py`, add a documented pending item and avoid claiming CI IDF coverage.

## Metadata cleanup

Update:

- `SECURITY.md` supported versions (`1.0.x` or whatever matches release plan).
- `CHANGELOG.md` with unreleased hardening notes.
- `library.json` / `idf_component.yml` description so it does not overclaim.
- `ASSUMPTIONS.md` if assumptions changed.

## Required local validation

Run all available:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
```

Run any new doc contract script.

Run IDF builds if available.

## Report update

Create:

```text
docs/OPT4001_DOCS_EXAMPLES_CI_REPORT.md
```

Include:

- claims changed,
- public API docs added,
- example changes,
- CI changes,
- local and CI-intended validation,
- remaining claims that are intentionally conservative.

## Commit and sync

```bash
git diff --check
git status --short
git add .
git commit -m "docs: align OPT4001 readiness claims and contracts"
git push
```

## Final response for this prompt

Report files changed, tests, CI changes, commit hash, sync result, and what remains for Prompt 7.
