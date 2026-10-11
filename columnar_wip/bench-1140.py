#!/usr/bin/env python3
"""#1140 snapshot benchmark: old (branch tip 501f523a) vs new (the objective-option warnings
ask the engine that runs) over every configuration of the #1136 snapshot, on the fetched
inputs (columnar_wip/fetch-benchmark-networks.py). The change is a run-path diagnostic, so
every row is measured to show that nothing else moves. No object-oriented arm: the OO engine
is untouched. Interleaved by arm, alternating which arm goes first; -N1 rows as 3 reps spread
across the batch.

    bench-1140.py <out.tsv> <old binary> <new binary> [<rep> <old|new> <key> <label> ...]

Row: key label arm rep flags codelength total_s instr top levels load1m
Resumable: existing (key, label, arm, rep) rows are skipped. Extra arguments re-measure one
configuration as additional reps (an outlier, as the minimum of more runs)."""

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

BASE = [
    ("ninetriangles", f"{NET}/ninetriangles.net", ""),
    ("jazz", f"{NET}/jazz.txt", ""),
    ("netscience", f"{NET}/netscience.txt", ""),
    ("powergrid", f"{NET}/powergrid.txt", ""),
    ("polblogs", f"{NET}/polblogs.txt", "-d"),
    ("word_assoc", f"{NET}/word_assoc.txt", "-d"),
    ("web-NotreDame", f"{NET}/web-NotreDame.txt", "-d"),
    ("lazega", f"{NET}/lazega.net", f"--meta-data {NET}/lazega.meta"),
    ("multilayer (ex.)", f"{NET}/multilayer.net", ""),
    ("malaria", f"{NET}/malaria.net", ""),
    ("air30k", f"{NET}/air30k.net", ""),
    ("air30k (reg.)", f"{NET}/air30k.net", "-d --regularized"),
    ("air30k (meta)", f"{NET}/air30k.net", f"--meta-data {NET}/air30k_usstate.meta"),
    (
        "word_assoc (pref.)",
        f"{NET}/word_assoc.txt",
        "-d --preferred-number-of-modules 25",
    ),
]
# The density each om runs at in the main tables (F55); the other one is the family's.
MAIN_DENSITY = {
    2: 50000,
    3: 100000,
    4: 100000,
    5: 100000,
    6: 100000,
    7: 100000,
    8: 100000,
}
WIKI = f"{NET}/wikispeedia_states.net"


def om_path(om, e, ext):
    return f"{NET}/om/overlapping-memory-n256-om{om}-nc64-E{e}-mu0.1-seed1.{ext}"


BOTH = ("old", "new")
# (key, label, net, flags, arms, reps)
configs = []
for label, net, fl in BASE:
    configs.append(("C10", label, net, f"-C -N10 {fl}".strip(), BOTH, 1))
    configs.append(("C2_10", label, net, f"-C -2 -N10 {fl}".strip(), BOTH, 1))
    configs.append(("C1", label, net, f"-C -N1 {fl}".strip(), BOTH, 3))
    configs.append(("F10", label, net, f"-C -F -N10 {fl}".strip(), ("new",), 1))
    configs.append(
        ("L10", label, net, f"-C --non-redundant -N10 {fl}".strip(), ("new",), 1)
    )
for om, e in MAIN_DENSITY.items():
    net, clu = om_path(om, e, "net"), om_path(om, e, "clu")
    lab, fam = f"overlapping om{om}", f"om{om}"
    configs.append(("C10", f"{lab} `-d`", net, "-C -d -N10", BOTH, 1))
    configs.append(("C2_10", f"{lab} `-2d`", net, "-C -2d -N10", BOTH, 1))
    configs.append(("C1", f"{lab} `-2d`", net, "-C -2d -N1", BOTH, 3))
    configs.append(("C1", f"{lab} `-d`", net, "-C -d -N1", BOTH, 3))
    configs.append(
        ("C1", f"{lab} `-2d -c` planted", net, f"-C -2d -N1 -c {clu}", BOTH, 3)
    )
    for shape in ("-2d", "-d"):
        for n, reps in (("-N1", 3), ("-N10", 1)):
            configs.append(
                (
                    "FAM",
                    f"{fam} `{shape} --regularized {n}`",
                    net,
                    f"-C {shape} --regularized {n}",
                    BOTH,
                    reps,
                )
            )
    configs.append(
        (
            "FAM",
            f"{fam} planted, `-2d --no-infomap -c`",
            net,
            f"-C -2d -N1 --no-infomap -c {clu}",
            BOTH,
            3,
        )
    )
    configs.append(("F10", f"{lab} `-d`", net, "-C -F -d -N10", BOTH, 1))
    configs.append(("F1", f"{lab} `-d`", net, "-C -F -d -N1", BOTH, 3))
