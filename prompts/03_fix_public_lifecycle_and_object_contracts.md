# Prompt 3 — Fix H2 plus Object Lifecycle, Copy/Move, Thread/ISR/Public API Contracts

Continue on `hardening/opt4001-industry-readiness`. Complete only this prompt, commit, sync, and stop.

## Findings from exploration report

H2: Public raw register APIs can touch I2C while driver is uninitialized.

Evidence:

- `readRegisters()` checks `_initialized`.
- `readRegister16()` and `writeRegister16()` do not.
- `end()` marks `_initialized=false` but callbacks remain cached.
- Failed `begin()` can store `_config` before returning failure.

Also reported:

- `OPT4001` owns mutable runtime state and callback/user pointers but does not delete copy/move operations.
- Public headers lack thread-safety, ISR-safety, reentrancy, callback-recursion, and blocking contracts.

## Goal

Make lifecycle behavior safe and explicit. Raw public APIs must not touch the bus while uninitialized. The class must not be accidentally copied/moved. Public contracts must be visible in headers, not only README.

## Required first commands

```bash
git status --short
git branch --show-current
```

Stop if dirty.

## Subagents

Spawn:

1. **lifecycle subagent**
   - Inspect all public APIs that can touch I2C.
   - Identify which already guard `_initialized` and which do not.
   - Separate private raw helpers used during `begin()/probe()/recover()` from public guarded APIs.

2. **object-model subagent**
   - Inspect class members and determine copy/move risk.
   - Propose deleted copy/move declarations and tests.

3. **public-contract-docs subagent**
   - Draft Doxygen comments for lifecycle, thread safety, ISR safety, reentrancy, and blocking.

4. **tests subagent**
   - Add fake-bus tests for all lifecycle states.

## Implementation requirements

### Public raw register API guards

Implement:

- `readRegister16()` before `begin()` returns `NOT_INITIALIZED` and does not touch I2C.
- `writeRegister16()` before `begin()` returns `NOT_INITIALIZED` and does not touch I2C.
- Same behavior after failed `begin()`.
- Same behavior after `end()`.
- Same behavior while health state is `OFFLINE` if the project’s normal public operations are expected not to touch the bus while offline; otherwise document exception clearly.
- Internal initialization/probe/recover code must use private raw helpers that are not public lifecycle APIs.

Avoid recursion and avoid making `probe()` require `begin()`.

### `end()` / shutdown

Clarify behavior:

- If `end()` is best-effort, document it and ensure it does not leave the object half-initialized.
- If a status-returning shutdown is useful, add `Status shutdown()` and keep `end()` as compatibility best-effort. Do this only if the existing API pattern supports it cleanly.

### Copy/move deletion

In the `OPT4001` class declaration:

```cpp
OPT4001(const OPT4001&) = delete;
OPT4001& operator=(const OPT4001&) = delete;
OPT4001(OPT4001&&) = delete;
OPT4001& operator=(OPT4001&&) = delete;
```

Add compile-time/static tests where practical.

### Public Doxygen contracts

Add or improve public header comments for:

- all APIs requiring `begin()`,
- all APIs that can perform I2C,
- non-ISR-safe public APIs,
- non-thread-safe instance contract,
- callback non-reentrancy,
- external serialization on shared buses,
- what `end()` means,
- what health/offline means for public operations.

## Required tests

Add native tests for:

1. Public `readRegister16()` before `begin()` returns `NOT_INITIALIZED`, no bus touch.
2. Public `writeRegister16()` before `begin()` returns `NOT_INITIALIZED`, no bus touch.
3. Failed `begin()` leaves public raw APIs guarded.
4. `end()` leaves public raw APIs guarded.
5. Optional: offline public raw register behavior matches documented policy.
6. Copy/move operations are compile-time disabled.
7. Internal `probe()` still works without requiring prior `begin()`.

## Documentation updates

Update:

- README lifecycle/API section,
- public header comments,
- AGENTS.md if needed,
- finding-to-prompt plan marking H2 complete/pending.

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

Skip a command only if it does not exist; report exact reason.

Attempt IDF builds if available.

## Report

Create:

```text
docs/OPT4001_H2_LIFECYCLE_OBJECT_CONTRACTS_REPORT.md
```

Include:

- old behavior,
- new behavior,
- API changes,
- test matrix,
- exact command results,
- remaining limitations.

## Commit and sync

```bash
git diff --check
git status --short
git add .
git commit -m "fix: guard OPT4001 lifecycle and object contracts"
git push
```

## Final response

Report exact changes, tests, commit hash, push result, and that Prompt 4 will fix probe/device-ID/status diagnostics.
