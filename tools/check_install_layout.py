#!/usr/bin/env python3
"""Validate the installed Tier-1 layout and sender startup."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path


def require_file(path: Path) -> None:
    if not path.is_file() or path.stat().st_size == 0:
        raise RuntimeError(f"missing or empty installed file: {path}")


def require_one_file(paths: tuple[Path, ...]) -> Path:
    for path in paths:
        if path.is_file() and path.stat().st_size > 0:
            return path
    raise RuntimeError(f"none of the required files exist: {', '.join(map(str, paths))}")


def validate_sbom(document: object) -> None:
    """Require a patched, componentized Qt runtime inventory."""
    if not isinstance(document, dict) or document.get("spdxVersion") != "SPDX-2.3":
        raise RuntimeError("installed SBOM is not an SPDX 2.3 document")
    packages = document.get("packages")
    relationships = document.get("relationships")
    if not isinstance(packages, list) or not isinstance(relationships, list):
        raise RuntimeError("installed SBOM has no package/relationship inventory")
    by_name = {
        package.get("name"): package
        for package in packages
        if isinstance(package, dict) and isinstance(package.get("name"), str)
    }
    platform_plugin = (
        "Qt6PlatformPlugin-qcocoa"
        if sys.platform == "darwin"
        else "Qt6PlatformPlugin-qwindows"
        if sys.platform == "win32"
        else "Qt6PlatformPlugin-qxcb"
    )
    expected_qt = {
        "Qt6Core",
        "Qt6Concurrent",
        "Qt6Gui",
        "Qt6Network",
        "Qt6Widgets",
        "Qt6OpenGL",
        "Qt6OpenGLWidgets",
        platform_plugin,
        "Qt6ImageHandler-png",
        "Qt6ImageHandler-jpeg",
    }
    missing = expected_qt - by_name.keys()
    if missing or "Qt" in by_name:
        raise RuntimeError(
            f"installed SBOM Qt inventory is incomplete or generic: {sorted(missing)}"
        )

    versions: set[str] = set()
    expected_ids: set[str] = set()
    for name in expected_qt:
        package = by_name[name]
        version = package.get("versionInfo")
        if not isinstance(version, str):
            raise RuntimeError(f"SBOM package {name} has no version")
        try:
            numeric = tuple(int(part) for part in version.split(".")[:3])
        except ValueError as error:
            raise RuntimeError(f"SBOM package {name} has invalid version {version}") from error
        if len(numeric) != 3 or numeric < (6, 10, 3):
            raise RuntimeError(f"SBOM package {name} is below the Qt 6.10.3 baseline")
        refs = package.get("externalRefs")
        if not isinstance(refs, list) or not any(
            isinstance(ref, dict)
            and ref.get("referenceType") == "purl"
            and isinstance(ref.get("referenceLocator"), str)
            for ref in refs
        ):
            raise RuntimeError(f"SBOM package {name} has no package URL")
        versions.add(version)
        expected_ids.add(str(package.get("SPDXID")))
    if len(versions) != 1:
        raise RuntimeError(f"installed Qt components have inconsistent versions: {versions}")

    lar_dependencies = {
        relationship.get("relatedSpdxElement")
        for relationship in relationships
        if isinstance(relationship, dict)
        and relationship.get("spdxElementId") == "SPDXRef-Package-LAR"
        and relationship.get("relationshipType") == "DEPENDS_ON"
    }
    if not expected_ids <= lar_dependencies:
        raise RuntimeError("installed SBOM omits direct Qt dependency relationships")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_install_layout.py PREFIX", file=sys.stderr)
        return 2
    prefix = Path(sys.argv[1]).resolve()
    executable_suffix = ".exe" if sys.platform == "win32" else ""
    viewer = prefix / "bin" / f"lar-viewer{executable_suffix}"
    sender_path = prefix / "bin" / f"lar-test-sender{executable_suffix}"
    if sys.platform == "darwin":
        viewer = prefix / "lar-viewer.app" / "Contents" / "MacOS" / "lar-viewer"
    viewer_asset_root = viewer.parent / "assets"
    required = (
        viewer,
        viewer_asset_root / "models" / "f16_3.glb",
        viewer_asset_root / "water" / "dted0_water_mask.bin",
        sender_path,
        prefix / "bin" / "lar_world_map.larmap",
        prefix / "bin" / "lar_world_map.manifest.json",
        prefix / "share" / "lar-area-display" / "maps" / "full-state.json",
        prefix / "share" / "lar-area-display" / "maps" / "dlz-inputs.json",
        prefix / "share" / "lar-area-display" / "maps" / "full-state-dlz.json",
        prefix / "share" / "lar-area-display" / "lar-sbom.spdx.json",
    )
    try:
        for path in required:
            require_file(path)
        if sys.platform == "win32":
            require_file(prefix / "bin" / "Qt6Core.dll")
            require_one_file(
                (
                    prefix / "plugins" / "platforms" / "qwindows.dll",
                    prefix / "bin" / "platforms" / "qwindows.dll",
                    prefix / "bin" / "plugins" / "platforms" / "qwindows.dll",
                    prefix / "share" / "qt" / "plugins" / "platforms" / "qwindows.dll",
                )
            )
        cubemap_faces = sorted((viewer_asset_root / "cubemaps").glob("*.png"))
        if not cubemap_faces:
            raise RuntimeError("installed Plane cubemap catalog is empty")
        cubemap_sets: dict[str, set[str]] = {}
        for path in cubemap_faces:
            require_file(path)
            match = re.fullmatch(r"(.+)_(rt|lf|up|dn|ft|bk)\.png", path.name, re.IGNORECASE)
            if match is None:
                raise RuntimeError(f"invalid installed cubemap face name: {path.name}")
            cubemap_sets.setdefault(match.group(1), set()).add(match.group(2).lower())
        required_faces = {"rt", "lf", "up", "dn", "ft", "bk"}
        for name, faces in cubemap_sets.items():
            if faces != required_faces:
                raise RuntimeError(f"incomplete installed cubemap set: {name}")
        manifest_path = prefix / "bin" / "lar_world_map.manifest.json"
        sbom_path = prefix / "share" / "lar-area-display" / "lar-sbom.spdx.json"
        json.loads(manifest_path.read_text(encoding="utf-8"))
        validate_sbom(json.loads(sbom_path.read_text(encoding="utf-8")))
        sender = subprocess.run(
            [str(sender_path), "--list-scenarios"],
            check=False,
            capture_output=True,
            text=True,
            timeout=15,
        )
        if sender.returncode != 0 or "hundred-hz" not in sender.stdout:
            raise RuntimeError(
                f"installed sender smoke failed ({sender.returncode}): {sender.stderr}"
            )
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        return 1
    print(f"Installed layout is complete: {prefix}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
