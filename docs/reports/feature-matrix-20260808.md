# OPT4001 SBOS993A Feature-Completeness Matrix

Audit date: 2026-08-08  
Library version: 1.2.0  
Primary specification: Texas Instruments OPT4001 datasheet SBOS993A,
December 2021, revised December 2022 ([TI PDF](https://www.ti.com/lit/ds/symlink/opt4001.pdf)).  
Local source asset: `docs/reference/OPT4001_datasheet.pdf`, SHA-256
`A12A63EBF414564871FEA1427CB767FCA0EEFB14DB55B7365AAB61CD21A99D6F`.

This matrix records source-level support and executable fake-transport/static
checks. It is not evidence of real-device, optical, FIFO-timing, address-strap,
INT, or injected-fault validation.

## Device Capability Matrix

| SBOS993A capability / register | Core API and behavior | Arduino CLI | Native ESP-IDF CLI | Executable/source evidence |
| --- | --- | --- | --- | --- |
| Package/address matrix: PicoStar fixed `0x45`; SOT-5X3 `0x44`, `0x45`, `0x46` | `PackageVariant`, command-table address constants, begin/bind validation, package lux LSB | `pkg`, `addr`, `scale`, `discover` | Same | Native address/package matrix and PicoStar INT rejection tests |
| Protocol-qualified discovery | Bus-silent `bind()` plus raw, health-neutral `probe()` validates full DEVICE_ID pattern | `discover` probes the three legal addresses; `scan` remains ACK-only | Same; temporary IDF handles are removed | CLI contract requires bind/probe/DEVICE_ID path and IDF handle cleanup |
| RESULT `0x00` + RESULT_LSB_CRC `0x01` | Coherent burst read, exponent/mantissa/ADC/counter/CRC/lux decode; latest versus fresh APIs | `raw`, `read`, `tryread`, `trylux`, `lux`, `mlux`, `ulux`, `job sample` | Same | Independent datasheet CRC vectors, freshness/counter, CRC warning, overflow tests |
| FIFO0 `0x02/0x03`, FIFO1 `0x04/0x05`, FIFO2 `0x06/0x07` | `readBurst()`, `startReadBurst()`, `getLastBurst()`, `readSampleSlot(1..3)` | `readburst`, `slot`, `job burst`, `job result` | Same (`fifo` alias retained) | Four-slot decode, CRC aggregation, ordering, burst/non-burst tests |
| THRESHOLD_L `0x08`, THRESHOLD_H `0x09` | Raw/typed/lux threshold read/write, default restore, 64-bit encode/decode/order checks | `threshold`, `threshold raw/default`, `thcalc`, `thdecode`, `int th` | Same | Quantization, range, saturation, ordering, partial-write dirty tests |
| CONFIGURATION `0x0A`: QWAKE | Typed config/setter and decoded/raw register APIs | `qwake`, `config`, `config write`, `snapshot` | Same (`quickwake` alias) | Register encode/decode and cache rollback tests |
| CONFIGURATION `0x0A`: RANGE 0..8/AUTO | All specified ranges and package-specific scale/resolution | `range`, `measure`, `scale`, `job measure` | Same | Full range vector and invalid-value tests |
| CONFIGURATION `0x0A`: conversion time 600 us..800 ms | All 12 enum values, timing/effective-bit helpers | `ctime`, `timing`, `measure`, `job measure` | Same | Full conversion-time vector and deadline tests |
| CONFIGURATION `0x0A`: modes | Power-down, forced-auto one-shot, history one-shot, continuous | `mode`, `start [force]`, `read [force]`, `powerdown` | Same | Mode/register, full forced-auto budget, wraparound and timeout tests |
| CONFIGURATION `0x0A`: latch, polarity, fault count | Typed setters and decoded readback | `int latch/pol/faults` | Same plus compatibility aliases | Packing/validation/cache rollback tests |
| INT_CONFIGURATION `0x0B`: fixed pattern/reserved bits | Validated raw/decoded read/write | `intcfg`, `intcfg write` | Same | Fixed-pattern rejection and decode tests |
| INT direction input/output and functions threshold/every-conversion/FIFO-full | Typed setters and presets; PicoStar output rejection | `int dir/cfg/ready/fifo/th`, `intpin` | Same | Package validation and register-packing tests; physical pin timing remains pending |
| I2C burst enable | Typed setter plus coherent burst and bounded per-register fallback | `burst` | Same | Burst/non-burst transaction-count and register-window tests |
| FLAGS `0x0C`: overload, ready, high/low window | Decoded/raw reads, documented clear-on-read, ready-only write-clear | `status`, `flags`, `status_raw`, `flags_raw`, clear commands | Same | Clear behavior, cache freshness synchronization, side-effect tests |
| DEVICE_ID `0x11` | Full fixed-pattern validation, decoded/raw read, health-neutral probe | `id`, `identify`, `probe`, `discover` | Same | Valid/mismatch/reserved-high-bit and transport-detail tests |
| General-call reset address `0x00`, byte `0x06` | Explicit synchronous reset/reapply and poll-chunked reset/reapply | `reset`, `resetreapply`, `job reset confirm` | Same | Success, failure, OFFLINE, partial-apply dirty, cancellation tests |
| INT input hardware one-shot trigger | Core exposes direction/configuration but does not own GPIO waveform generation | `int dir in`; help/docs identify application ownership | Same | Source contract only; waveform/HIL pending |
| SMBus alert response and HS-mode entry master code | Deliberately transport/controller-owned; no device-register API is invented | Documented, no misleading device command | Same | Boundary review against SBOS993A; controller-specific validation pending |
| Persistence/NVM | Device has no user persistence/NVM capability | Not advertised | Not advertised | Datasheet register/capability review |

## Managed Driver / External Owner Matrix

| Integration capability | 1.2.0 contract | Evidence |
| --- | --- | --- |
| Bus/task ownership | Core owns no bus, pins, task, lock, queue, retry loop, or framework object; callbacks/context are injected synchronously | Core timing/framework guard; strict `native_core_no_arduino` compile/link; public Config documentation |
| Caller serialization | Instance is not thread-safe or ISR-safe; callbacks must not re-enter; sole owner task provides serialization | Public class and callback contracts |
| Bus-silent setup/release | `bind()` validates/caches with zero callbacks; `unbind()` releases with zero callbacks | Exact callback-count native tests |
| Cooperative attach | `startAttach()` performs one DEVICE_ID read and four config writes; `poll(..., 1)` invokes at most one transport callback per owner poll | Exact five-phase and zero-budget native tests |
| Cooperative jobs | Sample, burst, configuration, and reset/reapply jobs use explicit instruction budgets and cached results | Poll budget, gate, failure-stop, result tests; both CLI `job` families |
| Cancellation | `cancelPollJob()` is I2C-silent and idempotent while idle; partial config/reset writes mark cache/hardware dirty | Cancellation callback-count and dirty-provenance tests |
| Legacy shutdown compatibility | `end()` retains its documented best-effort raw power-down attempt | Regression verifies one write and ignored failure semantics |
| Error-honest power down | `powerDown()` returns transport status and updates cached mode only after success | Success/failure and cache tests |
| Health/recovery | Tracked I2C only after initialization, saturating counters, manual recovery, OFFLINE latch | Health transition, counter, recovery and no-health probe tests |

## CLI / Diagnostic Matrix

- Both CLIs expose the same owner lifecycle, qualified discovery, full poll-job
  family, typed configuration, FIFO/FLAGS/threshold/interrupt/raw diagnostics,
  health monitor, dynamic ANSI color, finite cooperative stress/mixed-stress,
  and bounded selfcheck aliases.
- Both use `examples/common/CliLineBuffer.h`: fixed 192-byte storage, no heap,
  complete overlong-line discard through CR/LF, and deterministic recovery.
- Arduino command parsing uses fixed-capacity `CliText`; native IDF uses fixed C
  buffers and `strtok_r`. Neither example uses a dynamic command registry.
- `stress` and `stress_mix` are finite owner-loop sessions. Measurement work is
  split through `poll(..., 1)` and all other mixed operations perform at most
  one transport call per service iteration.
- `job poll` defaults to the owner-fair budget of one. Values above one are
  explicitly diagnostic batching, not the recommended sole-owner integration
  cadence.

## Confirmed Audit Fixes

1. Added bus-silent external-owner binding/release and instruction-budgeted
   attachment rather than changing the established `end()` contract.
2. Added error-honest power-down and poll-job cancellation with partial-write
   provenance.
3. Replaced ACK-only discovery as device identification with a separate
   DEVICE_ID-qualified `discover` command while retaining truthful `scan`.
4. Exposed the complete poll-job API in both CLIs.
5. Replaced truncating/dynamic CLI input with a shared bounded discard parser,
   added runtime color parity and native-IDF health monitoring.
6. Replaced exposed blocking stress loops with cooperative finite sessions and
   made selfcheck wording explicit about hardware side effects.

No new physical-validation claim is made by this audit.
