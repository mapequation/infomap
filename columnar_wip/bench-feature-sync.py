#!/usr/bin/env python3
"""Feature-build rows for a master sync: both engines, old and new feature binaries.

    bench-feature-sync.py <out.tsv> old=<feature binary> new=<feature binary>

Multilayer networks of bench-multilayer.py under --regularized (-N10, -2 -N10, -N1), and the
lossy map equation on jazz, netscience and lazega. -N1 rows: minimum of 3 reps, rep loop
outermost. Interleaved by arm within a row.
Row: key label engine arm rep flags codelength total_s instr top levels tree_md5
Resumable: existing (key, label, engine, arm, rep) rows are skipped.
"""

import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NB = "networks/columnar-benchmark"
ML = "networks/multilayer"
JEL = f"{ML}/Jelena/network_incomplete/synthetic_layers_5_sample_id_1_percent_removed_links"
MULTI = [
    ("multilayer_intra (ex.)", "examples/networks/multilayer_intra.net"),
    ("multilayer_intra_inter (ex.)", "examples/networks/multilayer_intra_inter.net"),
    ("malaria", f"{NB}/malaria.net"),
    ("EU airlines", f"{ML}/real-world/EUAirports/airline_europe.net"),
    ("aucs", f"{ML}/real-world/aucs/aucs.net"),
    ("StarWars", f"{ML}/real-world/StarWars/StarWars.net"),
    ("DanioRerio", f"{ML}/real-world/DanioRerio/danioRerio_genetic_multiplex.net"),
    ("Jelena synthetic 0%", f"{JEL}0.net"),
    ("Jelena synthetic 50%", f"{JEL}50.net"),
]
LOSSY = [
    ("jazz", f"{NB}/jazz.txt"),
    ("netscience", f"{NB}/netscience.txt"),
    ("lazega", f"{NB}/lazega.net"),
]

ROWS = []  # key, label, engine, input, flags
for label, net in MULTI:
    for key, flags in (
        ("ML10", "--regularized -N10"),
        ("ML2_10", "--regularized -2 -N10"),
        ("ML1", "--regularized -N1"),
    ):
        for engine in ("C", "OO"):
            ROWS.append((key, label, engine, net, flags))
for label, net in LOSSY:
    for key, flags in (
        ("LO10", "--lossy -N10"),
        ("LO10l", "--lossy --lambda 0.5 -N10"),
    ):
        ROWS.append((key, label, "C", net, flags))


def grab(pattern, text):
    m = re.search(pattern, text, re.MULTILINE)
    return m.group(1) if m else "NA"


def tree_md5(out):
    h = hashlib.md5()
    for name in sorted(os.listdir(out)):
        if name.endswith(".tree"):
            with open(os.path.join(out, name)) as f:
                for line in f:
                    if not line.startswith("#"):
                        h.update(line.encode())
    return h.hexdigest()[:12]


def run(binary, net, engine, flags):
    with tempfile.TemporaryDirectory() as out:
        tj = os.path.join(out, "timing.json")
        sj = os.path.join(out, "summary.json")
        args = (["-C"] if engine == "C" else []) + flags.split()
        cmd = [
            "/usr/bin/time",
            "-l",
            binary,
            f"{R}/{net}",
            out,
            *args,
            "--seed",
            "123",
            "-o",
            "tree",
            "--timing-json",
            tj,
            "--summary-json",
            sj,
        ]
        p = subprocess.run(cmd, cwd=R, capture_output=True, text=True, check=False)
        text = p.stdout + p.stderr
        try:
            with open(tj) as f:
                total = json.load(f)["timing"]["total_s"]
            with open(sj) as f:
                cl = f"{json.load(f)['codelength']:.10g}"
        except (OSError, KeyError, ValueError):
            total, cl = "NA", "NA"
        top = grab(r"^\s*Top modules\s+(\d+)", text)
        levels = grab(r"^\s*Levels\s+(\d+)", text)
        instr = grab(r"^\s*(\d+)\s+instructions retired", text)
        return cl, total, instr, top, levels, tree_md5(out)


def main():
    out_tsv = sys.argv[1]
    arms = dict(a.split("=", 1) for a in sys.argv[2:])
    done = set()
    if os.path.exists(out_tsv):
        with open(out_tsv) as f:
            for line in f:
                c = line.rstrip("\n").split("\t")
                if len(c) > 6 and c[6] != "NA":
                    done.add(tuple(c[:5]))
    with open(out_tsv, "a") as f:
        for rep in (1, 2, 3):
            for key, label, engine, net, flags in ROWS:
                if rep > 1 and "-N1" not in flags.split():
                    continue
                for arm in ("old", "new"):
                    if (key, label, engine, arm, str(rep)) in done:
                        continue
                    res = run(arms[arm], net, engine, flags)
                    f.write(
                        "\t".join(
                            [key, label, engine, arm, str(rep), flags, *map(str, res)]
                        )
                        + "\n"
                    )
                    f.flush()


if __name__ == "__main__":
    main()
