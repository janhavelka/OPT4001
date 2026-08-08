#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys
from typing import Dict

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCAN_DIRS = ("src", "include")
VALID_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}

FORBIDDEN_CALLS = {
    "millis": re.compile(r"\bmillis\s*\("),
    "micros": re.compile(r"\bmicros\s*\("),
    "delay": re.compile(r"\bdelay\s*\("),
    "delayMicroseconds": re.compile(r"\bdelayMicroseconds\s*\("),
    "yield": re.compile(r"\byield\s*\("),
}

FORBIDDEN_IDENTIFIERS = {
    "String": re.compile(r"\bString\b"),
    "Serial": re.compile(r"\bSerial\b"),
    "Wire": re.compile(r"\bWire\b"),
}

FORBIDDEN_INCLUDE_RES = {
    "Arduino.h": re.compile(r'^\s*#\s*include\s*[<\"]Arduino\.h[>\"]', re.MULTILINE),
    "Wire.h": re.compile(r'^\s*#\s*include\s*[<\"]Wire\.h[>\"]', re.MULTILINE),
    "driver/i2c.h": re.compile(r'^\s*#\s*include\s*[<\"]driver/i2c\.h[>\"]', re.MULTILINE),
    "driver/i2c_master.h": re.compile(r'^\s*#\s*include\s*[<\"]driver/i2c_master\.h[>\"]', re.MULTILINE),
    "driver/gpio.h": re.compile(r'^\s*#\s*include\s*[<\"]driver/gpio\.h[>\"]', re.MULTILINE),
    "esp_err.h": re.compile(r'^\s*#\s*include\s*[<\"]esp_err\.h[>\"]', re.MULTILINE),
    "esp_timer.h": re.compile(r'^\s*#\s*include\s*[<\"]esp_timer\.h[>\"]', re.MULTILINE),
    "freertos": re.compile(r'^\s*#\s*include\s*[<\"]freertos/', re.MULTILINE),
}

OBSOLETE_CORE_SYMBOLS = (
    "_i2cWriteRawTo",
    "_i2cWriteTrackedTo",
    "_markConversionReadyByRegisterPoll",
)

BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT_RE = re.compile(r"//[^\n]*")
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')


def strip_non_code(text: str) -> str:
    text = BLOCK_COMMENT_RE.sub("", text)
    text = LINE_COMMENT_RE.sub("", text)
    return STRING_RE.sub('""', text)


def collect_sources() -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for dirname in SCAN_DIRS:
        root = ROOT / dirname
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in VALID_SUFFIXES:
                files.append(path)
    return files


def main() -> int:
    observed_calls: Dict[str, Dict[str, int]] = {}
    observed_identifiers: Dict[str, Dict[str, int]] = {}
    observed_includes: Dict[str, Dict[str, int]] = {}

    for path in collect_sources():
        rel = path.relative_to(ROOT).as_posix()
        raw = path.read_text(encoding="utf-8", errors="replace")
        code = strip_non_code(raw)

        call_counts: Dict[str, int] = {}
        for call_name, pattern in FORBIDDEN_CALLS.items():
            count = len(pattern.findall(code))
            if count > 0:
                call_counts[call_name] = count
        if call_counts:
            observed_calls[rel] = call_counts

        identifier_counts: Dict[str, int] = {}
        for identifier, pattern in FORBIDDEN_IDENTIFIERS.items():
            count = len(pattern.findall(code))
            if count > 0:
                identifier_counts[identifier] = count
        if identifier_counts:
            observed_identifiers[rel] = identifier_counts

        include_counts: Dict[str, int] = {}
        for include_name, pattern in FORBIDDEN_INCLUDE_RES.items():
            count = len(pattern.findall(raw))
            if count > 0:
                include_counts[include_name] = count
        if include_counts:
            observed_includes[rel] = include_counts

    errors: list[str] = []

    for rel, counts in observed_calls.items():
        errors.append(f"forbidden platform/timing calls in core: {rel} -> {counts}")

    for rel, counts in observed_identifiers.items():
        errors.append(f"forbidden Arduino identifiers in core: {rel} -> {counts}")

    for rel, counts in observed_includes.items():
        errors.append(f"forbidden framework headers in core: {rel} -> {counts}")

    for symbol in OBSOLETE_CORE_SYMBOLS:
        for path in collect_sources():
            text = path.read_text(encoding="utf-8", errors="replace")
            if re.search(rf"\b{re.escape(symbol)}\b", text):
                rel = path.relative_to(ROOT).as_posix()
                errors.append(f"obsolete core helper in {rel}: {symbol}")

    if errors:
        print("Core framework guard FAILED:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("Core framework guard PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

