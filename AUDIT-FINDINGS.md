# OPT4001 Audit Findings

**This is a work list, not documentation. Delete it when the items are closed.**

Audit date: 2026-08-29, against commit `56b509a`. Every finding below was raised by
one reviewer and then independently attacked by three more whose instruction was to
*refute* it; only findings a majority failed to refute are listed. Device facts were
checked against `docs/reference/OPT4001_datasheet.pdf` (SBOS993A) directly, not
against the markdown summary.

Baseline at the time of writing: 133/133 native tests pass, and
`native_core_no_arduino` builds clean under `-Wall -Wextra -Wpedantic -Wconversion
-Wsign-conversion -Werror`. **No finding below is caught by the current test suite** —
that is itself a finding (§8).

Findings are grouped so that one refactor closes several at once. Within each group
the proposal is concrete and deliberately minimal; where a reviewer's counter-proposal
was better than the original, the counter-proposal is what is written here.

---

## Already fixed in this pass

These were mechanically certain and are applied in the working tree:

| Change | Location |
| --- | --- |
| `getConversionTimeUs()/Ms()` now honour their documented "invalid returns 0" guard instead of indexing a 12-entry table unchecked | `src/OPT4001.cpp` |
| Unreachable `lsb > 0.0f` branch removed from `luxToThreshold()`; the range check is now unconditional | `src/OPT4001.cpp` |
| `crcValid` doc corrected — it is always computed, whatever `Config::verifyCrc` says | `include/OPT4001/OPT4001.h` |
| `consecutiveFailures()` documented as saturating at `UINT8_MAX`, not `UINT32_MAX` | `include/OPT4001/OPT4001.h` |
| OFFLINE exception list corrected to include `probe()`, `startResetAndReapply()`, `poll()` | `include/OPT4001/OPT4001.h` |
| SOT-5X3 resolution table: `47.344` → `57.344` (a TI typo; annotated as a deviation) | `docs/reference/OPT4001_datasheet.md` |
| CRC section rewritten: `X[3:0]` is the expected CRC nibble, not a zero-syndrome | `docs/reference/OPT4001_datasheet.md` |
| PicoStar pin table relabelled Top View (SBOS993A Figure 6-1) | `docs/reference/OPT4001_datasheet.md` |
| Package excluded ~10 MB of vendor PDFs from every consumer (`pkg pack`: 10 MB → 0.13 MB) | `library.json` |
| ESP-IDF build artifacts (`sdkconfig`, `sdkconfig.old`, `dependencies.lock`) ignored | `.gitignore` |
| ~110 lines of dead cross-project code removed (a dependency-pin generator gated on the directory being named `tunnelmonitor`) | `scripts/generate_version.py` |
| Dead alias header deleted with its checker entry | `examples/common/TransportAdapter.h` |
| CI action SHA-pinning guard restored as a standalone checker (it was collateral in the `check_readiness_claims.py` deletion) | `tools/check_ci_action_pins.py` |
| `selfcheck` no longer restores thresholds it never successfully read (§9a) | `examples/01_basic_bringup_cli/main.cpp` |
| `config write` accepts `IN_PROGRESS` as the one-shot success it is (§9b) | `examples/01_basic_bringup_cli/main.cpp` |

Plus the documentation cleanup: `docs/reports/` (three dated campaign reports),
`docs/validation/validation-status.md`, `tools/check_readiness_claims.py` and
`tools/check_public_api_docs.py` deleted; README 785 → 372 lines; `AGENTS.md`,
`CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`, `CODEOWNERS`, `ASSUMPTIONS.md`,
`docs/README.md` and the two app-note summaries corrected.

---

## 1. Blocking reads cannot succeed on real hardware — CRITICAL

**`src/OPT4001.cpp:126` `blockingPollLimit()`, used at `:1006` and `:1035`.**

```cpp
uint64_t polls = (static_cast<uint64_t>(timeoutMs) + extraMs + 1ULL) * 16ULL + 16ULL;
```

The cap conflates *loop iterations* with *milliseconds*. The wait loop increments
`polls` on every iteration, including the pure-CPU spin iterations at `:1038-1042`
that only re-read the clock. `Config::cooperativeYield` is optional and defaults to
`nullptr`, in which case `_cooperativeYield()` does nothing at all — so the loop is a
bare busy-spin running millions of iterations per second.

**Failure:** `readBlockingLux(lux, 1500)` with the default 100 ms conversion time gives
`maxPolls = (1500 + 101 + 1) * 16 + 16 = 25 648`. On an ESP32-S3 at 240 MHz that is
exhausted in single-digit milliseconds, and the call returns `TIMEOUT` before the
conversion could possibly have completed. This affects `readBlocking()`,
`readFreshBlocking()` and `readBlockingLux()` — i.e. the exact call in the README
quick start.

**Why the tests miss it:** every blocking test configures `fakeAdvancingYield`
(`test/test_basic.cpp:192`), which advances the fake clock **1 ms per iteration**, so
iterations and milliseconds coincide and the cap is never the binding constraint.

**Proposal.** The cap exists only to bound the loop when the clock never advances, so
make it measure *lack of clock progress* rather than iteration count. Apply to both
loops and delete `blockingPollLimit()`:

```cpp
// Bounds the wait when Config::nowMs never advances (a precondition violation).
// This is NOT a timeout — the deadline is the timeout. It counts only consecutive
// iterations observing an unchanged millisecond, so a healthy clock resets it long
// before it can trip.
static constexpr uint32_t CLOCK_STALL_ITERATIONS = 4096U;

uint32_t lastMs = _nowMs();
uint32_t stalled = 0;
while (/* deadline not reached */) {
  const uint32_t now = _nowMs();
  if (now != lastMs) { lastMs = now; stalled = 0; }
  else if (++stalled >= CLOCK_STALL_ITERATIONS) { break; }
  ...
}
```

