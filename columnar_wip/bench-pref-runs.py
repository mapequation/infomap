#!/usr/bin/env python3
"""The 24 preferred-modules runs (#1408's set), interleaved across binaries.

usage: bench-pref-runs.py OUT.tsv ARM=BINARY [ARM=BINARY ...] [--reps N]

Twelve configurations, each in the standard search and `-2`, at `-N5` (word_assoc
K = 25 and science2001 at `-N10`), `--seed 123`. Every round runs each row once per
arm, arm after arm, so the arms share the machine's moments. A row records the
final `Best codelength` (stdout, full precision, after the winner's repair), the top
modules and levels, `--timing-json`'s `timing.total_s` and instructions retired
(`/usr/bin/time -l`). Inputs: `networks/columnar-benchmark/` (see
`columnar_wip/benchmark-networks.md`) and science2001 from `$SCIENCE2001`.
"""

import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

NET = Path("networks/columnar-benchmark")
SCIENCE2001 = Path(
    os.environ.get(
        "SCIENCE2001",
        "/Users/daniel/dev/projects/icelab/code/web/navigator/public/data/science2001.net",
    )
)

# label, network, preferred modules, flags
CONFIGS = [
    ("air30k_k20", NET / "air30k.net", 20, ["-N5"]),
    ("jazz_k10", NET / "jazz.txt", 10, ["-N5"]),
    ("jazz_k100", NET / "jazz.txt", 100, ["-N5"]),
    ("netscience_k50", NET / "netscience.txt", 50, ["-N5"]),
    ("polblogs_k400", NET / "polblogs.txt", 400, ["-d", "-N5"]),
    ("polblogs_k5", NET / "polblogs.txt", 5, ["-d", "-N5"]),
    ("powergrid_k1500", NET / "powergrid.txt", 1500, ["-N5"]),
    ("powergrid_k30", NET / "powergrid.txt", 30, ["-N5"]),
    ("science2001_k25", SCIENCE2001, 25, ["-d", "-N10"]),
    ("word_assoc_k100", NET / "word_assoc.txt", 100, ["-d", "-N5"]),
    ("word_assoc_k25", NET / "word_assoc.txt", 25, ["-d", "-N10"]),
    ("word_assoc_k3000", NET / "word_assoc.txt", 3000, ["-d", "-N5"]),
]
MODES = {"std": [], "-2": ["-2"]}


def run(binary, net, k, flags, mode, tmp):
    timing = Path(tmp) / "t.json"
    cmd = [
        "/usr/bin/time",
        "-l",
        binary,
        str(net),
        tmp,
        "-C",
        *MODES[mode],
        *flags,
        "--seed",
        "123",
        "--preferred-number-of-modules",
        str(k),
        "--timing-json",
        str(timing),
        "--no-file-output",
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    out = proc.stdout
    codelength = re.search(r"Best codelength\s+([0-9.eE+-]+)", out).group(1)
    top = re.search(r"Top modules\s+(\d+)", out).group(1)
    levels = re.search(r"^[ \t]*Levels[ \t]+(\d+)[ \t]*$", out, re.MULTILINE).group(1)
    instr = re.search(r"(\d+)\s+instructions retired", proc.stderr).group(1)
    with timing.open() as f:
        total = json.load(f)["timing"]["total_s"]
    return codelength, top, levels, total, instr


def main():
    args = sys.argv[1:]
    reps = 3
    if "--reps" in args:
        i = args.index("--reps")
        reps = int(args[i + 1])
        del args[i : i + 2]
    out_path, arms = args[0], [a.split("=", 1) for a in args[1:]]
    with open(out_path, "w") as out, tempfile.TemporaryDirectory() as tmp:
        out.write("cfg\tmode\tarm\trep\tcodelength\ttop\tlevels\ttotal_s\tinstr\n")
        for rep in range(1, reps + 1):
            for label, net, k, flags in CONFIGS:
                for mode in MODES:
                    for arm, binary in arms:
                        row = run(binary, net, k, flags, mode, tmp)
                        out.write(
                            "\t".join([label, mode, arm, str(rep), *map(str, row)])
                            + "\n"
                        )
                        out.flush()


if __name__ == "__main__":
    main()
