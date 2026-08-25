#!/usr/bin/env python3
"""Fail when source or build dependencies cross the documented layers."""

from __future__ import annotations

import re
import sys
from pathlib import Path


INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
TARGET_LINK_LIBRARIES = re.compile(
    r"target_link_libraries\s*\(\s*([^\s)]+)(.*?)\)",
    re.IGNORECASE | re.DOTALL,
)


def source_files(directory: Path):
    for suffix in ("*.h", "*.hpp", "*.cpp"):
        yield from directory.rglob(suffix)


def target_links(cmake_file: Path, target: str) -> set[str]:
    """Return simple link tokens from one target_link_libraries block."""
    text = cmake_file.read_text(encoding="utf-8")
    for candidate, body in TARGET_LINK_LIBRARIES.findall(text):
        if candidate == target:
            return set(re.findall(r"[A-Za-z0-9_.:+-]+", body))
    return set()


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    failures: list[str] = []

    layer_rules = {
        "domain": ("application/", "infrastructure/", "viewer/"),
        "application": ("infrastructure/", "viewer/"),
        "infrastructure": ("viewer/",),
    }
    for layer, forbidden_prefixes in layer_rules.items():
        for path in source_files(root / "src" / layer):
            text = path.read_text(encoding="utf-8")
            for include in INCLUDE.findall(text):
                if include.startswith(forbidden_prefixes):
                    failures.append(
                        f"{path.relative_to(root)} imports forbidden {include}"
                    )

    concrete_qt = (
        "QFile",
        "QSaveFile",
        "QHostAddress",
        "QUdpSocket",
        "QThread",
        "QWidget",
        "QOpenGL",
        "QtNetwork/",
        "QtWidgets/",
    )
    for path in source_files(root / "src" / "application"):
        text = path.read_text(encoding="utf-8")
        for include in INCLUDE.findall(text):
            if include.startswith(concrete_qt):
                failures.append(
                    f"{path.relative_to(root)} imports concrete Qt API {include}"
                )

    domain_side_effect_qt = (
        "QFile",
        "QIODevice",
        "QObject",
        "QSaveFile",
        "QThread",
        "QTimer",
        "QtConcurrent/",
        "QtNetwork/",
        "QtWidgets/",
        "QOpenGL",
    )
    for path in source_files(root / "src" / "domain"):
        text = path.read_text(encoding="utf-8")
        for include in INCLUDE.findall(text):
            if include.startswith(domain_side_effect_qt):
                failures.append(
                    f"{path.relative_to(root)} imports side-effect Qt API {include}"
                )

    application_cmake = (root / "src/application/CMakeLists.txt").read_text(
        encoding="utf-8"
    )
    for forbidden_link in ("Qt6::Network", "Qt6::Widgets", "Qt6::OpenGL"):
        if forbidden_link in application_cmake:
            failures.append(
                f"src/application/CMakeLists.txt links forbidden {forbidden_link}"
            )

    link_rules = {
        (root / "src/domain/CMakeLists.txt", "lar-domain"): {
            "lar-application",
            "lar-infrastructure-qt",
            "lar-presentation-widgets",
            "lar-map-format",
            "lar-map-rendering",
            "lar-map-asset-compiler-support",
            "Qt6::Concurrent",
            "Qt6::Gui",
            "Qt6::Network",
            "Qt6::OpenGLWidgets",
            "Qt6::Widgets",
        },
        (root / "src/application/CMakeLists.txt", "lar-application"): {
            "lar-infrastructure-qt",
            "lar-presentation-widgets",
            "lar-map-format",
            "lar-map-rendering",
            "lar-map-asset-compiler-support",
            "Qt6::Concurrent",
            "Qt6::Gui",
            "Qt6::Network",
            "Qt6::OpenGLWidgets",
            "Qt6::Widgets",
        },
        (root / "src/infrastructure/CMakeLists.txt", "lar-infrastructure-qt"): {
            "lar-presentation-widgets",
            "lar-map-rendering",
            "lar-map-asset-compiler-support",
            "Qt6::OpenGLWidgets",
            "Qt6::Widgets",
        },
        (root / "src/viewer/map/CMakeLists.txt", "lar-map-format"): {
            "lar-application",
            "lar-domain",
            "lar-infrastructure-qt",
            "lar-map-rendering",
            "lar-presentation-widgets",
            "lar-map-asset-compiler-support",
            "Qt6::Network",
            "Qt6::OpenGLWidgets",
            "Qt6::Widgets",
        },
        (root / "src/viewer/map/CMakeLists.txt", "lar-map-rendering"): {
            "lar-application",
            "lar-domain",
            "lar-infrastructure-qt",
            "lar-presentation-widgets",
            "lar-map-asset-compiler-support",
            "Qt6::Network",
        },
        (root / "tools/CMakeLists.txt", "lar-map-asset-compiler-support"): {
            "lar-application",
            "lar-domain",
            "lar-infrastructure-qt",
            "lar-map-rendering",
            "lar-presentation-widgets",
            "Qt6::Network",
            "Qt6::OpenGLWidgets",
            "Qt6::Widgets",
        },
        (root / "src/viewer/CMakeLists.txt", "lar-viewer"): {
            "lar-map-asset-compiler",
            "lar-map-asset-compiler-support",
        },
    }
    for (cmake_file, target), forbidden_links in link_rules.items():
        links = target_links(cmake_file, target)
        if not links:
            failures.append(
                f"{cmake_file.relative_to(root)} has no parsed links for {target}"
            )
            continue
        for forbidden_link in sorted(links & forbidden_links):
            failures.append(
                f"{cmake_file.relative_to(root)} target {target} links forbidden "
                f"{forbidden_link}"
            )

    size_limits = {
        root / "CMakeLists.txt": 150,
        root / "src/viewer/mainwindow.cpp": 350,
        root / "src/viewer/viewport/earth_lar_view.cpp": 400,
    }
    for path, maximum in size_limits.items():
        count = len(path.read_text(encoding="utf-8").splitlines())
        if count > maximum:
            failures.append(
                f"{path.relative_to(root)} has {count} lines (maximum {maximum})"
            )

    for path in (root / "src").rglob("*.cpp"):
        if "testsender" in path.parts:
            continue
        count = len(path.read_text(encoding="utf-8").splitlines())
        if count > 600:
            failures.append(
                f"{path.relative_to(root)} has {count} lines (maximum 600)"
            )

    if failures:
        print("Architecture gate failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("Architecture gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
