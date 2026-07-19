#!/usr/bin/env python3
"""ctest gate: verify the tracked golden-codelength manifest matches what the
built CLI produces right now. This is the CLI's own parity assertion (the
Python and R suites assert the same manifest from their surfaces).

Usage: check_golden_codelengths.py <infomap-binary>
"""

import difflib
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "scripts"))

import generate_golden_codelengths as gen  # noqa: E402


def main() -> int:
    if len(sys.argv) != 2:
        sys.stderr.write("usage: check_golden_codelengths.py <infomap-binary>\n")
        return 2
    infomap = sys.argv[1]

    tracked = gen.MANIFEST.read_text(encoding="utf-8")
    regenerated = gen.render(gen.generate(infomap))

    if tracked != regenerated:
        sys.stderr.write(
            "golden-codelength manifest is stale or the CLI drifted.\n"
            "If this is an intentional algorithm/codelength change, run "
            "`make build-golden-codelengths` and commit the diff.\n\n"
        )
        sys.stderr.writelines(
            difflib.unified_diff(
                tracked.splitlines(keepends=True),
                regenerated.splitlines(keepends=True),
                fromfile="tracked",
                tofile="regenerated",
            )
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
