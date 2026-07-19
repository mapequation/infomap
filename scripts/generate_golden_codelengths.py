#!/usr/bin/env python3
"""Generate the golden-codelength manifest from the Infomap CLI.

The CLI is the reference implementation. For each entry this runs
``Infomap <network> <flags> --summary-json -`` and records the resulting
codelength and module/level counts. The Python and R test suites replay the
exact same ``flags`` string against the same network and must reproduce these
numbers -- pinning CLI == Python == R three-way (see
test/python/test_golden_codelengths.py and the R testthat suite).

Regenerate whenever an intentional algorithm/codelength change lands; the diff
is a reviewable record of that change:

    make build-golden-codelengths
"""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MANIFEST = REPO_ROOT / "test/fixtures/expected/golden-codelengths.json"

# Every entry runs single-threaded (seed 123, one trial) so the codelength is
# identical across OpenMP / no-OpenMP builds and platforms. Keep networks small
# and under examples/networks so the R package can stage them. Add entries per
# flow representation; drop any that turn out not to be cross-platform stable.
_DETERMINISM = "--seed 123 --num-trials 1 --silent"
ENTRIES = (
    (
        "twotriangles-two-level",
        "examples/networks/twotriangles.net",
        f"--two-level {_DETERMINISM}",
    ),
    (
        "twotriangles-directed",
        "examples/networks/twotriangles.net",
        f"--two-level --directed {_DETERMINISM}",
    ),
    ("ninetriangles-hierarchical", "examples/networks/ninetriangles.net", _DETERMINISM),
    ("states-memory", "examples/networks/states.net", f"--two-level {_DETERMINISM}"),
    ("bipartite", "examples/networks/bipartite.net", f"--two-level {_DETERMINISM}"),
    ("multilayer", "examples/networks/multilayer.net", f"--two-level {_DETERMINISM}"),
    (
        "multilayer-intra-inter",
        "examples/networks/multilayer_intra_inter.net",
        f"--two-level {_DETERMINISM}",
    ),
)


def _run(infomap: str, network: str, flags: str) -> dict:
    result = subprocess.run(
        [
            infomap,
            str(REPO_ROOT / network),
            ".",
            *flags.split(),
            "--no-file-output",
            "--summary-json",
            "-",
        ],
        capture_output=True,
        text=True,
        check=True,
        cwd=REPO_ROOT,
    )
    return json.loads(result.stdout)


def generate(infomap: str) -> list[dict]:
    entries = []
    for entry_id, network, flags in ENTRIES:
        summary = _run(infomap, network, flags)
        entries.append(
            {
                "id": entry_id,
                "network": network,
                "flags": flags,
                "codelength": summary["codelength"],
                "num_top_modules": summary["top_modules"],
                "num_levels": summary["levels"],
            }
        )
    entries.sort(key=lambda e: e["id"])
    return entries


def render(entries: list[dict]) -> str:
    return json.dumps(entries, indent=2) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--infomap",
        default=str(REPO_ROOT / "Infomap"),
        help="Path to the Infomap CLI binary.",
    )
    parser.add_argument(
        "--output", default=str(MANIFEST), help="Manifest path to write."
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Print the manifest to stdout instead of writing it.",
    )
    args = parser.parse_args()

    text = render(generate(args.infomap))
    if args.check:
        print(text, end="")
    else:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        Path(args.output).write_text(text, encoding="utf-8")
        print(f"Wrote {len(json.loads(text))} entries to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
