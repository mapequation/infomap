#!/usr/bin/env python3
"""Snapshot benchmark for the fetched benchmark inputs: one binary, the old input file against
the one columnar_wip/fetch-benchmark-networks.py writes, on every row whose input changed. The
configurations are the #1127 snapshot's (columnar_wip/1127-ab-results.tsv), so the labels and
flags line up with its tables; rows whose input is byte-identical are not run here (their
numbers are carried from that TSV, measured with the same binary). The object-oriented arm runs
on the new input of each changed row of the two OO tables. Interleaved by arm; -N1 rows as 3
reps spread across the batch.
Row: key label arm rep flags codelength total_s instr top levels
Resumable: existing (key, label, arm, rep) rows are skipped."""

import json
import os
import re
import subprocess
import sys

R = "/Users/daniel/dev/projects/icelab/code/infomap/Infomap"
S = "/private/tmp/claude-501/-Users-daniel-dev-projects-icelab-code-infomap-Infomap/56577666-af50-4930-949b-755e6a48b7f8/scratchpad"
BIN = f"{S}/Infomap-c9ce210b"  # columnar-hierarchical-core tip c9ce210b, md5 54c8b6b2
NEW = "networks/columnar-benchmark"  # fetch-benchmark-networks.py output
OUT = sys.argv[1]
TMP = f"{S}/bench-inputs-tmp"
os.makedirs(TMP, exist_ok=True)

AIR_META = "networks/states/air2011/air30k_usstate.meta"
BASE = {  # label -> (old input, new input, {old flag path: new flag path})
    "jazz": ("networks/arenas-jazz.txt", f"{NEW}/jazz.txt", {}),
    "powergrid": ("networks/powergrid.txt", f"{NEW}/powergrid.txt", {}),
    "web-NotreDame": ("networks/db/web-NotreDame.net", f"{NEW}/web-NotreDame.txt", {}),
    "malaria": (
        "networks/multilayer/real-world/malaria/malaria_PLOSCompBiology_2013.net",
        f"{NEW}/malaria.net",
        {},
    ),
    "air30k": ("networks/states/air2011/air30k.net", f"{NEW}/air30k.net", {}),
    "air30k (reg.)": ("networks/states/air2011/air30k.net", f"{NEW}/air30k.net", {}),
    "air30k (meta)": (
        "networks/states/air2011/air30k.net",
        f"{NEW}/air30k.net",
        {AIR_META: f"{NEW}/air30k_usstate.meta"},
    ),
}
MAIN_DENSITY = {
    2: 50000,
    3: 100000,
    4: 100000,
    5: 100000,
    6: 100000,
    7: 100000,
    8: 100000,
}
# The object-oriented rows of the two OO tables whose input changed (air30k (meta) is -N1 there).
OO = {
    "O10": {lb: "-N10" for lb in BASE if lb != "air30k (meta)"}
    | {"air30k (meta)": "-N1"},
    "O2_10": {lb: "-2 -N10" for lb in BASE if lb != "air30k (meta)"},
}
OO_FLAGS = {"web-NotreDame": "-d", "air30k (reg.)": "-d --regularized"}
OO_FLAGS["air30k (meta)"] = f"--meta-data {NEW}/air30k_usstate.meta"


def inputs(label):
    """(old net, new net, flag substitutions) for a label whose input changed, else None."""
    if label in BASE:
        return BASE[label]
    m = re.match(r"(?:overlapping )?om(\d)(?: E(\d+))?\b", label)
    if not m:
        return None
    om = int(m.group(1))
    e = int(m.group(2)) if m.group(2) else MAIN_DENSITY[om]
    old = f"networks/debug/Jelena/network_N256_om{om}_nc64_E{e}_mu10_sample1.net"
    new = f"{NEW}/om/overlapping-memory-n256-om{om}-nc64-E{e}-mu0.1-seed1.net"
    old_clu = f"networks/debug/Jelena/planted_partition_N256_om{om}_nc64_E{e}_mu10_sample1.clu"
    return old, new, {old_clu: new[: -len(".net")] + ".clu"}


