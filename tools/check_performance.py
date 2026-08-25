#!/usr/bin/env python3
"""Reject benchmark median/p95 regressions above a configured percentage."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def load(path: str) -> dict:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print(
            "usage: check_performance.py BASELINE.json CURRENT.json [MAX_PERCENT]",
            file=sys.stderr,
        )
        return 2
    baseline = load(sys.argv[1])
    current = load(sys.argv[2])
    maximum = float(sys.argv[3]) if len(sys.argv) == 4 else 10.0
    factor = 1.0 + maximum / 100.0
    failed = False
    for name, expected in baseline.get("metrics", {}).items():
        actual = current.get("metrics", {}).get(name)
        if not isinstance(actual, dict):
            print(f"{name}: missing from current report", file=sys.stderr)
            failed = True
            continue
        if actual.get("unit") != expected.get("unit"):
            print(f"{name}: unit changed", file=sys.stderr)
            failed = True
            continue
        for statistic in ("median", "p95"):
            expected_value = float(expected[statistic])
            actual_value = float(actual[statistic])
            limit = expected_value * factor
            print(
                f"{name} {statistic}: {actual_value:.3f} "
                f"(baseline {expected_value:.3f}, limit {limit:.3f})"
            )
            failed = failed or actual_value > limit
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
