#!/usr/bin/env python3
"""Verify that all packaged version strings match a release tag.

Release artifacts are built from several files that each carry their own
version string. release-please keeps them in sync, but a botched bump (or a
hand-edited tag) silently produces packages whose versions disagree with the
tag, which only surfaces late in the release. This check compares every
version-bearing file against the tag (and against each other) before any
artifacts are built.

Only fixed, tracked source paths are read -- never a repo-wide search -- so a
stale local build artifact (e.g. a gitignored include/version.h) cannot cause
a spurious mismatch.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Callable, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent

TAG_RE = re.compile(r"^v(\d+\.\d+\.\d+)$")


def _extract_regex(pattern: re.Pattern[str]) -> Callable[[str], Optional[str]]:
    def extractor(text: str) -> Optional[str]:
        match = pattern.search(text)
        return match.group(1) if match else None

    return extractor


def _extract_json_version(text: str) -> Optional[str]:
    try:
        return json.loads(text).get("version")
    except json.JSONDecodeError:
        return None


# (label, relative path, extractor). Authoritative list mirrors the
# extra-files tracked in release-please-config.json plus the root package.json.
SOURCES: tuple[tuple[str, str, Callable[[str], Optional[str]]], ...] = (
    (
        "src/version.h",
        "src/version.h",
        _extract_regex(re.compile(r'INFOMAP_VERSION\s*=\s*"([^"]+)"')),
    ),
    (
        "python _version.py",
        "interfaces/python/src/infomap/_version.py",
        _extract_regex(re.compile(r'__version__\s*=\s*"([^"]+)"')),
    ),
    (
        "R DESCRIPTION",
        "interfaces/R/infomap/DESCRIPTION",
        _extract_regex(re.compile(r"^Version:\s*(.+?)\s*$", re.MULTILINE)),
    ),
    (
        "package.json",
        "package.json",
        _extract_json_version,
    ),
    (
        "CITATION.cff",
        "CITATION.cff",
        _extract_regex(re.compile(r"^version:\s*(.+?)\s*$", re.MULTILINE)),
    ),
)


def parse_tag(tag: str) -> str:
    """Return the expected version for a stable vX.Y.Z tag, or exit on error."""
    match = TAG_RE.match(tag)
    if not match:
        print(f"Expected stable release tag vX.Y.Z, got: {tag}", file=sys.stderr)
        raise SystemExit(1)
    return match.group(1)


def collect() -> list[tuple[str, str, Optional[str]]]:
    rows: list[tuple[str, str, Optional[str]]] = []
    for label, rel_path, extractor in SOURCES:
        path = REPO_ROOT / rel_path
        if not path.is_file():
            rows.append((label, rel_path, None))
            continue
        rows.append((label, rel_path, extractor(path.read_text(encoding="utf-8"))))
    return rows


def render_table(expected: str, rows: list[tuple[str, str, Optional[str]]]) -> str:
    lines = [
        f"Expected version (from tag): {expected}",
        "",
        f"| {'File':<20} | {'Version':<12} | Status |",
        f"| {'-' * 20} | {'-' * 12} | ------ |",
    ]
    for label, _rel_path, found in rows:
        if found is None:
            status, shown = "MISSING", "-"
        elif found == expected:
            status, shown = "OK", found
        else:
            status, shown = "MISMATCH", found
        lines.append(f"| {label:<20} | {shown:<12} | {status} |")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True, help="Release tag, e.g. v2.11.0")
    args = parser.parse_args()

    expected = parse_tag(args.tag)
    rows = collect()
    table = render_table(expected, rows)
    print(table)

    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_path:
        with open(summary_path, "a", encoding="utf-8") as summary:
            summary.write(table + "\n")

    ok = all(found == expected for _label, _rel_path, found in rows)
    if not ok:
        print(
            "\nVersion mismatch: every tracked version must equal the tag.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