# The other trigram density of every om (F55).
for om, main in MAIN_DENSITY.items():
    e = 100000 if main == 50000 else 50000
    net = om_path(om, e, "net")
    lab, fam = f"overlapping om{om} E{e}", f"om{om} E{e}"
    for shape in ("-2d", "-d"):
        configs.append(
            (
                "FAM",
                f"{fam} `{shape} --regularized -N10`",
                net,
                f"-C {shape} --regularized -N10",
                BOTH,
                1,
            )
        )
    configs.append(("C10", f"{lab} `-d`", net, "-C -d -N10", BOTH, 1))
    configs.append(("C2_10", f"{lab} `-2d`", net, "-C -2d -N10", BOTH, 1))
    configs.append(("C1", f"{lab} `-d`", net, "-C -d -N1", BOTH, 3))
    if e == 50000:
        for shape in ("-2d", "-d"):
            configs.append(
                (
                    "FAM",
                    f"{fam} `{shape} --regularized -N1`",
                    net,
                    f"-C {shape} --regularized -N1",
                    BOTH,
                    3,
                )
            )
configs.append(("C10", "wikispeedia `-d`", WIKI, "-C -d -N10", BOTH, 1))
configs.append(("C2_10", "wikispeedia `-2d`", WIKI, "-C -2d -N10", BOTH, 1))
configs.append(("C1", "wikispeedia `-2d`", WIKI, "-C -2d -N1", BOTH, 3))
configs.append(("C1", "wikispeedia `-d`", WIKI, "-C -d -N1", BOTH, 3))
configs.append(("F10", "wikispeedia `-d`", WIKI, "-C -F -d -N10", ("new",), 1))
configs.append(
    ("L10", "wikispeedia `-d`", WIKI, "-C --non-redundant -d -N10", ("new",), 1)
)

done = set()
if os.path.exists(OUT):
    with open(OUT) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            # Only rows that captured a codelength count as done, so a later run retries NA.
            if len(p) >= 6 and p[5] != "NA":
                done.add((p[0], p[1], p[2], p[3]))


def run(key, label, net, flags, arm, rep):
    if (key, label, arm, str(rep)) in done:
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
    row = [key, label, arm, rep, flags, cl, total, instr, top, lv, f"{load:.1f}"]
    with open(OUT, "a") as f:
        f.write("\t".join(map(str, row)) + "\n")
    if cl == "NA":
        with open(OUT + ".err", "a") as f:
            f.write(
                f"### {key} {label} {arm} {rep} :: {' '.join(cmd)}\n{out[-3000:]}\n"
            )


def arms_in_turn(arms, turn):
    return arms if turn % 2 == 0 else tuple(reversed(arms))


if len(sys.argv) > 4:
    # Re-measure: <rep> <old|new> <key> <label>, e.g. an outlier's extra reps.
    rep, arm, key, label = sys.argv[4:8]
    (cfg,) = [c for c in configs if c[0] == key and c[1] == label]
    run(cfg[0], cfg[1], cfg[2], cfg[3], arm, int(rep))
    sys.exit(0)

n1 = [c for c in configs if c[5] == 3]
n10 = [c for c in configs if c[5] == 1]
chunks = [n10[i::3] for i in range(3)]  # three interleaved slices of the -N10 work
for rep in (1, 2, 3):
    for i, (key, label, net, flags, arms, _) in enumerate(n1):
        for arm in arms_in_turn(arms, rep + i):
            run(key, label, net, flags, arm, rep)
    for i, (key, label, net, flags, arms, _) in enumerate(chunks[rep - 1]):
        for arm in arms_in_turn(arms, rep + i):
            run(key, label, net, flags, arm, 1)
with open(OUT, "a") as f:
    f.write("BENCH-DONE\n")
