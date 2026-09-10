#!/usr/bin/env python3
"""Hierarchical-gaps snapshot benchmark (#1041): old (branch tip) vs new (escalation-completed
flat pipeline + first-trial one-level rescue) over every configuration
the snapshot tables need, plus the OO arm (new binary, no -C) for the OO-vs-columnar tables. Interleaved by arm; -N1 rows as 3 reps spread across the batch.
Row: key label arm rep flags codelength total_s instr top levels
Resumable: existing (key, label, arm, rep) rows are skipped."""

import json
import os
import re
import subprocess
import sys

R = "/Users/daniel/dev/projects/icelab/code/infomap/Infomap"
S = "/private/tmp/claude-501/-Users-daniel-dev-projects-icelab-code-infomap-Infomap/2e5e5993-267c-4b29-9563-8cca6b12bc3b/scratchpad"
BIN = {
    "old": f"{S}/Infomap-old-a02dc105",  # fresh build at the branch tip a02dc105
    "new": f"{R}/.claude/worktrees/omfam/Infomap",
}
OUT = sys.argv[1]
TMP = "/tmp/bench-hier-gaps"
os.makedirs(TMP, exist_ok=True)

BASE = [
    ("ninetriangles", "examples/networks/ninetriangles.net", ""),
    ("jazz", "networks/arenas-jazz.txt", ""),
    ("netscicoauthor2010", "networks/db/netscicoauthor2010.net", ""),
    ("powergrid", "networks/powergrid.txt", ""),
    ("politicalblogs", "networks/db/politicalblogs.net", "-d"),
    ("science2001", "networks/db/science2001.net", "-d"),
    ("web-NotreDame", "networks/db/web-NotreDame.net", "-d"),
    ("lazega", "networks/meta/lazega.net", "--meta-data networks/meta/lazega.meta"),
    ("multilayer (ex.)", "examples/networks/multilayer.net", ""),
    (
        "malaria",
        "networks/multilayer/real-world/malaria/malaria_PLOSCompBiology_2013.net",
        "",
    ),
    ("air30k", "networks/states/air2011/air30k.net", ""),
    ("air30k (reg.)", "networks/states/air2011/air30k.net", "-d --regularized"),
    (
        "air30k (meta)",
        "networks/states/air2011/air30k.net",
        "--meta-data networks/states/air2011/air30k_usstate.meta",
    ),
    (
        "science2001 (pref.)",
        "networks/db/science2001.net",
        "-d --preferred-number-of-modules 25",
    ),
]
OM = [
    ("om2", "E50000"),
    ("om3", "E100000"),
    ("om4", "E100000"),
    ("om5", "E100000"),
    ("om6", "E100000"),
    ("om7", "E100000"),
    ("om8", "E100000"),
]
WIKI = "/Users/daniel/dev/projects/icelab/code/networks/examples/wikispeedia_states.net"

# (key, label, net, flags, arms, reps)
configs = []
for label, net, fl in BASE:
    configs.append(("C10", label, net, f"-C -N10 {fl}", ("old", "new"), 1))
    configs.append(("C2_10", label, net, f"-C -2 -N10 {fl}", ("old", "new"), 1))
    # No object-oriented arm: this PR does not touch that engine, its rows are carried
    # from the #1078 snapshot session (same machine, same instrument).
    configs.append(("C1", label, net, f"-C -N1 {fl}", ("old", "new"), 3))
    configs.append(("F10", label, net, f"-C -F -N10 {fl}", ("new",), 1))
    configs.append(("L10", label, net, f"-C --non-redundant -N10 {fl}", ("new",), 1))