> Do **not** take the alternative "only count iterations that made an I2C attempt":
> `test_read_blocking_times_out_with_stalled_clock` (`test/test_basic.cpp:1102`) uses
> the no-op `fakeYield` with a clock nothing advances, so the spin branch is taken
> every iteration and never touches I2C — the cap is that loop's only exit, and
> removing the increment turns the test into an infinite loop.

Also update `include/OPT4001/OPT4001.h:393` ("finite internal poll cap") and the
README, and document the new `INVALID_CONFIG` "Config::nowMs is not advancing" result.

---

## 2. The driver has two clocks and compares them against each other — CRITICAL

**`src/OPT4001.cpp:2923` `_nowMs()`, and every writer/reader of `_conversionStartMs`
and `_lastSampleTimestampMs`.**

Two independent time sources feed the same fields:

| Stamped with `_nowMs()` (the `Config::nowMs` hook, **0 when unset**) | Stamped/compared with the caller's `nowMs` argument |
| --- | --- |
| `_applyConfig()` → `_finishApplyConfig(_nowMs())` | `tick(nowMs)` |
| `startConversion()` `:567` | `poll(nowMs, n)` → `_pollReadGateElapsed(nowMs)` |
| `writeConfiguration()` `:1675`, `:1693` | `_shouldProbeCounterForFreshness(nowMs)` |
| `_markFreshSampleConsumed()`, `_cacheSample()` | `_markFreshSampleConsumedAt(s, nowMs)`, `_finishApplyConfig(nowMs)` |

**Failure:** an app that uses `begin()` (synchronous, stamps via `_nowMs()`) and then
drives `tick(millis())` without configuring `Config::nowMs` gets
`_conversionStartMs == 0` while `nowMs` is a real millisecond count. Every gate
computes `millis() - 0`, which is always "elapsed" — the timing gate is silently
disabled for the life of the driver. The mirror case (a `Config::nowMs` hook plus a
different time base passed to `poll()`) fails the same way.

**Proposal — one clock, with the caller's `nowMs` feeding the single reader.**

```cpp
// header, private:
uint32_t _hostMs = 0;          ///< Last caller-supplied timestamp.
bool     _timeBaseValid = false;

// impl:
uint32_t OPT4001::_nowMs() const {
  return _config.nowMs != nullptr ? _config.nowMs(_config.timeUser) : _hostMs;
}
void OPT4001::_observeHostMs(uint32_t nowMs) {
  if (_config.nowMs != nullptr) { return; }
  _hostMs = nowMs;
  if (!_timeBaseValid) { _timeBaseValid = true; /* rebase stamps taken before this */ }
}
```

`tick()` and `poll()` call `_observeHostMs(nowMs)` first, then everything uses
`_nowMs()`. The `(uint32_t nowMs)` overloads of `_finishApplyConfig`,
`_markFreshSampleConsumedAt`, `_cacheSampleAt`, `_pollReadGateElapsed` and
`_shouldProbeCounterForFreshness` then collapse into their no-arg forms — the refactor
**deletes** code.

Three constraints a reviewer identified:

1. **Do not feed `_hostMs` from `sampleAgeMs()`.** It is a `const` pure query; letting
   it write the time base lets a caller poison the conversion gates.
2. **Do not reset `_hostMs` in `_clearRuntimeState()`.** That helper is called from
   *inside* `_finishApplyConfig()` **before** `_conversionStartMs = nowMs`, so the
   reset would guarantee the two operands disagree — strictly worse than today. Reset
   it in `bind()`/`unbind()` only, where the clock domain actually changes.
3. **Handle the startup transient.** `begin()` runs before any `tick()`, so
   `_conversionStartMs` is stamped 0 while the app's clock is at, say, 5000; the first
   `tick(5001)` then sees `5001 - 0 >= 100`. That is what `_timeBaseValid` is for.

---

## 3. Readiness resolution burns the clear-on-read FLAGS register — MAJOR

Four confirmed findings share one root cause. `FLAGS` (`0x0C`) is clear-on-read: every
read consumes the device's latched `FLAG_H`/`FLAG_L` threshold view as well as
`CONVERSION_READY_FLAG`.

### 3a. The time gate is applied to the harmless probe and not to the destructive one

`_refreshReadinessEvidence()` (`src/OPT4001.cpp:1883`) gates the **counter** read —
which is side-effect free — behind `_shouldProbeCounterForFreshness()`, then falls
through to `_pollConversionReadyFlag()` → `readFlags()` **with no gate at all**. So
when the conversion window has *not* elapsed the driver skips the cheap read and does
the destructive one. With an 800 ms conversion time and a caller polling
`conversionReady()` from `loop()`, that is hundreds of clear-on-read FLAGS
transactions per conversion, each destroying latched threshold evidence.

### 3b. The poll engine tries FLAGS first, defeating the documented escape hatch

`_pollReadJob` goes `WAIT_READY → READ_FLAGS → (maybe) READ_COUNTER` — the opposite
order to the synchronous path. `OPT4001.h:353` tells callers to "use configured
INT/counter evidence when those flags must remain untouched", but the poll path reads
FLAGS before the counter is ever consulted.

**Proposal (3b):** in `WAIT_READY`, pick the entry step by whether a baseline exists —
`_pollStep = _shouldProbeCounterForFreshness(nowMs) ? READ_COUNTER : READ_FLAGS;` — and
in `READ_COUNTER`'s stale branch **leave `_pollStep` at `READ_COUNTER`** rather than
routing to `READ_FLAGS`. Routing back produces an alternating FLAGS/COUNTER loop that
still destroys the latched flags every other cycle.

### 3c. The read job ping-pongs and never re-arms its gate

