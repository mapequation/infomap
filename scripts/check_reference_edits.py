#!/usr/bin/env python3
"""Keep edits to the reference oracles apart from edits to the core they check.

``test/python/reference_*.py`` recompute flow and codelength from first
principles. An oracle changed together with the code it checks stops being
independent: one wrong idea can make both agree (#1214). So a pull request that
changes a reference module must not change anything under ``src/``.

Split such a change in two. First the reference change, citing its source in
the module's "Sources" section, with the tests it breaks pinned as strict
xfails. Then the core fix, which removes those xfails.

Usage: check_reference_edits.py BASE compares HEAD with its merge base with BASE.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from collections.abc import Iterable
from fnmatch import fnmatch

REFERENCE = "test/python/reference_*.py"
CORE = "src/"


def mixed(paths: Iterable[str]) -> tuple[list[str], list[str]]:
    """The reference and core files of a change that touches both, else empty."""
    paths = list(paths)
    references = [p for p in paths if fnmatch(p, REFERENCE)]
    core = [p for p in paths if p.startswith(CORE)]
    if references and core:
        return references, core
    return [], []


def changed(base: str) -> list[str]:
    out = subprocess.run(
        ["git", "diff", "--name-only", f"{base}...HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return out.splitlines()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("base", help="the branch the change goes into")
    args = parser.parse_args(argv)
    references, core = mixed(changed(args.base))
    if not references:
        return 0
    print(
        "This change edits a reference oracle and the core it checks:\n"
        + "".join(f"  {p}\n" for p in references + core)
        + "Land the reference change in its own pull request, citing its source "
        "in the module's Sources section; see scripts/check_reference_edits.py.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
