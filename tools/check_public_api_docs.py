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
        "Health tracking counts tracked transport outcomes",
        "device-identity mismatch observed by `recover()`",
        "begin()",
        "bind(const Config& config)",
        "without touching I2C",
        "startAttach()",
        "at most one transport",
        "unbind()",
        "powerDown()",
        "cancelPollJob()",
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
        "driverStateName",
        "driverState()",
        "isOnline()",
        "lastOkMs()",
        "lastErrorMs()",
        "lastError()",
        "consecutiveFailures()",
        "totalFailures()",
        "totalSuccess()",
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
        "errorName",
        "toString",
    ],
}

FORBIDDEN_TOKENS = {
    "include/OPT4001/OPT4001.h": ["UNKNOWN_STATE"],
    "include/OPT4001/Status.h": ["UNKNOWN_ERROR"],
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
        for token in FORBIDDEN_TOKENS.get(rel, []):
            if token in text:
                fail(f"{rel} restored obsolete enum fallback: {token}")

    print("Public API docs check PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
