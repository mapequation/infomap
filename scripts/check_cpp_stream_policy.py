#!/usr/bin/env python3
"""Enforce the C++ runtime stream policy.

Runtime code that is shared with bindings should not introduce direct global
std stream usage. Keep CLI/error handling and central log routing explicit by
allowing only src/main.cpp and src/utils/Log.cpp.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

DEFAULT_SCAN_PATTERNS = (
    "src/**/*.h",
    "src/**/*.cpp",
    "interfaces/swig/**/*.i",
)
EXTRA_SCAN_FILES = ("interfaces/R/generated/infomap_wrap.cpp",)
ALLOWED_FILES = {
    Path("src/main.cpp"),
    Path("src/utils/Log.cpp"),
}
STREAM_NAMES = ("cout", "cerr", "clog", "wcout", "wcerr", "wclog")
FORBIDDEN_PATTERNS = (
    ("include <iostream>", re.compile(r"^\s*#\s*include\s*<\s*iostream\s*>")),
    (
        "global std stream",
        re.compile(r"\bstd\s*::\s*(?:" + "|".join(STREAM_NAMES) + r")\b"),
    ),
    (
        "using global std stream",
        re.compile(r"\busing\s+std\s*::\s*(?:" + "|".join(STREAM_NAMES) + r")\b"),
    ),
    # fmt is vendored iostream-free (fmt/base.h, format.h, format-inl.h only).
    # These fmt headers reintroduce <iostream>/<ostream>/FILE* and must not leak
    # into shared runtime code; include "utils/format.h" (which pulls only the
    # safe subset) instead.
    (
        "include unsafe fmt header",
        re.compile(r"^\s*#\s*include\s*<\s*fmt/(?:ostream|os|printf|color|std)\.h\s*>"),
    ),
    # fmt::print/println write to stdout/FILE*, bypassing the infomap::Log sink
    # (and the R Rprintf redirect). Build strings with fmt::format(...) and route
    # them through Log instead.
    ("fmt::print to stdout", re.compile(r"\bfmt\s*::\s*println?\s*\(")),
)

# The two wrapper headers are the ONLY place allowed to include <fmt/...>
# directly. Everything else must go through them so FMT_HEADER_ONLY is defined
# identically on every build surface (CLI, Python, R, JS). A stray
# `#include <fmt/format.h>` in a TU that hasn't pinned FMT_HEADER_ONLY first is
# an ODR/link landmine that only detonates on one specific build. See
# src/utils/format.h.
FMT_WRAPPER_FILES = {
    Path("src/utils/format.h"),
    Path("src/utils/format_core.h"),
}

# Patterns whose allow-list differs from the global ALLOWED_FILES (main.cpp /
# Log.cpp). Each entry carries its own set of files permitted to match.
SCOPED_FORBIDDEN_PATTERNS = (
    (
        "direct <fmt/...> include (use utils/format.h instead)",
        re.compile(r"^\s*#\s*include\s*<\s*fmt/"),
        FMT_WRAPPER_FILES,
    ),
)


def repo_relative(path: Path, root: Path) -> Path:
    return path.resolve().relative_to(root)


def iter_scan_files(root: Path) -> list[Path]:
    files: set[Path] = set()
    for pattern in DEFAULT_SCAN_PATTERNS:
        files.update(path for path in root.glob(pattern) if path.is_file())
    for relpath in EXTRA_SCAN_FILES:
        path = root / relpath
        if path.is_file():
            files.add(path)
    return sorted(files)


def scan_file(path: Path, root: Path) -> tuple[list[str], list[str]]:
    relpath = repo_relative(path, root)
    findings: list[str] = []
    allowed_hits: list[str] = []

    for line_number, line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), 1
    ):
        for label, pattern in FORBIDDEN_PATTERNS:
            if not pattern.search(line):
                continue
            message = f"{relpath}:{line_number}: forbidden {label}: {line.strip()}"
            if relpath in ALLOWED_FILES:
                allowed_hits.append(message)
            else:
                findings.append(message)

        for label, pattern, allowed in SCOPED_FORBIDDEN_PATTERNS:
            if not pattern.search(line):
                continue
            message = f"{relpath}:{line_number}: forbidden {label}: {line.strip()}"
            if relpath in allowed:
                allowed_hits.append(message)
            else:
                findings.append(message)

    return findings, allowed_hits


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root to scan. Defaults to the parent of scripts/.",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    findings: list[str] = []
    allowed_hits: list[str] = []

    for path in iter_scan_files(root):
        file_findings, file_allowed_hits = scan_file(path, root)
        findings.extend(file_findings)
        allowed_hits.extend(file_allowed_hits)

    if findings:
        print("Forbidden direct C++ std stream usage found:\n", file=sys.stderr)
        print("\n".join(findings), file=sys.stderr)
        print(
            "\nUse infomap::Log or an explicit caller-owned stream instead. "
            "Only src/main.cpp and src/utils/Log.cpp may use <iostream> or "
            "global std streams directly. For formatting, include "
            '"utils/format.h" and use fmt::format(...) (never fmt::print, and '
            "never fmt/ostream.h). Only src/utils/format.h and "
            "src/utils/format_core.h may include <fmt/...> directly; every other "
            "file must include those wrappers so FMT_HEADER_ONLY stays "
            "consistent across build surfaces.",
            file=sys.stderr,
        )
        return 1

    if allowed_hits:
        print("Allowed std stream usage is confined to:")
        for hit in allowed_hits:
            print(f"  {hit}")
    else:
        print("No direct C++ std stream usage found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