`READ_COUNTER`'s not-fresh branch sets `_pollStep = READ_FLAGS` (`:2567`) and
`READ_FLAGS`'s not-ready branch can send it back (`:2547`). Neither returns to
`WAIT_READY`, so after the first gate pass every `poll()` spends its whole instruction
budget on readiness reads for the rest of the conversion.

**Proposal (3c):** add a dedicated retry deadline. **Do not** re-arm by writing
`_conversionStartMs = nowMs` as originally proposed — that field also drives the
one-shot timeout at `:316` and `:1038` (pushing it out on every retry turns a bounded
wait into a hang) and is published in `SettingsSnapshot::conversionStartMs`. Instead:

```cpp
uint32_t _pollReadRetryMs = 0;
bool     _pollReadRetryArmed = false;
```

On each failed resolution set `_pollReadRetryMs = nowMs + max(1, getConversionTimeMs()/16)`,
`_pollReadRetryArmed = true`, `_pollStep = WAIT_READY`. Gate `WAIT_READY` on it. Clear
in `_finishPollJob`, `startReadSample`/`startReadBurst` and `_clearRuntimeState`.

### 3d. The read job can never terminate

With the driver in `POWER_DOWN` and no conversion started, `_pollReadGateElapsed()`
returns **true** (`:2745`, the `return true;` fallthrough), so `startReadSample()` +
`poll()` issues one clear-on-read FLAGS transaction per poll **forever**: the job stays
`IN_PROGRESS`, `pollBusy()` stays true, and every other tracked API returns `BUSY`.

**Proposal (3d):** reject the state up front in `startReadSample()`/`startReadBurst()`
— non-continuous mode, `!_conversionStarted`, no cached evidence means no conversion
can ever complete — and give the job a deadline captured **on first `poll()` entry**,
not at `start*()` time (`_nowMs()` returns 0 when the hook is absent, which is a
supported configuration). `_shouldProbeCounterForFreshness(uint32_t)` has the same
`return true;` fallthrough and needs the same treatment.

> A related consequence: because `_conversionStarted` is only cleared when readiness
> resolves, a one-shot whose flag was consumed elsewhere latches it forever and
> `startConversion()` returns `BUSY` permanently. Give the one-shot a finite
> abandonment deadline expressed as *elapsed since start* (not an absolute stamp, for
> the same `_nowMs()` reason), after which `_conversionStarted` is released so the
> caller can re-trigger. Note the hardware conversion is not cancelled by this.

---

## 4. Freshness evidence is unsound in three places

### 4a. INT freshness level-polls a 1 µs pulse — CRITICAL

`_intFreshEvidenceAsserted()` (`src/OPT4001.cpp:1925`) accepts INT as proof of a fresh
conversion for exactly `INT_CFG = EVERY_CONVERSION` and `FIFO_FULL` — and SBOS993A
Table 8-2 says those are precisely the two settings where INT is **a ~1 µs pulse**, not
a level. `Config::gpioRead` is a polled level read (`examples/common/BoardConfig.h:37`
is `digitalRead(pin) != 0`). The only INT_CFG that holds a steady level is `THRESHOLD`,
which the guard explicitly rejects. The accepted set and the observable set are
disjoint. `include/OPT4001/Config.h:119` already says "~1 us pulse" — the driver
contradicts its own header.

Two failure modes:

- **Dead fast path.** A 1 µs pulse is caught with probability ~1e-5 per poll, so the
  INT path never fires and every `readSample()` falls through to the clear-on-read
  FLAGS read — the exact thing a caller configured INT to avoid.
- **False positive, and it is worse than it looks.** If the INT net reads asserted for
  any other reason (missing pull-up, or a second device holding the shared open-drain
  line — a topology the datasheet explicitly endorses), `_refreshReadinessEvidence()`
  latches ready **with no time gate**, and `_pollReadJob`'s `WAIT_READY` (`:2516`) jumps
  straight to `READ_BURST_BLOCK` bypassing the gate entirely. Immediately after
  `begin()`, `_lastFreshCounterValid` is false so `_sampleCounterIsFresh()` returns
  true unconditionally — the driver returns the RESULT **reset value** (0x0000, i.e.
  0.00 lux) as a proven-fresh measurement with `Status::Ok()`.

`test/test_basic.cpp:2551 test_int_fresh_evidence_does_not_clear_flags` pins this
behaviour: it sets `gpioLevel = false` with `EVERY_CONVERSION` + ACTIVE_LOW, does not
advance time, and asserts the read succeeds.

**Proposal.** Preferred: demote INT from *proof* to *hint*. Do not let
`_intFreshEvidenceAsserted()` set `ready` directly; use it only to authorise the
counter probe, and require the counter or the ready flag to confirm. Mirror the change
at `:2516` so the INT hint advances to `READ_FLAGS`/`READ_COUNTER` and never straight
to `READ_BURST_BLOCK`. This needs no API change and keeps the signal useful.

Alternative, if genuine INT-driven freshness is wanted: add an ISR-latched event hook
(`using IntEventTakeFn = bool (*)(void* user);` reusing `gpioUser`), take one event per
call, and drop `intPin`/`gpioRead`/polarity from that path — an edge latch has no
level, and the trigger edge already encodes polarity. If you take this route, the
PicoStar guards at `src/OPT4001.cpp:231` and `setPackageVariant()` must also reject
`intEventTake != nullptr`, or a PicoStar config passes validation and then feeds
freshness from a pin the package does not have.

Either way, `ASSUMPTIONS.md` item 3, `OPT4001.h:351-355` and the `conversionReady()`
doc must stop promising evidence the driver cannot obtain.

### 4b. Raw `0x0C` reads throw away the evidence they just read — MAJOR

