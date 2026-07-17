"""Print notebook paths from the tutorial notebook manifest."""

from __future__ import annotations

import argparse
import sys
import tomllib
from pathlib import Path

SUITE_TIERS = {
    "smoke": {"smoke"},
    "full": {"smoke", "full"},
    "manual": {"manual"},
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--suite", choices=sorted(SUITE_TIERS), required=True)
    parser.add_argument("--print0", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = tomllib.loads(args.manifest.read_text())
    selected_tiers = SUITE_TIERS[args.suite]
    paths = [
        notebook["path"]
        for notebook in manifest["notebooks"]
        if notebook["tier"] in selected_tiers
    ]

    separator = "\0" if args.print0 else "\n"
    sys.stdout.write(separator.join(paths))
    if paths:
        sys.stdout.write(separator)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
