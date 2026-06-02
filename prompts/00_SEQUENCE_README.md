# OPT4001 Tailored Industry-Readiness Fix Prompt Sequence

These prompts are tailored directly from the `OPT4001 Industry-Readiness Exploration Report`.

This is not a generic I2C hardening prompt. The sequence exists to fix the concrete findings from the report:

- H1: `Version.h` clean checkout/manual/ESP-IDF reproducibility blocker.
- H2: public raw register APIs touching I2C while uninitialized.
- H3: continuous/readiness state over-reporting stale or premature samples.
- H4: weak probe/device-ID validation and collapsed transport diagnostics.
- H5: production/validation claims exceeding evidence.
- M1/M2/etc.: threshold/raw-lux overflow, missing independent vectors, FIFO CRC semantics, partial hardware/cache dirty state, weak ESP-IDF validation, IDF CLI/tick blocking, loose include boundaries, missing Doxygen public contracts, stale metadata, missing hardware/optical validation.

Use prompts in this order:

1. `01_branch_baseline_agents_and_plan.md`
2. `02_fix_version_header_and_build_reproducibility.md`
3. `03_fix_public_lifecycle_and_object_contracts.md`
4. `04_fix_probe_device_id_and_transport_diagnostics.md`
5. `05_fix_freshness_readiness_and_blocking_semantics.md`
6. `06_fix_numeric_threshold_crc_and_vectors.md`
7. `07_fix_fifo_burst_int_flags_and_partial_state.md`
8. `08_fix_esp_idf_example_ci_and_docs_honesty.md`
9. `09_hardware_hil_validation_and_final_report.md`

The coding agent must complete exactly one prompt per pass. After each prompt it must:

- spawn focused subagents,
- implement only that chunk,
- run the requested checks,
- update a chunk report under `docs/`,
- commit,
- sync/push if a remote is configured,
- stop and report exact results.

The final deliverable is:

```text
docs/OPT4001_HARDENING_FINAL_REPORT.md
```

Do not claim industry-grade, hardware-validated, optical-validated, or pure ESP-IDF-validated status unless the evidence was actually produced and logged.
