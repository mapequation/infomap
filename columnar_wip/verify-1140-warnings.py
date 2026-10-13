#!/usr/bin/env python3
"""#1140: do the objective-option warnings fire exactly when the option leaves the codelength
unchanged? Runs each benchmark network without and with --entropy-corrected and
--preferred-number-of-modules 5, on the object-oriented engine and on -C, plus -C on the
old binary to show what warned before the fix.

    verify-1140-warnings.py <out.tsv> <old binary> <new binary>

Row: label input engine variant codelength top warned_entropy warned_preferred total_s
"""

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
OM = f"{NET}/om/overlapping-memory-n256-om4-nc64-E100000-mu0.1-seed1"

# (label, input class, network, flags, seed)
NETWORKS = [
    ("jazz", "ordinary", f"{NET}/jazz.txt", "", 123),
    ("netscience", "ordinary", f"{NET}/netscience.txt", "", 123),
    ("powergrid", "ordinary", f"{NET}/powergrid.txt", "", 123),
    ("polblogs", "ordinary", f"{NET}/polblogs.txt", "-d", 123),
    ("word_assoc", "ordinary", f"{NET}/word_assoc.txt", "-d", 123),
    ("web-NotreDame", "ordinary", f"{NET}/web-NotreDame.txt", "-d", 123),
    (
        "lazega",
        "ordinary + meta-data",
        f"{NET}/lazega.net",
        f"--meta-data {NET}/lazega.meta",
        123,
    ),
    ("malaria", "multilayer", f"{NET}/malaria.net", "", 123),
    ("air30k", "state", f"{NET}/air30k.net", "", 123),
    ("air30k (reg.)", "state", f"{NET}/air30k.net", "-d --regularized", 123),
    # Seed 456: OO does not finish this row on seed 123 (#1134).
    (
        "air30k (meta)",
        "state + meta-data",
        f"{NET}/air30k.net",
        f"--meta-data {NET}/air30k_usstate.meta",
        456,
    ),
    ("wikispeedia", "state", f"{NET}/wikispeedia_states.net", "-d", 123),
    ("om4 E100000", "state", f"{OM}.net", "-2d", 123),
    # Scoring rather than searching: a planted partition is scored on the columnar stack,
    # no partition at all is the module-less fallback to the object-oriented objective.
    (
        "om4 E100000 planted, --no-infomap -c",
        "state",
        f"{OM}.net",
        f"-2d --no-infomap -c {OM}.clu",
        123,
    ),
    (
        "air30k, --no-infomap (no partition)",
        "state",
        f"{NET}/air30k.net",
        "--no-infomap",
        123,
    ),
]
VARIANTS = {
    "base": "",
    "entropy": "--entropy-corrected",
    "preferred": "--preferred-number-of-modules 5",
}
ENGINES = {  # engine -> (binary, flags, variants)
    "OO": ("new", "", VARIANTS),
    "-C": ("new", "-C", VARIANTS),
    "-C old": ("old", "-C", {k: v for k, v in VARIANTS.items() if k != "base"}),
}

done = set()
if os.path.exists(OUT):
    with open(OUT) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) >= 5 and p[4] != "NA":
                done.add((p[0], p[2], p[3]))

for label, kind, net, flags, seed in NETWORKS:
    for engine, (arm, eflags, variants) in ENGINES.items():
        for variant, vflags in variants.items():
            if (label, engine, variant) in done:
                continue
            tj = f"{TMP}/timing.json"
            if os.path.exists(tj):
                os.remove(tj)
            cmd = (
                [BIN[arm], net, TMP]
                + f"{eflags} {flags} {vflags}".split()
                + ["-N1", "--seed", str(seed), "--no-file-output", "--timing-json", tj]
            )
            try:
                p = subprocess.run(
                    cmd, cwd=R, capture_output=True, text=True, timeout=900, check=False
                )
                out = p.stdout + p.stderr
            except subprocess.TimeoutExpired:
                out = ""
            m = re.search(r"Best codelength\s+([0-9.eE+-]+)", out)
            cl = m.group(1) if m else "NA"
            m = re.search(r"^\s*Top modules\s+(\d+)", out, re.MULTILINE)
            top = m.group(1) if m else "NA"
            we = int("--entropy-corrected has no effect" in out)
            wp = int("--preferred-number-of-modules has no effect" in out)
            try:
                with open(tj) as f:
                    total = json.load(f)["timing"]["total_s"]
            except (OSError, KeyError, ValueError):
                total = "NA"
            row = [label, kind, engine, variant, cl, top, we, wp, total]
            with open(OUT, "a") as f:
                f.write("\t".join(map(str, row)) + "\n")
            print("\t".join(map(str, row)), flush=True)
