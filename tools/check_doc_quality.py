#!/usr/bin/env python3
"""Reject generated comment filler and require security-critical semantic contracts."""

from __future__ import annotations

import re
import sys
from pathlib import Path


DOXYGEN_BLOCK = re.compile(r"/\*\*(?:(?!\*/).)*\*/", re.DOTALL)
BOILERPLATE = (
    re.compile(r"@brief\s+Constructs this object\."),
    re.compile(r"@brief\s+Returns the current\b"),
    re.compile(r"@brief\s+Performs the\b"),
    re.compile(r"@brief\s+Reports whether\b.+\bis true\."),
    re.compile(r"@brief\s+Notifies observers about\b"),
    re.compile(r"@brief\s+Documents the\b"),
    re.compile(
        r"@brief\s+(?:Stops|Resets|Draws|Initializes|Cleanups|Clears|Starts|"
        r"Saves|Renders|Plays|Pauses|Closes|Controls|Updates) (?:the current|the shutdown) "
        r"operation\."
    ),
    re.compile(r"@brief\s+Returns the state changed\."),
    re.compile(r"@brief\s+Converts or compares the represented value\."),
    re.compile(r"@brief\s+(?:Starts start|Ends or resets|Obtains load|Updates set)\b"),
    re.compile(r"@brief\s+Reports whether\b.+\bis true\."),
    re.compile(
        r"@details\s+The operation observes the current object state without modifying it\."
    ),
    re.compile(
        r"\bImplementations provide the concrete behavior required by the owning interface\."
    ),
    re.compile(r"\bThe operation exposes the stable behavior of the owning type to its callers\."),
    re.compile(r"\bThe operation follows the contract of the owning type\b"),
    re.compile(r"@return\s+True when the reported condition holds; false otherwise\."),
    re.compile(r"\bsupplied to the operation\b"),
    re.compile(r"\bsupplied by the caller\b"),
    re.compile(r"\bThe value produced by the operation\."),
    re.compile(r"\bTrue when the operation succeeds\b"),
    re.compile(r"\bOptional destination for a human-readable diagnostic\."),
    re.compile(r"\bPath of the input or output file\."),
    re.compile(r"\bWhether the corresponding feature is enabled\."),
    re.compile(r"\bFinite numeric value used by the operation\."),
    re.compile(r"\bAngle value expressed in (?:radians|degrees)\."),
)

REQUIRED_CONTRACTS: dict[str, tuple[str, ...]] = {
    "ProjectSpecification.md": (
        "no product maximum for records per session",
        "0.65 pixel",
        "Qt 6.10.3",
        "threat model",
    ),
    "docs/THREAT_MODEL.md": (
        "## Assets and adverse outcomes",
        "## Trust boundaries and data flow",
        "## Attack surfaces and controls",
        "## STRIDE summary",
        "64 MiB",
        "symlinks",
        "4,096 records",
        "deferred deletion",
        "residual risk",
    ),
    "src/viewer/plane/glb_resource_reader.h": (
        "canonically contained",
        "non-symlink",
        "aggregate logical-byte budget",
    ),
    "src/infrastructure/session/lar_session_reader.h": (
        "no record-count policy limit",
        "4,096 records",
        "owning thread",
    ),
    "src/infrastructure/runtime/threaded_application_runtime.h": (
        "synchronously drains each worker",
        "complete QObject tree",
        "deferred deletion",
    ),
    "src/viewer/viewport/lar_parametric_zone_gpu_layer.h": (
        "0.65-pixel error",
        "CPU mesh path",
    ),
}


def reviewed_sources(root: Path) -> list[Path]:
    """Return production sources and reusable test support whose prose is reviewed."""
    files = [*root.joinpath("src").rglob("*.h"), *root.joinpath("src").rglob("*.cpp")]
    files.extend(root.joinpath("tools", "map_asset").rglob("*.h"))
    files.extend(root.joinpath("tools", "map_asset").rglob("*.cpp"))
    files.extend(root.joinpath("tests", "support").rglob("*.h"))
    files.extend(root.joinpath("tests", "support").rglob("*.cpp"))
    return sorted(files)


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    failures: list[str] = []
    block_count = 0

    for source in reviewed_sources(root):
        text = source.read_text(encoding="utf-8")
        for block in DOXYGEN_BLOCK.findall(text):
            block_count += 1
            for pattern in BOILERPLATE:
                match = pattern.search(block)
                if match:
                    line = text.count("\n", 0, text.find(block)) + 1
                    failures.append(
                        f"{source.relative_to(root)}:{line}: generated filler: "
                        f"{match.group(0).strip()}"
                    )
                    break

    for relative, required_fragments in REQUIRED_CONTRACTS.items():
        path = root / relative
        if not path.is_file():
            failures.append(f"{relative}: required semantic contract is missing")
            continue
        normalized = path.read_text(encoding="utf-8").casefold()
        for fragment in required_fragments:
            if fragment.casefold() not in normalized:
                failures.append(f"{relative}: missing required contract text: {fragment}")

    if failures:
        print("Documentation quality gate failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print(
        "Documentation quality gate passed "
        f"({block_count} reviewed Doxygen blocks, {len(REQUIRED_CONTRACTS)} semantic contracts)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