`readFlagsRaw()` (`:1152`), `readRegister16(0x0C)` (`:1795`) and `readRegisters()`
spanning `0x0C` (`:1783`) all call `_clearReadinessEvidence()` on success **without
looking at the `CONVERSION_READY_FLAG` bit they just read**. The typed `readFlags()`
does the opposite and captures it. Hardware has also cleared the flag by then, so a
completed one-shot is lost permanently.

**Proposal.** Mirror the typed path exactly, **set-only, never clear** — a conditional
clear would break `readFlags()`-then-raw-read sequences by discarding still-valid
evidence. Factor into one helper used by all four sites so they cannot drift again:

```cpp
void OPT4001::_captureReadinessFromFlags(uint16_t raw) {
  if ((raw & cmd::MASK_CONVERSION_READY_FLAG) == 0U) { return; }
  _sampleAvailable = true;
  _conversionReady = true;
  if (_config.mode != Mode::CONTINUOUS) {
    _conversionStarted = false;
    _pendingMode = Mode::POWER_DOWN;
  }
}
```

`readRegisters()` needs the word located by offset `(cmd::REG_FLAGS - startReg) * 2`,
and odd `len` is legal — guard the second byte.

### 4c. The freshness baseline advances from a CRC-failed counter — MAJOR

`readSample()` treats `CRC_ERROR` as data-bearing and calls
`_markFreshSampleConsumed(out)`, which stores `_lastFreshCounter = sample.counter`
(`:2831`). But a CRC failure means the received bits — *including the COUNTER nibble* —
are untrustworthy. A corrupted counter poisons the baseline and can silently reject the
next genuine sample as stale.

**Proposal.** On `CRC_ERROR`, treat the sample as freshness-**neutral**: skip both the
`_sampleCounterIsFresh()` gate and `_markFreshSampleConsumed()`, call `_cacheSample()`
plus `_clearReadinessEvidence()`, and leave `_lastFreshCounter`/`_lastFreshCounterValid`
untouched. This is a contract change across five call sites (`:850`, `:883`, `:920`,
`:802`, `:2585`) and three documents (`README.md`, `OPT4001.h:355`,
`driver-contracts.md:111`) — it permits the same conversion to be delivered twice (once
`CRC_ERROR`, once `OK`), which must be documented.

### 4d. Contested: the counter veto can discard flag-proven samples

Reviewers split 1/3 on this one, but the majority's counter-argument does not hold and
I judge the finding **real**. `readSample()` resolves readiness (possibly consuming the
clear-on-read flag), reads the sample, then applies `_sampleCounterIsFresh()` as a veto
(`:806`). In continuous mode, when exactly 16 conversions have elapsed the counter
aliases: the counter probe fails *because of the alias*, so the code falls through and
**does** consume the flag, and then the veto rejects a sample the flag just proved
fresh. The refuters argued the counter is tried first so the flag is not consumed —
true in general, false in exactly the aliasing case the finding describes.

**Proposal.** Record *which* evidence proved freshness and apply the counter veto only
when the counter was the evidence source. This falls out naturally from a single shared
readiness resolver (§3).

---

## 5. Burst reads can silently return incoherent data

### 5a. Framing is chosen from a cache that can diverge from hardware — CRITICAL

`_readSampleAt()` (`:2316`), `_readBurstBlockTracked()`, `readBurst()` (`:844`) and
`readRegisters()` (`:1760`) all pick I2C framing from `_config.burstMode`, a *mirror*
of the hardware `I2C_BURST` bit. A raw `writeRegister16(0x0B, ...)` marks the cache
dirty but does **not** update the mirror; an external reset or brownout diverges it
silently.

SBOS993A §8.5.2.2 defines the auto-increment behaviour **only for the burst-enabled
case** — it never says what a >2-byte read returns with `I2C_BURST = 0`. So when the
mirror is stale the driver issues a 16-byte single-transaction read whose result the
device documentation does not define, and decodes four samples from it. `verifyCrc`
(on by default) will usually reject the result, but with verification disabled it is
silent, and either way the driver is relying on undefined device behaviour without
knowing it.

**Proposal.** Track what was actually written rather than what was intended:

```cpp
bool _hwBurstEnabled = false;   // proven by the last successful 0x0B write/readback
void OPT4001::_noteIntConfigWrite(uint16_t value, const Status& st) {
  if (st.ok()) { _hwBurstEnabled = (value & cmd::MASK_I2C_BURST) != 0; }
}
```

Call after every `0x0B` write, refresh in `readIntConfiguration(uint16_t&)` on success,
and use `_hwBurstEnabled` at the five framing sites. Reset it wherever hardware is
reset or left mid-config: `_pollConfigJob()`'s general-call step (`:2696`),
`resetAndReapply()` (`:458`), and `cancelPollJob()`'s attach/reset branch.

> **Do not** fold the reset into `_clearRuntimeState()`. `_finishApplyConfig()` calls
> that helper at the *end* of a successful `_applyConfig()`, so it would clobber the
> bit the `INT_CONFIGURATION` write just proved and pin the driver to non-burst
> framing permanently.

### 5b. The non-burst `readBurst()` is not a snapshot — MAJOR

With burst framing off, `readBurst()` assembles the four-slot window from **eight
separate transactions** (`:862-877`). A conversion completing mid-sequence shifts the
FIFO, so a slot can be duplicated or skipped — with all four CRCs valid and
`Status::Ok()` returned. Nothing in the API says the frame is not atomic.

**Proposal.** Prove coherence from hardware rather than asserting it. After the four
slots decode, re-read `RESULT_LSB_CRC` and compare the counter:

```cpp
// Non-burst frames span eight transactions; prove the FIFO did not shift.
uint16_t lsbCrcAfter = 0;
Status guard = _readRegister16Tracked(cmd::REG_RESULT_LSB_CRC, lsbCrcAfter);
if (!guard.ok()) { return guard; }
const uint8_t counterAfter =
    static_cast<uint8_t>((lsbCrcAfter & cmd::MASK_COUNTER) >> cmd::BIT_COUNTER);
if (counterAfter != out.newest.counter) {
  _clearReadinessEvidence();
  return Status::Error(Err::MEASUREMENT_NOT_READY, "Burst window shifted during read");
}
```

