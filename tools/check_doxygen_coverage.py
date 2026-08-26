#!/usr/bin/env python3
"""Require file-level documentation and selected high-risk API contracts."""

from __future__ import annotations

import re
import sys
from pathlib import Path


FILE_BLOCK = re.compile(
    r"/\*\*(?:(?!\*/).)*@file\b(?:(?!\*/).)*@brief\b(?:(?!\*/).)*\*/",
    re.DOTALL,
)
DOXYGEN_BLOCK = re.compile(r"/\*\*(?:(?!\*/).)*\*/", re.DOTALL)
DOXYGEN_LINE = re.compile(r"///<?[^\n]*")

# These are intentionally an allowlist, not a blanket member-coverage target.
# Trivial accessors are often clearer without prose; the contracts below protect
# boundaries where omitted semantics have previously made the code hard to use
# safely (identity, units, coordinate systems, formats, and parser subsets).
CRITICAL_API_CONTRACTS: dict[str, tuple[str, ...]] = {
    "src/application/ports/application_runtime.h": (
        "exactly one operational completion",
        "acceptance only means",
        "recordingsavefinished",
    ),
    "src/application/source_lifecycle_coordinator.h": (
        "queued events from a stopped activation",
        "complete synchronously",
        "latest publication of each event type",
    ),
    "src/application/recording_pipeline_coordinator.h": (
        "drain token",
        "immutable snapshot",
        "stale completion",
    ),
    "src/application/playback_service.h": (
        "may skip source records",
        "seeking is inclusive",
        "without emitting completion",
    ),
    "src/domain/dlz/dlz_types.h": (
        "north-east-down (ned)",
        "metres per second",
        "positive when opening",
    ),
    "src/viewer/map/map_camera.h": (
        "origin at the upper-left",
        "projected degrees",
        "front hemisphere",
    ),
    "src/application/session_limits.h": (
        "little-endian",
        "no stored record-count field",
        "payload size",
    ),
    "src/viewer/map/map_asset_format.h": (
        "little-endian",
        "ieee crc-32",
        "tightly concatenated arrays",
    ),
    "src/viewer/plane/glb_model_reader.h": (
        "`triangles`",
        "`position` and `normal`",
        "largest extent",
    ),
    "tools/map_asset/polygon_triangulator.h": (
        "antimeridian",
        "ear clipping",
        "unbridgeable hole",
    ),
    "src/viewer/terrain/dted_mosaic_sampler.h": (
        "renormalizes the remaining weights",
        "least-recently-used",
        "non-negative depth",
    ),
}


def production_headers(root: Path) -> list[Path]:
    """Return maintained API headers, excluding tests and generated trees."""
    headers = list((root / "src").rglob("*.h"))
    headers.extend((root / "tools" / "map_asset").rglob("*.h"))
    return sorted(headers)


def contract_text(path: Path) -> str:
    """Normalize wrapped Doxygen prose without weakening phrase matching."""
    source = path.read_text(encoding="utf-8")
    text = "\n".join([*DOXYGEN_BLOCK.findall(source), *DOXYGEN_LINE.findall(source)]).casefold()
    text = re.sub(r"^\s*\*\s?", "", text, flags=re.MULTILINE)
    return " ".join(text.split())


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

    for relative, required_fragments in CRITICAL_API_CONTRACTS.items():
        path = root / relative
        if not path.is_file():
            failures.append(f"{relative}: critical API header is missing")
            continue
        normalized = contract_text(path)
        for fragment in required_fragments:
            if fragment.casefold() not in normalized:
                failures.append(f"{relative}: missing critical contract text: {fragment}")

    if failures:
        print("Doxygen coverage gate failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print(
        "Doxygen coverage gate passed "
        f"({len(headers)} production headers, "
        f"{len(CRITICAL_API_CONTRACTS)} critical API contracts)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
