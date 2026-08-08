# AGENTS.md - OPT4001 Production Embedded Guidelines

## PlatformIO

Before editing, fetch remotes and fast-forward the newest intended working
branch to its upstream. Stop and report dirty, divergent, or conflicted state;
never overwrite work to force a sync.

On Windows, use `.\scripts\pio.cmd <arguments>`; it selects the current user's
VS Code-managed installation. Never install another PlatformIO Core; if the
wrapper cannot find it, stop and report the missing installation.

## Role and Target
You are a professional embedded software engineer building a production-grade OPT4001 ambient light sensor library.

- Target: ESP32-S2 / ESP32-S3, Arduino framework, PlatformIO.
- Goals: deterministic behavior, long-term stability, clean API contracts, portability, and no surprises in the field.
- These rules are binding.

---

## Repository Model (Single Library)

```
include/OPT4001/          - Public API headers only (Doxygen)
  CommandTable.h          - Register addresses and bit masks
  Status.h
  Config.h
  OPT4001.h
  Version.h               - Auto-generated (do not edit)
src/                      - Implementation (.cpp)
examples/
  01_*/
  common/                 - Example-only helpers (Log.h, BoardConfig.h,
                            I2cTransport.h, I2cScanner.h, CliLineBuffer.h)
test/                     - Native/unit tests
platformio.ini
library.json
README.md
CHANGELOG.md
AGENTS.md
```

Rules:
- `examples/common/` is NOT part of the library. It simulates project glue and keeps examples self-contained.
- No board-specific pins/bus in library code; only in `Config`.
- Public headers only in `include/OPT4001/`.
- Examples demonstrate usage and may use `examples/common/BoardConfig.h`.
- Keep the layout boring and predictable.

Framework-boundary rules:
- Core/public headers and `src/` must remain framework-neutral. Do not include Arduino or ESP-IDF headers there unless the exception is documented in Doxygen and this file.
- Arduino examples may use Arduino APIs.
- ESP-IDF examples must be native IDF examples using `app_main`, `driver/i2c_master.h`, native GPIO/timer/task APIs, and fixed C buffers or `esp_console`/argtable.
- ESP-IDF examples must not include Arduino CLI sources or use `ArduinoCompat`, `IdfArduinoCompat`, `Arduino.h`, `Wire.h`, `String`, `Serial`, `TwoWire`, or equivalent Arduino facades.
- Keep command parity through repo-local command contracts/checkers, not by compiling Arduino sources into ESP-IDF examples.

---

## Core Engineering Rules (Mandatory)

- Prefer simplicity, clarity, correctness, robustness, safety, and readability over clever abstractions or speculative flexibility.
- Before coding, inspect whether existing code can be simplified, reused, or deleted.
- Prefer deleting unnecessary code over adding new code.
- Prefer extending existing owners/modules/contracts over creating parallel abstractions.
- Before adding a new service, class, file, interface, or abstraction, verify there is a concrete current need and a clear caller or test.
- Do not add placeholder classes, future stubs, empty managers, broad frameworks, plugin systems, service registries, or speculative extension points.
- Keep changes tightly scoped to the user's request.
- Deterministic: no unbounded loops/waits; all timeouts via deadlines, never `delay()` in library code.
- No unbounded waits, retries, loops, allocations, queues, or buffers in steady paths.
- Non-blocking lifecycle: `Status begin(const Config&)`, `void tick(uint32_t nowMs)`, `void end()`.
- Any I/O that can exceed about 1-2 ms must be split into state-machine steps driven by `tick()` or exposed as a clearly bounded blocking helper.
- Every hardware operation that can block must have a timeout and an observable failure path.
- Recovery logic must be bounded, deterministic, and testable.
- Prefer explicit state, explicit ownership, and small local helpers over hidden global state.
- Do not hide hardware failures behind silent retries or fake success.
- No heap allocation in steady state; no `String`, `std::vector`, or `new` in normal operations.
- Avoid dynamic allocation in steady embedded paths unless it is already an accepted local pattern and the bound is clear.
- No logging in library code; examples may log.
- No macros for constants; use `static constexpr`. Macros only for conditional compile or logging helpers.