configs = {}  # (key, label) -> [flags, arms, reps]
with open(f"{R}/columnar_wip/1127-ab-results.tsv") as f:
    for line in f:
        p = line.rstrip("\n").split("\t")
        if len(p) < 10 or inputs(p[1]) is None:
            continue
        c = configs.setdefault((p[0], p[1]), [p[4].strip(), set(), set()])
        c[1].add(p[2])
        c[2].add(int(p[3]))

done = set()
if os.path.exists(OUT):
    with open(OUT) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            # A row counts as done only if it actually captured a codelength; NA rows
            # (e.g. a bad path) are left out so a later invocation retries them.
            if len(p) >= 6 and p[5] != "NA":
                done.add((p[0], p[1], p[2], p[3]))


def run(key, label, net, flags, arm, rep):
    if (key, label, arm, str(rep)) in done:
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
    m = re.search(r"Best codelength\s+([0-9.eE+-]+)", out)
    cl = m.group(1) if m else "NA"
    m = re.search(r"^\s*Top modules\s+(\d+)", out, re.MULTILINE)
    top = m.group(1) if m else "NA"
    m = re.search(r"^\s*Levels\s+(\d+)", out, re.MULTILINE)
    lv = m.group(1) if m else "NA"
    m = re.search(r"(\d+)\s+instructions retired", out)
    instr = m.group(1) if m else "NA"
    try:
        with open(tj) as f:
            total = json.load(f)["timing"]["total_s"]
    except (OSError, KeyError, ValueError):
        total = "NA"
    with open(OUT, "a") as f:
        f.write(
            "\t".join(
                map(str, [key, label, arm, rep, flags, cl, total, instr, top, lv])
            )
            + "\n"
        )
    if cl == "NA":
        with open(OUT + ".err", "a") as f:
            f.write(
                f"### {key} {label} {arm} {rep} :: {' '.join(cmd)}\n{out[-3000:]}\n"
            )


def columnar(key, label, rep):
    flags, arms, _ = configs[key, label]
    old, new, subst = inputs(label)
    for arm in ("old", "new"):
        if arm == "old" and "old" not in arms:
            continue
        net, fl = (old, flags) if arm == "old" else (new, flags)
        if arm == "new":
            for a, b in subst.items():
                fl = fl.replace(a, b)
        run(key, label, net, fl, arm, rep)


def oo(key, label):
    if (key, label) == ("O10", "air30k (meta)"):
        return  # killed after 9 h 25 min on the first pass and not retried: #1134
    flags = f"{OO[key][label]} {OO_FLAGS.get(label, '')}".strip()
    run(key, label, BASE[label][1], flags, "oo", 1)


n1 = [k for k, c in configs.items() if len(c[2]) == 3]
# Everything else is -N10, run once; four of those had 4 reps in #1127 (its re-measures).
n10 = [k for k, c in configs.items() if len(c[2]) != 3]
n10 += [(key, lb) for key in OO for lb in OO[key]]
chunks = [n10[i::3] for i in range(3)]  # three interleaved slices of the -N10 work
for rep in (1, 2, 3):
    for key, label in n1:
        columnar(key, label, rep)
    for key, label in chunks[rep - 1]:
        if key in OO:
            oo(key, label)
        else:
            columnar(key, label, 1)
# Re-measured after the first pass, as #1127 did with its outliers: the two cells that read
# slowest in seconds against unchanged instructions, as reps 2-4 (the tables take the minimum).
REMEASURE = [("C2_10", "powergrid"), ("FAM", "om4 E50000 `-2d --regularized -N10`")]
for rep in (2, 3, 4):
    for key, label in REMEASURE:
        columnar(key, label, rep)
with open(OUT, "a") as f:
    f.write("BENCH-DONE\n")