Place it immediately before `_lastBurst = out;` at `:888` so a stale-window rejection
still takes priority. The freshness token is not consumed on this path, so there is no
lockup. The residual hole — a tear *inside* the newest pair — is caught by CRC only and
should be documented rather than closed with a ninth transaction.

---

## 6. Health model classifies by error code instead of by origin — MAJOR

**`src/OPT4001.cpp:2267`, `:2281`, `:2295`.** All three tracked wrappers do:

```cpp
Status st = _i2cWriteReadRaw(...);
if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) { return st; }
return _updateHealth(st);
```

The intent is "validation errors are not transport failures", but the filter runs on
the status the **application's callback** returned. Any transport that maps a platform
error to `INVALID_PARAM`/`INVALID_CONFIG` silently escapes health tracking and the
OFFLINE latch entirely — and the repo's own ESP-IDF adapter does exactly that
(`Opt4001IdfI2cTransport.cpp:27`, `ESP_ERR_INVALID_ARG → INVALID_PARAM`).

**Proposal.** Split by origin, not by code. Hoist the driver-side preflight
(null-callback and null-buffer checks) into `_i2cPreflightWriteRead()` /
`_i2cPreflightWrite()`, run it *before* the callback in the tracked wrappers, and
health-track everything the callback returns. Delete all three code-based blocks.

Then make the in-repo adapters consistent: `INVALID_PARAM`/`INVALID_CONFIG` become
reserved for driver-side validation, so the IDF adapter should map
`ESP_ERR_INVALID_ARG → I2C_BUS` (matching `ESP_ERR_INVALID_STATE` right below it), and
`docs/integration/esp-idf.md` needs the same row updated. Document in `Config.h` that a
transport callback must not return those two codes for bus faults.

Behaviour change worth a CHANGELOG line: an adapter that returns `INVALID_PARAM` for an
unsupported general-call address would now accumulate failures on `softReset()`. With
`offlineThreshold = 5` and a reset on any success, this only latches for a genuinely
broken integration — and the driver already latches today when the same write fails
with `I2C_BUS`.

---

## 7. Smaller core items

| # | Location | Finding | Proposal |
| --- | --- | --- | --- |
| 7a | `src/OPT4001.cpp:1136` | `readFlags()` clears `_conversionStarted`/`_pendingMode` even in CONTINUOUS mode, unlike the guarded copy of the same logic in `_refreshReadinessEvidence()`. Three copies of this block exist and have already drifted. | Extract `_latchReadinessEvidence()` and use it at all three sites (`readFlags()` `:1136`, `_refreshReadinessEvidence()` `:1913`, `_pollReadJob` WAIT_READY `:2515`). Leave the `READ_COUNTER` step alone — routing it through the helper would additionally clear `_conversionStarted` mid-job. |
| 7b | `src/OPT4001.cpp:2385` | `_decodeSampleRegisters()` returns `rawToAdcCodes()`'s `INVALID_PARAM` for a corrupted exponent **before** the CRC check, so a bus error is reported as a parameter error and every fresh-read API bails without consuming freshness. | Keep the numeric block but defer its *return* so `adcCodes`/`lux` are always written and CRC wins the status race. A bare early return would leave the output fields stale, breaking the documented "populated on CRC_ERROR" contract. The `adcCodes > UINT32_MAX` branch is unreachable (20-bit mantissa × exponent ≤ 8 caps at 2^28) — delete it. |
| 7c | `src/OPT4001.cpp:192` | `bind()` clears `hardwareConfigDirty` without touching hardware, so a failed re-`begin()` silently erases divergence evidence. | Delete the clear from `bind()` (keep it in `unbind()`), with a comment: a bus-silent call cannot make hardware match the cache again. Add a regression test. |
| 7d | `include/OPT4001/OPT4001.h:206` | `bind()`/`begin()` do not document that **any** validation failure is destructive: `_config` is reset to defaults before validation runs, so one bad field tears down a working driver and `probe()` then fails with "I2C read callback missing". | Documentation only. `test/test_basic.cpp:698` pins the wipe as intended behaviour, so deferring the commit would regress the suite. Also fix `:214` ("retry `begin()` with the cached config") — after a validation failure there is no cached config left. |
| 7e | `src/OPT4001.cpp:1182` | `readIntPinAsserted()` returns an undocumented `NOT_INITIALIZED` for a call that performs no I2C. | Take the documentation branch, not the "relax to `_bound`" branch: before attach, the cached `INT_POL` has not been written to hardware (reset value is ACTIVE_LOW), so a user who configured ACTIVE_HIGH would get a silently inverted bool instead of an honest error. |
| 7f | `include/OPT4001/OPT4001.h:323` | `getLastBurst()` documents a poll-only cache, but synchronous `readBurst()` also fills it and `startReadSample()` invalidates it without refilling. | Docs only. Keep `_lastBurstValid = false` in `startReadSample()` — it preserves the invariant that the burst cache agrees with the sample cache, which two tests codify. |
| 7g | `include/OPT4001/OPT4001.h:719` | `_pollBurst` is a permanently resident ~96-byte member used only as scratch inside one poll step. | Make it a local inside the `READ_BURST_BLOCK` block (lines `:2580-2595` fill and consume it with no intervening return) and delete the member. |
| 7h | `include/OPT4001/OPT4001.h:577` | `getConversionTimeUs()/Ms()` — the guard added in this pass is defensible, but one reviewer argues persuasively that `return 0` in a *timing* helper fails **open** (`nowMs - start >= 0` always true), so a clamp to the maximum entry would be safer than 0. | Either keep the guard and reword the doc, or clamp to `CONVERSION_TIME_*[11]`. Decide and make the header say what the code does. |
| 7i | `src/OPT4001.cpp:2068` | `getRangeFullScaleLux()` returns the datasheet's *rounded* values (117441.0f), which can be lower than the lux the same exponent actually produces, so an obvious saturation check misses the top of the range. | Either compute from the register format (`(2^20 - 1) << exponent) * luxLsb`) so it is self-consistent with `adcCodesToLux()`, or document that these are nominal values and not exact bounds. |
| 7j | `src/OPT4001.cpp:1760` | `readRegisters()`'s `len <= 2U` short-circuit contradicts the documented unconditional non-burst rule and duplicates the span computation. | Delete the length short-circuit. Optionally fold the span walk into a validating accessor `publicRegisterBlockEnd(startReg, len, uint8_t& endReg)` so the reserved-gap check is not duplicated. |
| 7k | `src/OPT4001.cpp:324` | *Contested (1/3).* `end()` clears the poll-job fields directly instead of going through `cancelPollJob()`, so it bypasses the dirty-marking rule. The reviewers showed the originally-claimed scenario cannot diverge (`startConfigureMeasurement` copies thresholds verbatim), but the code path is real for a cancelled attach or reset. | Low priority: `if (_pollJob != PollJob::NONE) { (void)cancelPollJob(); }` at the top of `end()`, so the dirty rule lives in one place. |