---

## I2C Manager + Transport (Required)

- The library MUST NOT own I2C. It never touches `Wire` directly.
- The I2C bus must have one clear owner.
- Device drivers must not directly own or reconfigure a shared bus unless this repository's architecture explicitly says so.
- `Config` MUST accept transport callbacks (`i2cWrite`, `i2cWriteRead`) and optional timing/GPIO hooks.
- I2C transactions must be timeout-bounded and report errors clearly.
- Transport errors MUST map to `Status`; do not leak `Wire`, `esp_err_t`, or platform-specific errors through the public API.
- The library MUST NOT configure bus timeouts or pins.
- Optional INT-pin behavior must go through `Config::gpioRead`; do not call Arduino GPIO APIs directly from the driver.
- Blocking helpers must use `Config::nowMs` / caller time where available and `Config::cooperativeYield` only as a cooperative hook.
- Do not implement chip protocols manually if an existing hardened project library already provides the needed timeout, recovery, and testability behavior.
- Keep chip-level protocol code inside the driver/wrapper. Keep application policy outside the chip driver.
- Do not add fake devices, simulated buses, or test doubles to production paths.

---

## Status / Error Handling (Mandatory)

All fallible APIs return `Status`:

```cpp
struct Status {
  Err code;
  int32_t detail;
  const char* msg;  // static string only
};
```

- Silent failure is unacceptable.
- No exceptions.
- Validation/precondition failures must not be counted as transport failures.

---

## OPT4001 Driver Requirements

- I2C address validation by package variant:
  - PicoStar package: fixed 0x45.
  - SOT-5X3 package: supported selectable addresses 0x44, 0x45, 0x46.
- Check device presence in `begin()` by reading and decoding DEVICE_ID.
- Support package-specific lux scaling and fixed-pattern validation.
- Support range selection: RANGE_0..RANGE_8 and AUTO.
- Support conversion times from 600 us through 800 ms.
- Support operating modes:
  - **Power down**.
  - **One-shot forced auto-range**.
  - **One-shot using previous range history**.
  - **Continuous**.
- Support quick-wake configuration.
- Support sample decoding from RESULT and RESULT_LSB_CRC registers: exponent, mantissa, ADC codes, counter, CRC, and lux.
- Support optional CRC verification with clear warning/error behavior.
- Support four-deep burst/FIFO history reads: newest sample plus FIFO0..FIFO2.
- Support threshold registers and conversion between threshold encoding, ADC codes, and lux.
- Support interrupt configuration: latch/transparent, polarity, fault count, INT direction, threshold interrupt, every-conversion pulse, FIFO-full pulse.
- Support FLAGS decoding and documented clear behavior.
- Support blocking read helpers with explicit timeout and poll-friendly `tryRead*` helpers that do not treat "not ready" as transport failure.
- Support general-call soft reset only through explicit API with clear bus-wide side effect documentation.

---

## Driver Architecture: Managed Synchronous Driver

The driver follows a managed synchronous model with health tracking:

- Public I2C operations are blocking and bounded.
- `tick()` may be used for conversion wait, conversion-ready polling, and cached sample state.
- Health is tracked via tracked transport wrappers; public API never calls `_updateHealth()` directly.
- Recovery is manual via `recover()`; the application controls retry strategy.

### DriverState (4 states only)

```cpp
enum class DriverState : uint8_t {
  UNINIT,
  READY,
  DEGRADED,
  OFFLINE
};
```

State transitions:
- `begin()` success -> READY
- Any I2C failure in READY -> DEGRADED
- Success in DEGRADED/OFFLINE -> READY
- Failures reach `offlineThreshold` -> OFFLINE
- `end()` -> UNINIT

