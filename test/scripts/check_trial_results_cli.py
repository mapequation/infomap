#!/usr/bin/env python3

import json
import subprocess
import sys
import tempfile
from pathlib import Path

from schema_validation import validate_json_schema


def main(argv: list[str]) -> int:
    infomap_bin = argv[1]

    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        (work / "network.net").write_text(
            "*Vertices 6\n"
            '1 "one"\n'
            '2 "two"\n'
            '3 "three"\n'
            '4 "four"\n'
            '5 "five"\n'
            '6 "six"\n'
            "*Edges\n"
            "1 2 1\n"
            "2 3 1\n"
            "3 1 1\n"
            "4 5 1\n"
            "5 6 1\n"
            "6 4 1\n",
            encoding="utf-8",
        )

        result = subprocess.run(
            [
                infomap_bin,
                "network.net",
                "out",
                "--silent",
                "--seed",
                "7",
                "--num-trials",
                "2",
                "--trial-offset",
                "5",
                "--trial-results",
                "trial-results.json",
                "--no-final-output",
                "--tree",
            ],
            cwd=work,
            check=False,
            text=True,
            capture_output=True,
        )
        assert result.returncode == 0, result.stderr

        data = json.loads((work / "trial-results.json").read_text(encoding="utf-8"))
        validate_json_schema(data, "trial-results.schema.json")
        assert data["trial_offset"] == 5
        assert data["num_trials"] == 2
        assert [trial["trial"] for trial in data["trials"]] == [5, 6]
        assert data["best_tree_file"]

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
