#!/usr/bin/env python3
from __future__ import annotations

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

CLAIM_SURFACES = [
    "README.md",
    "library.json",
    "idf_component.yml",
    "SECURITY.md",
    "CHANGELOG.md",
    "docs/IDF_PORT.md",
    "docs/IDF_PORT_IMPLEMENTATION.md",
    "docs/OPT4001_datasheet.md",
]

BANNED_PHRASES = [
    "production-grade",
    "production ready",
    "production-ready",
    "industry-grade",
    "hardware validated",
    "optical validated",
    "esp-idf ready",
]

README_REQUIRED = [
    "## Current Readiness",
    "Classification: source-level hardened and diagnostic-build tested.",
    "### Validation Evidence",
    "### Pending Validation Matrix",
    "### Package, Address, And Electrical Matrix",
    "Hardware validation procedure: `docs/OPT4001_HARDWARE_VALIDATION_PROCEDURE.md`.",
    "Pure ESP-IDF builds | CI job configured",
    "5.5 V tolerant",
    "readLatestSample()",
    "readBurst()",
    "hardware/cache state",
]

CI_REQUIRED = [
    "python tools/check_core_timing_guard.py",
    "python tools/check_cli_contract.py",
    "python tools/check_idf_example_contract.py",
    "python tools/check_version_header_contract.py",
    "python tools/check_clean_consumer_package.py",
    "python tools/check_readiness_claims.py",
    "python tools/check_public_api_docs.py",
    "python scripts/generate_version.py check",
    "python -m platformio test -e native",
    "python -m platformio run -e ${{ matrix.environment }}",
    "python -m platformio pkg pack",
    "espressif/esp-idf-ci-action@v1",
    "idf.py set-target ${{ matrix.target }} build",
]


def fail(msg: str) -> None:
    print(f"Readiness claims check FAILED: {msg}")
    raise SystemExit(1)


def read_rel(path: str) -> str:
    full = ROOT / path
    if not full.exists():
        fail(f"missing required file: {path}")
    return full.read_text(encoding="utf-8", errors="replace")


def main() -> int:
    for rel in CLAIM_SURFACES:
        text = read_rel(rel)
        lowered = text.lower()
        for phrase in BANNED_PHRASES:
            if phrase in lowered:
                fail(f"unsupported claim '{phrase}' found in {rel}")

    readme = read_rel("README.md")
    for token in README_REQUIRED:
        if token not in readme:
            fail(f"README missing readiness contract token: {token}")

    metadata = json.loads(read_rel("library.json"))
    if metadata.get("version") != "1.0.0":
        fail("library.json version is expected to be 1.0.0 for this hardening line")
    if "production-grade" in metadata.get("description", "").lower():
        fail("library.json description overclaims production-grade readiness")

    idf_component = read_rel("idf_component.yml")
    if re.search(r'^version:\s*"1\.0\.0"\s*$', idf_component, re.MULTILINE) is None:
        fail("idf_component.yml version must match 1.0.0")
    if "production-grade" in idf_component.lower():
        fail("idf_component.yml description overclaims production-grade readiness")

    security = read_rel("SECURITY.md")
    if "1.0.x" not in security or "0.3.x" in security:
        fail("SECURITY.md supported versions are stale")

    ci = read_rel(".github/workflows/ci.yml")
    for token in CI_REQUIRED:
        if token not in ci:
            fail(f"CI missing required command/token: {token}")

    print("Readiness claims check PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
