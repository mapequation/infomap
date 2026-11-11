#!/usr/bin/env python3
"""Re-time a bench-sync.py sweep's outliers in seconds, interleaved and as the minimum of N.

    retime-outliers.py <sweep.tsv> <out.tsv> old=<binary> new=<binary> [--rows <results.tsv>]
                       [--reps 5]

A sweep runs each -N10 row once, so its seconds carry machine load; instructions retired are the
stable column there. This re-runs the rows the snapshot has to explain, by the rule earlier
snapshots used: every changed configuration more than 5% off in seconds in either direction, and
every unchanged one more than 10% slower. Columnar rows only; the OO rows of a columnar sub-PR are
carried. Each row runs <reps> rounds of (old, new) or (new, old), alternating, and keeps the
minimum of total_s and of instructions retired per arm.

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


def read_sweep(path):
    """(key, label, flags) -> {arm: [(codelength, total_s, top, levels)]}, columnar rows only."""
    rows = defaultdict(lambda: {"old": [], "new": []})
    with open(path) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) < 10 or p[0].startswith("OO") or p[2] not in ("old", "new"):
                continue
            if p[5] in ("", "NA") or p[6] in ("", "NA"):
                continue
            k = (p[0], p[1], " ".join(p[4].split()))
            rows[k][p[2]].append((float(p[5]), float(p[6]), p[8], p[9]))
    return rows


def outliers(sweep):
    picked = []
    for k, arms in sweep.items():
        if not arms["old"] or not arms["new"]:
            continue
        o, n = arms["old"], arms["new"]
        to, tn = min(x[1] for x in o), min(x[1] for x in n)
        if to <= 0:
            continue
        changed = (o[0][0], o[0][2:]) != (n[0][0], n[0][2:])
        dt = (tn - to) / to
        if (changed and abs(dt) > 0.05) or (not changed and dt > 0.10):
            picked.append((k, changed, dt))
    return picked


def main():
    argv = sys.argv[1:]
    if len(argv) < 4:
        sys.exit(__doc__)
    sweep_path, out = argv[0], argv[1]
    bins, rows_path, reps = {}, None, 5
    it = iter(argv[2:])
    for a in it:
        if a == "--rows":
            rows_path = os.path.abspath(next(it))
        elif a == "--reps":
            reps = int(next(it))
        elif "=" in a:
            arm, b = a.split("=", 1)
            bins[arm] = os.path.abspath(b)
    bench = load_bench_sync()
    nets = {
        (k, lb): net for k, lb, net, _, _ in bench.read_rows(rows_path or bench.ROWS)
    }
    picked = outliers(read_sweep(sweep_path))
    print(f"{len(picked)} rows to re-time", flush=True)
    with tempfile.TemporaryDirectory(prefix="retime-") as tmp, open(out, "a") as fo:
        for (key, label, flags), changed, dt in picked:
            for r in range(reps):
                order = ("old", "new") if r % 2 == 0 else ("new", "old")
                for arm in order:
                    tj = f"{tmp}/timing.json"
                    if os.path.exists(tj):
                        os.remove(tj)
                    cmd = (
                        [
                            "/usr/bin/time",
                            "-l",
                            bins[arm],
                            f"{R}/{nets[key, label]}",
                            tmp,
                        ]
                        + flags.split()
                        + ["--seed", "123", "--no-file-output", "--timing-json", tj]
                    )
                    p = subprocess.run(
                        cmd, cwd=R, capture_output=True, text=True, check=False
                    )
                    text = p.stdout + p.stderr
                    cl = re.search(r"Best codelength\s+([0-9.eE+-]+)", text)
                    ins = re.search(r"(\d+)\s+instructions retired", text)
                    with open(tj) as f:
                        total = json.load(f)["timing"]["total_s"]
                    fo.write(
                        "\t".join(
                            [
                                key,
                                label,
                                flags,
                                arm,
                                str(r),
                                str(total),
                                ins.group(1) if ins else "NA",
                                cl.group(1) if cl else "NA",
                            ]
                        )
                        + "\n"
                    )
                    fo.flush()
            print(
                f"done {key} {label} {flags} ({'changed' if changed else 'unchanged'}, sweep {100 * dt:+.1f}%)",
                flush=True,
            )


if __name__ == "__main__":
    main()
