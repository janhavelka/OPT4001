#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

HEADER_TOKENS = {
    "include/OPT4001/OPT4001.h": [
        "not internally synchronized",
        "not ISR-safe",
        "Transport callbacks must not",
        "NOT_INITIALIZED",
        "OFFLINE",
        "Health tracking counts only tracked transport outcomes",
        "begin()",
        "Config application writes multiple registers",
        "may perform tracked I2C readiness polling",
        "probe()",
        "without updating health",
        "recover()",
        "clears dirty config state",
        "readLatestSample",
        "Freshness requires hardware evidence",
        "readBurst",
        "all four slots are decoded and populated",
        "CRC_ERROR",
        "hardwareConfigDirty",
        "Config::nowMs",
        "Config::cooperativeYield",
        "clear-on-read",
        "SOT_5X3-only signal",
        "bus-wide",
        "low <= high",
        "sampleCounterDelta",
    ],
    "include/OPT4001/Config.h": [
        "must honor",
        "timeoutMs",
        "must not",
        "re-enter",
        "application owns GPIO",
        "public driver APIs are not ISR-safe",
        "1.6 V to",
        "5.5 V tolerant",
        "driver never configures",
        "application lock",
        "offlineThreshold",
    ],
    "include/OPT4001/Status.h": [
        "MEASUREMENT_NOT_READY",
        "CRC_ERROR",
        "I2C_TIMEOUT",
        "BUSY",
    ],
}


def fail(msg: str) -> None:
    print(f"Public API docs check FAILED: {msg}")
    raise SystemExit(1)


def main() -> int:
    for rel, tokens in HEADER_TOKENS.items():
        path = ROOT / rel
        if not path.exists():
            fail(f"missing public header: {rel}")
        text = path.read_text(encoding="utf-8", errors="replace")
        for token in tokens:
            if token not in text:
                fail(f"{rel} missing documentation token: {token}")

    print("Public API docs check PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
