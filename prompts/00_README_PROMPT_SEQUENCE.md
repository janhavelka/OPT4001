# OPT4001 Industry-Readiness Hardening — Prompt Sequence README

Use these prompts one by one, in order. Do not skip ahead unless the current prompt is fully completed, committed, pushed/synced, and reported.

The coding agent must understand that this is a staged hardening workflow:

1. Establish branch, baseline, AGENTS.md, and a work plan.
2. Fix clean checkout / manual / ESP-IDF build reproducibility first.
3. Fix lifecycle and probe/ID diagnostics.
4. Fix measurement freshness and continuous/one-shot readiness semantics.
5. Fix numeric/FIFO/partial-state issues and strengthen tests.
6. Fix documentation honesty, examples, CI, and public API contracts.
7. Run hardware-validation preparation and produce the final comprehensive report.

The coder may spawn subagents at each step. It must commit and sync after each prompt. It must not claim hardware or pure ESP-IDF validation unless actually executed and logged.

Suggested branch:

```bash
git checkout -b hardening/opt4001-industry-readiness
```

If the branch already exists, continue on it only if it is clearly the intended branch.

The final target report is:

```text
docs/OPT4001_HARDENING_FINAL_REPORT.md
```
