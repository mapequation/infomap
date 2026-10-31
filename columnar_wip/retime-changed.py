#!/usr/bin/env python3
"""Re-time every configuration whose codelength a change moves, interleaved and as the minimum of N.

    retime-changed.py <sweep.tsv> <out.tsv> old=<binary> new=<binary> --rows <results.tsv>
                      [--reps 5]

retime-outliers.py re-runs only the rows a sweep shows far off in seconds; a change that moves
the search needs every moved row timed alike, since those rows are the trade-off it asks a
reviewer to accept. This picks every columnar configuration whose old and new codelength differ
in <sweep.tsv> (a bench-sync.py sweep) and runs <reps> rounds of (old, new) or (new, old),
alternating, keeping every run.

--rows is the row set the sweep was driven from (bench-sync.py's --rows); it resolves each
label to its input network exactly as the sweep did.

Row: key label flags arm round total_s instr codelength
"""

import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile
from collections import defaultdict

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_bench_sync():
    spec = importlib.util.spec_from_file_location(
        "bench_sync", f"{R}/columnar_wip/bench-sync.py"
    )
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def changed_configs(sweep):
    first = defaultdict(dict)
    with open(sweep) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) < 10 or p[0].startswith("OO") or p[2] not in ("old", "new"):
                continue
            first[p[0], p[1], " ".join(p[4].split())].setdefault(p[2], p[5])
    return [k for k, v in first.items() if v.get("old") != v.get("new")]


def main():
    argv = sys.argv[1:]
    if len(argv) < 2:
        sys.exit(__doc__)
    sweep, out = argv[0], argv[1]
    bins, rows_path, reps = {}, None, 5
    i = 2
    while i < len(argv):
        if argv[i] == "--rows":
            rows_path = argv[i + 1]
            i += 2
        elif argv[i] == "--reps":
            reps = int(argv[i + 1])
            i += 2
        else:
            k, v = argv[i].split("=", 1)
            bins[k] = os.path.abspath(v)
            i += 1
    if rows_path is None or set(bins) != {"old", "new"}:
        sys.exit(__doc__)
    nets = load_bench_sync().net_of_label(rows_path)
    picked = changed_configs(sweep)
    print(f"{len(picked)} changed configurations", flush=True)
    with (
        tempfile.TemporaryDirectory(prefix="retime-changed-") as tmp,
        open(out, "w") as fo,
    ):
        tj = f"{tmp}/timing.json"
        for rnd in range(reps):
            order = ("old", "new") if rnd % 2 == 0 else ("new", "old")
            for key, label, flags in picked:
                for arm in order:
                    if os.path.exists(tj):
                        os.remove(tj)
                    cmd = [
                        "/usr/bin/time",
                        "-l",
                        bins[arm],
                        f"{R}/{nets[key, label]}",
                        tmp,
                        *flags.split(),
                        "--seed",
                        "123",
                        "--no-file-output",
                        "--timing-json",
                        tj,
                    ]
                    p = subprocess.run(
                        cmd, cwd=R, capture_output=True, text=True, check=False
                    )
                    text = p.stdout + p.stderr
                    with open(tj) as f:
                        total = json.load(f)["timing"]["total_s"]
                    instr = re.search(r"(\d+)\s+instructions retired", text).group(1)
                    cl = re.search(r"Best codelength\s+([0-9.eE+-]+)", text).group(1)
                    fo.write(
                        "\t".join(
                            [key, label, flags, arm, str(rnd), str(total), instr, cl]
                        )
                        + "\n"
                    )
                    fo.flush()
            print(f"round {rnd} done", flush=True)


if __name__ == "__main__":
    main()
