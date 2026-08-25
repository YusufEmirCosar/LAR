#!/usr/bin/env python3
"""Require meaningful file-level Doxygen documentation in production headers."""

from __future__ import annotations

import re
import sys
from pathlib import Path


FILE_BLOCK = re.compile(
    r"/\*\*(?:(?!\*/).)*@file\b(?:(?!\*/).)*@brief\b(?:(?!\*/).)*\*/",
    re.DOTALL,
)


def production_headers(root: Path) -> list[Path]:
    """Return maintained API headers, excluding tests and generated trees."""
    headers = list((root / "src").rglob("*.h"))
    headers.extend((root / "tools" / "map_asset").rglob("*.h"))
    return sorted(headers)


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    headers = production_headers(root)
    failures: list[str] = []

    for header in headers:
        text = header.read_text(encoding="utf-8")
        if not FILE_BLOCK.search(text):
            failures.append(
                f"{header.relative_to(root)} lacks one /** @file ... @brief ... */ block"
            )

    if failures:
        print("Doxygen coverage gate failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print(f"Doxygen coverage gate passed ({len(headers)} production headers)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