for om, e in OM:
    net = f"networks/debug/Jelena/network_N256_{om}_nc64_{e}_mu10_sample1.net"
    clu = f"networks/debug/Jelena/planted_partition_N256_{om}_nc64_{e}_mu10_sample1.clu"
    lab = f"overlapping {om}"
    configs.append(("C10", f"{lab} `-d`", net, "-C -d -N10", ("old", "new"), 1))
    configs.append(("C2_10", f"{lab} `-2d`", net, "-C -2d -N10", ("old", "new"), 1))
    configs.append(("C1", f"{lab} `-2d`", net, "-C -2d -N1", ("old", "new"), 3))
    configs.append(("C1", f"{lab} `-d`", net, "-C -d -N1", ("old", "new"), 3))
    configs.append(
        (
            "C1",
            f"{lab} `-2d -c` planted",
            net,
            f"-C -2d -N1 -c {clu}",
            ("old", "new"),
            3,
        )
    )
    configs.append(
        (
            "FAM",
            f"{om} `-2d --regularized -N1`",
            net,
            "-C -2d --regularized -N1",
            ("old", "new"),
            3,
        )
    )
    configs.append(
        (
            "FAM",
            f"{om} `-2d --regularized -N10`",
            net,
            "-C -2d --regularized -N10",
            ("old", "new"),
            1,
        )
    )
    configs.append(
        (
            "FAM",
            f"{om} `-d --regularized -N1`",
            net,
            "-C -d --regularized -N1",
            ("old", "new"),
            3,
        )
    )
    configs.append(
        (
            "FAM",
            f"{om} `-d --regularized -N10`",
            net,
            "-C -d --regularized -N10",
            ("old", "new"),
            1,
        )
    )
    configs.append(
        (
            "FAM",
            f"{om} planted, `-2d --no-infomap -c`",
            net,
            f"-C -2d -N1 --no-infomap -c {clu}",
            ("old", "new"),
            3,
        )
    )
    # -F shares the gate and the rescue (optimizeFlexible), so the family runs it too.
    configs.append(("F10", f"{lab} `-d`", net, "-C -F -d -N10", ("old", "new"), 1))
    configs.append(("F1", f"{lab} `-d`", net, "-C -F -d -N1", ("old", "new"), 3))
# The other trigram density of every om (F55): the regularized rows, where om2 used to fail at
# E50000 and om4/om5 at E100000, plus the hierarchical -N1 row the rescue targets.
for om, e in [("om2", "E100000")] + [(f"om{i}", "E50000") for i in (3, 4, 5, 6, 7, 8)]:
    net = f"networks/debug/Jelena/network_N256_{om}_nc64_{e}_mu10_sample1.net"
    lab = f"overlapping {om} {e}"
    configs.append(
        (
            "FAM",
            f"{om} {e} `-2d --regularized -N10`",
            net,
            "-C -2d --regularized -N10",
            ("old", "new"),
            1,
        )
    )
    configs.append(
        (
            "FAM",
            f"{om} {e} `-d --regularized -N10`",
            net,
            "-C -d --regularized -N10",
            ("old", "new"),
            1,
        )
    )
    configs.append(("C10", f"{lab} `-d`", net, "-C -d -N10", ("old", "new"), 1))
    configs.append(("C2_10", f"{lab} `-2d`", net, "-C -2d -N10", ("old", "new"), 1))
    configs.append(("C1", f"{lab} `-d`", net, "-C -d -N1", ("old", "new"), 3))
configs.append(("C10", "wikispeedia `-d`", WIKI, "-C -d -N10", ("old", "new"), 1))
configs.append(("C2_10", "wikispeedia `-2d`", WIKI, "-C -2d -N10", ("old", "new"), 1))
configs.append(("C1", "wikispeedia `-2d`", WIKI, "-C -2d -N1", ("old", "new"), 3))
configs.append(("C1", "wikispeedia `-d`", WIKI, "-C -d -N1", ("old", "new"), 3))
configs.append(("F10", "wikispeedia `-d`", WIKI, "-C -F -d -N10", ("new",), 1))
configs.append(
    ("L10", "wikispeedia `-d`", WIKI, "-C --non-redundant -d -N10", ("new",), 1)
)

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
    netpath = net if net.startswith("/") else f"{R}/{net}"
    tj = f"{TMP}/timing.json"
    if os.path.exists(tj):
        os.remove(tj)
    cmd = (
        ["/usr/bin/time", "-l", BIN[arm], netpath, TMP]
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


n1 = [c for c in configs if c[5] == 3]
n10 = [c for c in configs if c[5] == 1]
chunks = [n10[i::3] for i in range(3)]  # three interleaved slices of the -N10 work
for rep in (1, 2, 3):
    for key, label, net, flags, arms, _ in n1:
        for arm in arms:
            run(key, label, net, flags, arm, rep)
    for key, label, net, flags, arms, _ in chunks[rep - 1]:
        for arm in arms:
            run(key, label, net, flags, arm, 1)
with open(OUT, "a") as f:
    f.write("BENCH-DONE\n")