> **Do not** delete `case PollJob::NONE:` from `poll()`'s switch or the post-loop
> `return`. Both are unreachable at runtime but required by `-Wswitch` and
> `-Werror=return-type` respectively; removing them is a hard build error.

---

## 8. Tests and tooling do not cover any of the above

Every finding in this document passes the 133-test suite. Two structural reasons:

- **The fake clock advances 1 ms per yield** (`test/test_basic.cpp:192`), which is what
  hides §1 — iterations and milliseconds coincide only in the fake.
- **`FakeBus` always auto-increments the register pointer**, regardless of the
  `I2C_BURST` bit it stores. Reviewers correctly pushed back on modelling a specific
  burst-disabled behaviour — the datasheet defines none — so the test should assert the
  *driver's* side instead: that it never issues a >2-byte read while hardware burst is
  unproven.

Recommended additions, in priority order:

1. A blocking-read test with a clock that advances **independently of iteration count**
   (e.g. a yield that advances 1 ms every 500 calls), which fails today.
2. A test that clears the hardware `I2C_BURST` bit behind the driver's back and
   asserts the driver stops issuing >2-byte reads (§5a). Do **not** teach `FakeBus` a
   specific burst-off byte pattern — SBOS993A does not define one.
3. A test that drives `poll()` with a caller clock while `Config::nowMs` is null, and
   asserts the conversion gate actually holds.
4. `FakeBus` recording the I2C address of every transaction, asserting device traffic
   uses `cfg.i2cAddress` and that `softReset()` targets `0x00` with payload `0x06`.
5. `FakeBus` clearing FLAGS only on a **non-zero** write, per the datasheet.

Remaining tooling items are in §9 once verification completes.

---

## 9. Examples and integration

| # | Location | Finding | Proposal |
| --- | --- | --- | --- |
| 9a | `examples/01_basic_bringup_cli/main.cpp:1538` | **CRITICAL.** `selfcheck` reads the thresholds into default-constructed locals and only *reports* the status; `getThresholds()` leaves them untouched on failure. A transient NACK therefore makes selfcheck write `0x0000`/`0x0000` back at `:1665` and `:1691`, destroying the configured threshold window — and the run still displays as passing. | Gate the capture: only record `baseLowThreshold`/`baseHighThreshold` and perform the restore when the initial read succeeded. |
| 9b | `examples/01_basic_bringup_cli/main.cpp:2061` | `config write <hex>` treats `Err::IN_PROGRESS` as a failure, but that is `writeConfiguration()`'s documented **success** return when the written MODE selects a one-shot. The write lands, the CLI shows red and skips the readback. | Accept `IN_PROGRESS`, matching how the CLI already treats `startConversion()`. |
| 9c | `examples/esp_idf/basic/main/main.cpp:533`, `:550` | **`pdMS_TO_TICKS(1)` is 0 at the ESP-IDF default `CONFIG_FREERTOS_HZ=100`**, and `vTaskDelay(0)` returns immediately without yielding. Both IDF poll loops become full-speed I2C hammering that starves IDLE. | `delayAtLeastOneTick(ms)` computing `max(1, pdMS_TO_TICKS(ms))`, plus `sdkconfig.defaults` with `CONFIG_FREERTOS_HZ=1000` to restore the ~1 kHz cadence the 2000-iteration cap was sized for. Do not just raise the cap. `pdMS_TO_TICKS(10)` at `:1388` is already fine. |
| 9d | `examples/esp_idf/basic/main/Opt4001IdfI2cTransport.cpp:116` | `opt4001IdfYield()` is `taskYIELD()`, which only reschedules among tasks of equal-or-higher priority — so it never yields to IDLE and every blocking read busy-spins. | `vTaskDelay(1);` (`freertos/task.h` is already included). Note this costs 10 ms per poll at 100 Hz. **The Arduino example has the identical bug** at `main.cpp:374` (`yield()` is `vPortYield()` on arduino-esp32) and the README documents the same snippet. |
| 9e | `examples/esp_idf/basic/main/main.cpp:1336` | The INT pin is never `gpio_config()`'d, so `gpio_get_level()` reads an unconfigured pad. With the default ACTIVE_LOW polarity a constant 0 reads as **permanently asserted** — a permanent false positive into `_intFreshEvidenceAsserted()`. | Configure once in `configureI2c()` after `i2c.intPin = INT_PIN;`, guarded on the compile-time constant: `GPIO_MODE_INPUT` (load-bearing) plus `GPIO_PULLUP_ENABLE` to match the Arduino `INPUT_PULLUP`. The external 10 kΩ pull-up SBOS993A requires is still mandatory. |
| 9f | `examples/esp_idf/basic/main/CMakeLists.txt:6` | `REQUIRES OPT4001` resolves the component by **directory name**, so a checkout named `OPT4001-main` (the GitHub zip default) fails with "Failed to resolve component". ESP-IDF has no supported name override. | Either document it (already added to the README in this pass; add it to `docs/integration/esp-idf.md` too), or make the example independent: drop `EXTRA_COMPONENT_DIRS` and add `examples/esp_idf/basic/components/OPT4001/CMakeLists.txt` registering the repo-root sources by absolute path. |
| 9g | `idf_component.yml:6` | `targets: [esp32s2, esp32s3]` hard-blocks every other ESP32 chip although the core is target-agnostic. | Do **not** just delete the key — that trades an honest block for an untested support claim, and CI only builds S2/S3. Either broaden properly (drop `targets:`, add a non-Xtensa target such as `esp32c3` to the CI matrix, update `AGENTS.md`), or keep the pin and document it where users hit it. |
| 9h | `examples/esp_idf/basic/main/Opt4001IdfI2cTransport.cpp:52` | `validateContext()` returns `I2C_BUS` for an unconfigured adapter handle — a host-side fault attributed to the device, which can latch OFFLINE. | `Err::INVALID_CONFIG`. Note the consequence: health is then never updated, so a permanently null handle leaves the driver reporting READY while every call returns an explicit error. That is the intended contract per `AGENTS.md`. Revisit alongside §6. |

