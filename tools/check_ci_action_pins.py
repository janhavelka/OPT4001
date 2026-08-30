#!/usr/bin/env python3
"""Require every GitHub Action in CI to be pinned to a full commit SHA.

A tag or branch ref is mutable, so a compromised or retagged action would run
with this repository's credentials. Only a 40-hex commit SHA is immutable.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
WORKFLOW_DIR = ROOT / ".github" / "workflows"
USES_RE = re.compile(r"uses:\s+([^\s@]+)@([^\s#]+)")
SHA_RE = re.compile(r"[0-9a-f]{40}")


def main() -> int:
    workflows = sorted(WORKFLOW_DIR.glob("*.yml")) + sorted(WORKFLOW_DIR.glob("*.yaml"))
    if not workflows:
        print(f"CI action pin check FAILED: no workflows found in {WORKFLOW_DIR}")
        return 1

    unpinned: list[str] = []
    for workflow in workflows:
        rel = workflow.relative_to(ROOT).as_posix()
        text = workflow.read_text(encoding="utf-8")
        for action, ref in USES_RE.findall(text):
            if SHA_RE.fullmatch(ref) is None:
                unpinned.append(f"{rel}: {action}@{ref}")

    if unpinned:
        print("CI action pin check FAILED: not pinned to a full commit SHA:")
        for entry in unpinned:
            print(f"- {entry}")
        return 1

    print(f"CI action pin check PASSED ({len(workflows)} workflow(s))")
    return 0


if __name__ == "__main__":
    sys.exit(main())
