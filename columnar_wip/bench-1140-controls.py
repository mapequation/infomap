#!/usr/bin/env python3
"""#1140 snapshot, second session: the overlapping-family rows #1158 added (F66), old vs new.
The same 37 configurations as columnar_wip/bench-om-controls.py: the planted partition under
both objectives on all fourteen networks (which also gives each block its one-level
codelength), the plain `-2d -N1` search at the density the -C -N1 table lacks, and the
regularized -N1 searches on om2_N256_E100k. Interleaved by arm, alternating which arm goes
first; three reps per row, spread across the batch.

    bench-1140-controls.py <out.tsv> <old binary> <new binary>

Row: key label arm rep flags codelength total_s instr top levels one_level load1m
Resumable: existing (label, arm, rep) rows with a codelength are skipped."""

import json
import os
import re
import subprocess
import sys
from pathlib import Path

R = Path(__file__).resolve().parent.parent
NET = "networks/columnar-benchmark"
OUT = sys.argv[1]
BIN = {"old": sys.argv[2], "new": sys.argv[3]}
TMP = f"{OUT}.tmp"
os.makedirs(TMP, exist_ok=True)

# The density each om network had before the label named it (om2 was drawn at E50000 first).
MAIN_DENSITY = {
    2: 50000,
    3: 100000,
    4: 100000,
    5: 100000,
    6: 100000,
    7: 100000,
    8: 100000,
}


def name(om, e):
    return f"om{om}_N256_E{e // 1000}k"


ROWS = []  # (label, net, flags)
for om in range(2, 9):
    for e in (50000, 100000):
        base = f"{NET}/om/overlapping-memory-n256-om{om}-nc64-E{e}-mu0.1-seed1"
        for reg in ("", " --regularized"):
            ROWS.append(
                (
                    f"{name(om, e)} planted, `-2d{reg} --no-infomap -c`",
                    f"{base}.net",
                    f"-C -2d -N1{reg} --no-infomap -c {base}.clu",
                )
            )
        if e != MAIN_DENSITY[om]:  # the -C -N1 table has -2d only at the main density
            ROWS.append((f"{name(om, e)} `-2d -N1`", f"{base}.net", "-C -2d -N1"))
        if (om, e) == (2, 100000):  # -N10 only in the main tables (F47)
            for flags in ("-2d", "-d"):
                ROWS.append(
                    (
                        f"{name(om, e)} `{flags} --regularized -N1`",
                        f"{base}.net",
                        f"-C {flags} --regularized -N1",
                    )
                )

done = set()
if os.path.exists(OUT):
    with open(OUT) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) >= 6 and p[5] != "NA":
                done.add((p[1], p[2], p[3]))


def grab(pattern, text):
    m = re.search(pattern, text, re.MULTILINE)
    return m.group(1) if m else "NA"


def run(label, net, flags, arm, rep):
    if (label, arm, str(rep)) in done:
        return
    tj = f"{TMP}/timing.json"
    if os.path.exists(tj):
        os.remove(tj)
    load = os.getloadavg()[0]
    cmd = (
        ["/usr/bin/time", "-l", BIN[arm], net, TMP]
        + flags.split()
        + ["--seed", "123", "--no-file-output", "--timing-json", tj]
    )
    p = subprocess.run(cmd, cwd=R, capture_output=True, text=True, check=False)
    out = p.stdout + p.stderr
    try:
        with open(tj) as f:
            total = json.load(f)["timing"]["total_s"]
    except (OSError, KeyError, ValueError):
        total = "NA"
    fields = [
        "FAM",
        label,
        arm,
        rep,
        flags,
        grab(r"Best codelength\s+([0-9.eE+-]+)", out),
        total,
        grab(r"(\d+)\s+instructions retired", out),
        grab(r"^\s*Top modules\s+(\d+)", out),
        grab(r"^\s*Levels\s+(\d+)", out),
        grab(r"^\s*One-level codelength\s+([0-9.eE+-]+)", out),
        f"{load:.1f}",
    ]
    with open(OUT, "a") as f:
        f.write("\t".join(map(str, fields)) + "\n")


for rep in (1, 2, 3):
    for i, (label, net, flags) in enumerate(ROWS):
        for arm in ("old", "new") if (rep + i) % 2 == 0 else ("new", "old"):
            run(label, net, flags, arm, rep)