---
| 9i | `examples/01_basic_bringup_cli/main.cpp:1146` | `rebeginWithConfig()` does `device.end()` then `device.begin(cfg)` with **no rollback**. `addr 0x44` on a 0x45-strapped board powers down the working device, fails to attach, and leaves the driver `UNINIT` while still bound to the bad address — so `attach` retries the wrong address. It also skips the `finishWatch`/`finishStress` teardown every other lifecycle command does, leaving an orphaned watch polling an `UNINIT` driver. | Copy the previous config **by value before** `end()` (`getConfig()` returns a reference to `_config`, which `bind()` overwrites — never pass it straight back into `begin()`), and re-`begin(prev)` on failure. A candidate-instance probe is not enough on its own: it cannot cover an `_applyConfig()` write failure inside `begin()`, so the rollback is required regardless. Add the `finishWatch(true)`/`finishStress(true)` calls. |
| 9j | `examples/01_basic_bringup_cli/main.cpp:727` | The `watch` continuous branch **does** have an interval gate, but `lastStepMs` is not updated on the `!ready` path, so between interval expiry and readiness it calls `conversionReady()` — and therefore potentially a clear-on-read FLAGS read — every `loop()` iteration. | Do **not** set `lastStepMs = now` on the not-ready path; that would quantise the sample period to a multiple of the interval. Add a separate `lastPollMs` and a small `WATCH_MIN_POLL_MS` (5–10 ms) gate, so `lastStepMs` keeps driving sample cadence while `lastPollMs` bounds the readiness poll rate. Largely mitigated by the driver-side fix in §3a. |
| 9k | `examples/01_basic_bringup_cli/main.cpp:566` | `watch` flags any counter delta != 1 as a "Counter gap". In continuous mode the device keeps converting between samples, so a delta > 1 is *expected* whenever the interval exceeds the conversion time — every sample of `watch 10 1000` at 100 ms looks broken. | Do not try to compute an expected delta: the counter is mod-16, so once ~16 conversions fit in one interval the value aliases and carries no information (250 ms at `MS_12_7` gives ~19 conversions → delta 3, and 17 conversions would read as delta 1 and silently pass). Only apply the gap test when the host owns the cadence, i.e. in one-shot mode. |
| 9l | `examples/01_basic_bringup_cli/main.cpp:416` | `printStatus()` branches on `st.ok()` only, so `IN_PROGRESS` — the documented success return of all six `start*()` methods — renders in the error style. | Add a `statusColor()` helper mirroring the one the ESP-IDF example already has (`main.cpp:173`): green for ok, yellow for `IN_PROGRESS` / `CRC_ERROR` / `MEASUREMENT_NOT_READY`, red otherwise. The Arduino file's own `sampleStatusWarn()` already classifies `CRC_ERROR` as a warning, so red is wrong there too. |
| 9m | `examples/01_basic_bringup_cli/main.cpp:1894` and `:2394` | `measure` and `job measure` parse the same documented grammar with two different parsers — a tokenizer and an `sscanf` — with different validation. | Extract one `parseMeasureArgs()` and have **both** use the tokenizing form; delete the `sscanf`, which is the weaker parser (decimal-only, silently ignores trailing junk). Return `bool`, not `Status` — these are usage errors, not driver errors, and manufacturing a driver `Status` for them misuses the type. |
| 9n | `examples/01_basic_bringup_cli/main.cpp:2137` | `read N` is a non-cancellable blocking loop (up to 10 000 iterations) that freezes the shell. | Bound it hard (e.g. 1–20) and say so in the help. Note the cancellable equivalent is `watch N 0`, **not** `watch N` — `WATCH_DEFAULT_INTERVAL_MS` is 250 ms, so plain `watch N` is not back-to-back. |
| 9o | `examples/01_basic_bringup_cli/main.cpp:1774` | Help advertises `watch [N] [interval]` as streaming, but bare `watch` is a state inspector. | Fix the help, not the behaviour — bare `watch` matches the adjacent `job` pattern and is the only way to inspect an in-flight session. Split into two help entries. |
| 9p | `examples/common/Log.h:20`, `CliShell.h` | The `LOG_SERIAL` indirection is broken: the shell *reads* through `LOG_SERIAL` but the example initialises and writes to `Serial` directly (191 sites). Repointing it at a second UART yields a shell that reads one port and prints to another. | Delete the indirection rather than complete it — `BoardConfig.h` does the `begin()` and cannot see `Log.h` (it is included nine lines earlier in `main.cpp`), so "use LOG_SERIAL everywhere" does not even compile as stated. Remove the `#define` and the unused `log_begin()`, and use `Serial` at the four remaining sites. |
| 9q | `examples/common/CliStyle.h:19`, `I2cScanner.h:17` | Dead example helpers: `cli::yesNoColor`, `cli::successRateColor`, `i2c_scanner::recoverBus`, `log_begin`. | Delete `recoverBus` and `log_begin` outright. For the two colour helpers the real defect is **triplication** — `main.cpp:194` and `:202` define byte-identical private copies. Delete the copies in `main.cpp` and qualify the ten call sites as `cli::…`, keeping the shared header as the single definition. |

