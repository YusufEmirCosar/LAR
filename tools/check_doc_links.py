#!/usr/bin/env python3
"""Validate repository-local links in Markdown files."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from urllib.parse import unquote


LINK = re.compile(r"!?\[[^\]]*\]\((?:<([^>]+)>|([^\s)]+))(?:\s+['\"][^'\"]*['\"])?\)")


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    markdown = [*root.glob("*.md"), *(root / "docs").rglob("*.md")]
    failures: list[str] = []
    for document in markdown:
        text = document.read_text(encoding="utf-8")
        for match in LINK.finditer(text):
            raw = match.group(1) or match.group(2)
            if raw.startswith(("http://", "https://", "mailto:", "#")):
                continue
            path_part = unquote(raw.split("#", 1)[0])
            if not path_part:
                continue
            target = (document.parent / path_part).resolve()
            try:
                target.relative_to(root)
            except ValueError:
                failures.append(
                    f"{document.relative_to(root)}: link escapes repository: {raw}"
                )
                continue
            if not target.exists():
                failures.append(
                    f"{document.relative_to(root)}: missing link target: {raw}"
                )
    if failures:
        print("Documentation link gate failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"Documentation link gate passed ({len(markdown)} files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
