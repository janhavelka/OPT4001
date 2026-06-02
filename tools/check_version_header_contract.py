#!/usr/bin/env python3
"""Validate the tracked OPT4001 version-header contract."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PUBLIC_HEADER = ROOT / "include" / "OPT4001" / "OPT4001.h"
VERSION_HEADER = ROOT / "include" / "OPT4001" / "Version.h"
LIBRARY_JSON = ROOT / "library.json"
GENERATOR = ROOT / "scripts" / "generate_version.py"
REL_VERSION_HEADER = "include/OPT4001/Version.h"


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def fail(message: str) -> int:
    print(f"Version header contract FAILED: {message}", file=sys.stderr)
    return 1


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def library_version() -> str:
    with LIBRARY_JSON.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    version = str(data.get("version", ""))
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError(f"library.json version is not SemVer MAJOR.MINOR.PATCH: {version!r}")
    return version


def parse_define(text: str, name: str) -> str | None:
    match = re.search(rf'^\s*#define\s+{re.escape(name)}\s+"([^"]+)"\s*$', text, re.MULTILINE)
    return match.group(1) if match else None


def parse_const_int(text: str, name: str) -> int | None:
    match = re.search(
        rf"^\s*static\s+constexpr\s+(?:uint16_t|uint32_t|int)\s+{re.escape(name)}\s*=\s*(\d+)\s*;",
        text,
        re.MULTILINE,
    )
    return int(match.group(1)) if match else None


def main() -> int:
    if not PUBLIC_HEADER.exists():
        return fail(f"missing public header: {PUBLIC_HEADER}")
    if not VERSION_HEADER.exists():
        return fail(f"missing version header: {VERSION_HEADER}")

    public_text = read_text(PUBLIC_HEADER)
    if '#include "OPT4001/Version.h"' not in public_text:
        return fail(f"{PUBLIC_HEADER} does not include OPT4001/Version.h")

    ignored = run(["git", "check-ignore", "-q", "--", REL_VERSION_HEADER])
    if ignored.returncode == 0:
        return fail(f"{REL_VERSION_HEADER} is still ignored by git")
    if ignored.returncode not in (0, 1):
        return fail(f"git check-ignore failed: {ignored.stderr.strip()}")

    tracked = run(["git", "ls-files", "--error-unmatch", REL_VERSION_HEADER])
    if tracked.returncode != 0:
        return fail(f"{REL_VERSION_HEADER} is not tracked in the git index")

    version = library_version()
    major, minor, patch = (int(part) for part in version.split("."))
    version_code = major * 10000 + minor * 100 + patch
    version_text = read_text(VERSION_HEADER)

    if parse_define(version_text, "OPT4001_VERSION_STRING") != version:
        return fail("OPT4001_VERSION_STRING does not match library.json")
    expected_ints = {
        "VERSION_MAJOR": major,
        "VERSION_MINOR": minor,
        "VERSION_PATCH": patch,
        "VERSION_CODE": version_code,
        "VERSION_INT": version_code,
    }
    for name, expected in expected_ints.items():
        actual = parse_const_int(version_text, name)
        if actual != expected:
            return fail(f"{name} is {actual!r}, expected {expected}")

    generator_check = run([sys.executable, str(GENERATOR), "check"])
    if generator_check.returncode != 0:
        output = (generator_check.stdout + generator_check.stderr).strip()
        return fail(f"scripts/generate_version.py check failed: {output}")

    print("Version header contract PASSED")
    if generator_check.stdout.strip():
        print(generator_check.stdout.strip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
