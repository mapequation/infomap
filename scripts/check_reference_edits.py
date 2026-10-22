#!/usr/bin/env python3
"""Keep edits to the reference oracles apart from edits to the core they check.

``test/python/reference_*.py`` recompute flow and codelength from first
principles. An oracle changed together with the code it checks stops being
independent: one wrong idea can make both agree (#1214). So a pull request that
changes a reference module must not change anything under ``src/``.

Split such a change in two. First the reference change, citing its source in
the module's "Sources" section, with the tests it breaks pinned as strict
xfails. Then the core fix, which removes those xfails.

A file that HEAD has exactly as the upstream branch has it is not an edit of
this change. Merging master into a long-lived branch brings master's own oracle
and core edits, which landed apart there; counting them refused every such sync.

Usage: check_reference_edits.py BASE [--upstream REF] compares HEAD with its
merge base with BASE, leaving out the files HEAD has as REF (origin/master).
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from collections.abc import Iterable
from fnmatch import fnmatch

REFERENCE = "test/python/reference_*.py"
CORE = "src/"
UPSTREAM = "origin/master"


def mixed(paths: Iterable[str]) -> tuple[list[str], list[str]]:
    """The reference and core files of a change that touches both, else empty."""
    paths = list(paths)
    references = [p for p in paths if fnmatch(p, REFERENCE)]
    core = [p for p in paths if p.startswith(CORE)]
    if references and core:
        return references, core
    return [], []


def changed(base: str, cwd: str | None = None) -> list[str]:
    # Without rename detection a moved file is listed at both paths, so a move
    # out of src/ or out of a reference module counts too.
    out = subprocess.run(
        ["git", "diff", "--name-only", "--no-renames", f"{base}...HEAD"],
        check=True,
        capture_output=True,
        text=True,
        cwd=cwd,
    ).stdout
    return out.splitlines()


def edited(paths: Iterable[str], upstream: str, cwd: str | None = None) -> list[str]:
    """The paths whose content at HEAD differs from the upstream branch's."""
    paths = list(paths)
    known = subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", f"{upstream}^{{commit}}"],
        check=False,
        capture_output=True,
        cwd=cwd,
    )
    if known.returncode != 0:
        return paths  # no upstream to compare with: every changed file counts
    out = subprocess.run(
        ["git", "diff", "--name-only", "--no-renames", upstream, "HEAD"],
        check=True,
        capture_output=True,
        text=True,
        cwd=cwd,
    ).stdout
    differ = set(out.splitlines())
    return [p for p in paths if p in differ]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("base", help="the branch the change goes into")
    parser.add_argument(
        "--upstream",
        default=UPSTREAM,
        help="the branch oracle edits land on (default: %(default)s)",
    )
    args = parser.parse_args(argv)
    references, core = mixed(edited(changed(args.base), args.upstream))
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