### Transport Wrapper Architecture

All I2C goes through layered wrappers:

```
Public API (readSample, setRange, readFlags, etc.)
    down to
Register helpers (readRegister16, writeRegister16, readRegisters)
    down to
TRACKED wrappers (_i2cWriteReadTracked, _i2cWriteTracked)
    down to
RAW wrappers (_i2cWriteReadRaw, _i2cWriteRaw)
    down to
Transport callbacks (Config::i2cWrite, Config::i2cWriteRead)
```

Rules:
- Public API methods NEVER call `_updateHealth()` directly.
- Register helpers use TRACKED wrappers so health is updated automatically.
- `probe()` uses RAW wrappers and does not update health.
- `recover()` tracks probe/readback failures because the driver is initialized.

### Health Tracking Rules

- `_updateHealth()` is called only inside tracked transport wrappers.
- State transitions are guarded by `_initialized`; no DEGRADED/OFFLINE before `begin()` succeeds.
- Do not track config/param validation errors as health failures.
- Do not track precondition errors as health failures.
- Reset consecutive failures on tracked success.
- Keep lifetime success/failure counters saturating or wrapping intentionally; do not leave overflow behavior accidental.

---

## Versioning and Releases

Single source of truth: `library.json`. `Version.h` is auto-generated and must never be edited manually.

SemVer:
- MAJOR: breaking API/Config/enum changes.
- MINOR: new backward-compatible features or error codes.
- PATCH: bug fixes, refactors, docs.

Release steps:
1. Update `library.json`.
2. Regenerate `Version.h`.
3. Update `CHANGELOG.md` using Added/Changed/Fixed/Removed.
4. Update `README.md` and examples if API or behavior changed.
5. Run tests and ESP32-S2/S3 builds.
6. Commit and tag: `Release vX.Y.Z`.

---

## Naming Conventions

- Member variables: `_camelCase`
- Methods/functions: `camelCase`
- Constants: `CAPS_CASE`
- Enum values: `CAPS_CASE` for register-style enums, otherwise preserve existing public API style
- Locals/params: `camelCase`
- Config fields: `camelCase`

---

## OPT4001 repository rules for coding agents

- Core code under `include/` and `src/` must remain framework-neutral: no Arduino, Wire, ESP-IDF, FreeRTOS, Serial, logging frameworks, global bus objects, hidden framework delays, pin ownership, task ownership, or heap-heavy framework types.
- I2C is injected and non-owning. Bus ownership, locking, timeout policy, reset-line ownership, INT GPIO ownership, and application scheduling belong to examples/adapters or the application.
- Public fallible APIs must return structured `Status`; do not silently ignore failed I2C writes or collapse diagnostics without documentation.
- Public APIs are not ISR-safe unless explicitly proven. Driver instances are not internally thread-safe; external serialization is required.
- Transport callbacks must not re-enter the same driver instance.
- PicoStar/YMN and SOT-5X3/DTS package differences must remain explicit. PicoStar lacks ADDR and INT. SOT-5X3 has ADDR and INT.
- Fresh sample semantics must be tied to hardware evidence: `CONVERSION_READY_FLAG`, INT when configured and available, or output counter changes.
- Do not treat repeated current output-register reads as new fresh samples unless the sample counter/ready evidence says so.
- Numeric helpers must validate exponent/mantissa/threshold ranges and must not perform undefined shifts.
- Multi-register configuration paths must either avoid partial hardware/cache divergence or expose a dirty/resync-required state.
- The current Arduino and ESP-IDF CLIs are diagnostic/bring-up examples unless they clearly demonstrate production bus management.
- Do not claim hardware validation, optical validation, interrupt validation, address-pin validation, FIFO validation, or pure ESP-IDF validation without captured evidence.
- Preserve dirty user changes. Never revert unrelated work unless the user explicitly asks.
- After each hardening prompt: run checks, update a report, commit, push/sync if possible, and stop.
