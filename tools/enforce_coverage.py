#!/usr/bin/env python3
"""Enforce project and core-layer thresholds from gcovr JSON summary."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def ratio(covered: int, total: int) -> float:
    return 100.0 if total == 0 else 100.0 * covered / total


def aggregate(files: list[dict], prefixes: tuple[str, ...]) -> tuple[float, float]:
    chosen = [f for f in files if f.get("filename", "").startswith(prefixes)]
    line_total = sum(f.get("line_total", 0) for f in chosen)
    line_covered = sum(f.get("line_covered", 0) for f in chosen)
    branch_total = sum(f.get("branch_total", 0) for f in chosen)
    branch_covered = sum(f.get("branch_covered", 0) for f in chosen)
    return ratio(line_covered, line_total), ratio(branch_covered, branch_total)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: enforce_coverage.py GCOVR_SUMMARY.json", file=sys.stderr)
        return 2
    data = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    files = data.get("files", [])
    overall = aggregate(files, ("src/",))
    core = aggregate(files, ("src/domain/", "src/application/"))
    checks = (
        ("overall line", overall[0], 90.0),
        ("overall branch", overall[1], 80.0),
        ("domain/application line", core[0], 95.0),
        ("domain/application branch", core[1], 90.0),
    )
    failed = False
    for label, actual, minimum in checks:
        print(f"{label}: {actual:.2f}% (required {minimum:.2f}%)")
        failed = failed or actual + 1e-9 < minimum
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
