#!/usr/bin/env python3
"""Measure the rows the overlapping-family table gains when every search is paired with its
planted control (F66): the planted partition under both objectives on all fourteen networks,
the plain `-2d -N1` search where no table had it yet, and the regularized -N1 searches on
om2 E100000, which had -N10 rows only. One binary, the tip of
columnar-hierarchical-core; three reps per row, spread across the batch.

The seven plain planted rows the family table already had are measured again: their bits
must reproduce the carried ones (same binary, same input), and their one-level codelength
is what the table's planted-vs-one-level check needs.

    bench-om-controls.py <session.tsv>

Row: key label arm rep flags codelength total_s instr top levels one_level state_nodes
Resumable: existing (label, rep) rows with a codelength are skipped. Each configuration is
also appended to columnar-search-runs.tsv as one row (minimum of the reps).
"""

import datetime
import json
import os
import re
import subprocess
import sys

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = "/private/tmp/claude-501/-Users-daniel-dev-projects-icelab-code-infomap-Infomap/dace3e0c-8169-4611-9793-804ea28f1a84/scratchpad"
BIN = f"{S}/Infomap-501f523a"  # columnar-hierarchical-core tip 501f523a, md5 54c8b6b2
MD5 = "54c8b6b21ab1b54dd3931a9c7f79833e"
COMMIT = "501f523a"
NEW = "networks/columnar-benchmark"
OUT = sys.argv[1]
TMP = f"{S}/bench-om-controls-tmp"
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


ROWS = []  # (label, net, flags, run-log network, objective)
for om in range(2, 9):
    for e in (50000, 100000):
        base = f"{NEW}/om/overlapping-memory-n256-om{om}-nc64-E{e}-mu0.1-seed1"
        for reg in ("", " --regularized"):
            ROWS.append(
                (
                    f"{name(om, e)} planted, `-2d{reg} --no-infomap -c`",
                    f"{base}.net",
                    f"-C -2d -N1{reg} --no-infomap -c {base}.clu",
                    f"om{om}-E{e} planted{'-reg' if reg else ''}",
                    "state/memory + regularized" if reg else "state/memory",
                )
            )
        if e != MAIN_DENSITY[om]:  # the -C -N1 table has -2d only at the main density
            ROWS.append(
                (
                    f"{name(om, e)} `-2d -N1`",
                    f"{base}.net",
                    "-C -2d -N1",
                    f"om{om}-E{e} search-2d-N1",
                    "state/memory",
                )
            )
        if (om, e) == (2, 100000):  # added at -N10 only (F47): no -N1 regularized rows
            for flags in ("-2d", "-d"):
                ROWS.append(
                    (
                        f"{name(om, e)} `{flags} --regularized -N1`",
                        f"{base}.net",
                        f"-C {flags} --regularized -N1",
                        f"om{om}-E{e} search{flags}-reg-N1",
                        "state/memory + regularized",
                    )
                )

done = set()
if os.path.exists(OUT):
    with open(OUT) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) >= 6 and p[5] != "NA":
                done.add((p[1], p[3]))


def grab(pattern, text):
    m = re.search(pattern, text, re.MULTILINE)
    return m.group(1) if m else "NA"


def run(label, net, flags, rep):
    if (label, str(rep)) in done:
        return
    tj = f"{TMP}/timing.json"
    if os.path.exists(tj):
        os.remove(tj)
    cmd = (
        ["/usr/bin/time", "-l", BIN, f"{R}/{net}", TMP]
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
        "new",
        rep,
        flags,
        grab(r"Best codelength\s+([0-9.eE+-]+)", out),
        total,
        grab(r"(\d+)\s+instructions retired", out),
        grab(r"^\s*Top modules\s+(\d+)", out),
        grab(r"^\s*Levels\s+(\d+)", out),
        grab(r"^\s*One-level codelength\s+([0-9.eE+-]+)", out),
        grab(r"^\s*State nodes\s+(\d+)", out),
    ]
    with open(OUT, "a") as f:
        f.write("\t".join(map(str, fields)) + "\n")


started = datetime.datetime.now(datetime.UTC).strftime("%Y-%m-%dT%H:%M:%SZ")
for rep in (1, 2, 3):
    for label, net, flags, _, _ in ROWS:
        run(label, net, flags, rep)

# One run-log row per configuration: bits of rep 1, minimum seconds and instructions.
reps = {}
with open(OUT) as f:
    for line in f:
        p = line.rstrip("\n").split("\t")
        if len(p) >= 12 and p[5] != "NA":
            reps.setdefault(p[1], []).append(p)
load = f"{os.getloadavg()[0]:.0f}"
with open(f"{R}/columnar_wip/columnar-search-runs.tsv", "a") as f:
    for label, _, flags, network, objective in ROWS:
        rs = sorted(reps[label], key=lambda p: int(p[3]))
        first = rs[0]
        log_flags = re.sub(r"-c \S+", "-c planted", flags)
        notes = (
            f"F66 planted controls for the om family (one-level {first[10]}); "
            "engine timing.total_s; datetime = batch start"
        )
        row = [
            "om-controls",
            started,
            load,
            len(rs),
            "min",
            MD5,
            COMMIT,
            "F66",
            network,
            first[11],
            objective,
            log_flags,
            "columnar",
            f"tip-{COMMIT}",
            "",
            1,
            123,
            "none",
            "",
            first[5],
            min(float(p[6]) for p in rs),
            "",
            first[8],
            first[9],
            0,
            notes,
            min(int(p[7]) for p in rs),
        ]
        f.write("\t".join(map(str, row)) + "\n")
with open(OUT, "a") as f:
    f.write("BENCH-DONE\n")