---

## 10. Tooling

| # | Location | Finding | Proposal |
| --- | --- | --- | --- |
| 10a | `tools/hil_opt4001_runner.py:28-32` | The HIL runner's bare status-token FAIL patterns are **case-insensitive**, so ordinary prose in CLI output matches them and a healthy command is reported FAIL. | The `EXPECTED_PATTERNS["cfg"]` entry is fine — do not touch it. Drop `re.IGNORECASE` from the bare-token FAIL patterns (`TIMEOUT`, `NOT_INITIALIZED`, `INVALID_CONFIG`/`INVALID_PARAM`, `OFFLINE`, `BUSY`), or better, anchor them as `Status:\s*…`. The driver only ever emits these names uppercase (`Status.h:42`), and the existing parser tests feed uppercase, so they still pass. Add a regression case. |
| 10b | `tools/check_idf_example_contract.py:213` | The mandatory-command check greps for the command name as a quoted literal *anywhere* in `main.cpp`, so a command that appears only in help text satisfies the parity guarantee. | Reuse the idiom `check_cli_contract.py:143` already has: slice from `void processCommand` and require `strcmp(cmd, "X")`. All 88 current commands already satisfy the tightened rule, so it passes today with no source change. |
| 10c | `test/test_basic.cpp:105`, `:153` | `FakeBus` discards the `addr` argument entirely, so **no test proves the driver talks to the right address** — or that general-call reset goes to `0x00`. Reclassify as *test-coverage*, severity minor: the driver itself is correct at all three general-call sites. | Name the parameter and record it. Assert device traffic uses `cfg.i2cAddress` and that reset uses `0x00`/`0x06` — covering all three general-call sites including the async `_pollResetAndReapplyJob` (`src/OPT4001.cpp:2686`), which the original proposal missed. |
| 10d | `test/test_basic.cpp:145` | `FakeBus` wipes the whole FLAGS register on a **zero** write, although the datasheet clears only on non-zero — and its own guard three lines earlier already encodes that. | Delete the fallback `else if (reg == cmd::REG_FLAGS) { registers[FLAGS] = value; }` branch so a zero write is a no-op; lines 127-129 already suppress the generic store. Do not instead mask bits 15:4 — those are R-only and must be written 0 anyway. |
| 10e | `test/test_basic.cpp` | Untested public API. (`poll()` budget burn is **not** in this list — it is covered at `:1847`, `:1191`, `:1766`, `:1822`, `:1985`, `:2018`.) | Add tests for `writeConfiguration()` (the one-shot `IN_PROGRESS` return at `src/OPT4001.cpp:1695`, plus the reserved-bit and invalid-field rejections), `readFlagsRaw()` (its readiness-evidence side effect — the inverse of `readFlags()`), `readBlockingLux()`'s `CRC_ERROR` passthrough, `isOnline()`, `setInterruptLatch()`, and `Flags::overload`. |

---

## Raised but refuted

Recorded so they are not re-raised. Each was rejected by a majority of three
independent reviewers.

| Claim | Why it does not hold |
| --- | --- |
| `pkg pico` can never succeed | The handler does not call `setPackageVariant()` at all — it rebuilds the config and goes through `rebeginWithConfig()`. `setPackageVariant` has no caller anywhere in the repo. |
| `parseU32` accepts negative input | Trailing-garbage rejection already exists (`*end != '\0'`), and the `watch` count never goes through `parseU32`. |
| HIL runner drops commands on `--overall-timeout` | Line 503 appends a `FAIL` result *before* the `break`, so a truncated run cannot look like a complete PASS. |
| `FakeBus` cannot model `I2C_BURST=0` | True that it always auto-increments, but SBOS993A defines no burst-off behaviour for an over-long read, so there is nothing correct to model. Test the driver's side instead (§8). |
| A `check_idf_example_contract.py` regex can never match | It does match well-formed C++ via the comma operator. |
| `end()` bypasses the dirty rule (1/3) | The specific scenario cannot diverge — `startConfigureMeasurement()` copies the thresholds verbatim, so the first writes are byte-identical. The code path is still asymmetric; kept as §7k at low priority. |
| Two `_shouldProbeCounterForFreshness` overloads have drifted (0/2, 1/3) | They do differ textually, but each has exactly one caller and every difference is unreachable as a behavioural difference. The duplication is still worth removing as part of §2. |
| `poll()` has unreachable dead code | `case PollJob::NONE:` is required by `-Wswitch` and the post-loop `return` by `-Werror=return-type`. Deleting either is a build error. |
| `FakeBus` one-shot budget diverges from the driver (1/3) | The arithmetic difference is real but never changes which millisecond the fake becomes ready. |
| Dirty-cache reason never surfaced (1/3) | Partially surfaced already; low value. |
