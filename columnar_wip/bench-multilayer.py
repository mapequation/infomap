#!/usr/bin/env python3
"""Regularized-multilayer table (#828): the columnar engine before and after the per-layer
teleport prior, against the object-oriented engine, on multilayer input under --regularized.

    bench-multilayer.py <out.tsv> old=<feature binary> new=<feature binary> [--table]

Both binaries must be feature builds (FEATURES="... regularized-multilayer"): without it,
--regularized does not apply to multilayer input. The OO arm runs on the new binary, since the
change touches only the columnar engine.

Before the change the columnar engine optimized and reported a codelength without the prior, so
its reported number is not the objective. Each old columnar partition is therefore also scored by
the OO engine (`--no-infomap -c <states tree>`), which prices the prior: that is the old arm's real
codelength, the one the new arm and OO are compared against.

Every configuration runs as -N10 and -2 -N10 once, and -N1 as the minimum of 3 reps spread across
the batch, interleaved by arm within a row.

Row: label flags arm rep codelength total_s instr top levels
Arms: old (old -C, reported), old_scored (old -C partition under OO), new (new -C), oo.
--table prints the HTML table from an existing TSV instead of running.
"""

import json
import os
import re
import subprocess
import sys
import tempfile
from collections import defaultdict

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NB = "networks/columnar-benchmark"
ML = "networks/multilayer"
JEL = f"{ML}/Jelena/network_incomplete/synthetic_layers_5_sample_id_1_percent_removed_links"
NETWORKS = [  # label, input; the real-world and synthetic files are not in the fetch manifest
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
FLAGS = ["--regularized -N10", "--regularized -2 -N10", "--regularized -N1"]


def grab(pattern, text):
    m = re.search(pattern, text, re.MULTILINE)
    return m.group(1) if m else "NA"


def run(binary, net, out, args):
    """One run: (codelength, total_s, instr, top, levels)."""
    tj = os.path.join(out, "timing.json")
    if os.path.exists(tj):
        os.remove(tj)
    cmd = [
        "/usr/bin/time",
        "-l",
        binary,
        f"{R}/{net}",
        out,
        *args,
        "--seed",
        "123",
        "--timing-json",
        tj,
    ]
    p = subprocess.run(cmd, cwd=R, capture_output=True, text=True, check=False)
    text = p.stdout + p.stderr
    try:
        with open(tj) as f:
            total = json.load(f)["timing"]["total_s"]
    except (OSError, KeyError, ValueError):
        total = "NA"
    top = grab(r"^\s*Top modules\s+(\d+)", text)
    levels = grab(r"^\s*Levels\s+(\d+)", text)
    return (
        grab(r"Best codelength\s+([0-9.eE+-]+)", text),
        total,
        grab(r"(\d+)\s+instructions retired", text),
        top,
        levels,
    )


def measure(out_tsv, bins):
    jobs = []
    for rep in range(3):
        for label, net in NETWORKS:
            for flags in FLAGS:
                if rep == 0 or "-N1" in flags.split():
                    jobs.append((label, net, flags, rep))
    with (
        tempfile.TemporaryDirectory(prefix="bench-ml-") as tmp,
        open(out_tsv, "a") as fo,
    ):
        for label, net, flags, rep in jobs:
            base = os.path.splitext(os.path.basename(net))[0]
            rows = {}
            for arm in ("old", "new", "oo"):
                d = os.path.join(tmp, arm)
                os.makedirs(d, exist_ok=True)
                args = flags.split() + (["-C"] if arm != "oo" else []) + ["-o", "tree"]
                rows[arm] = run(bins["new" if arm == "oo" else arm], net, d, args)
            # The old columnar partition under the objective it should have optimized.
            states = os.path.join(tmp, "old", f"{base}_states.tree")
            d = os.path.join(tmp, "scored")
            os.makedirs(d, exist_ok=True)
            scored = run(
                bins["new"],
                net,
                d,
                ["--regularized", "--no-infomap", "-c", states, "--no-file-output"],
            )
            rows["old_scored"] = (scored[0], "", "", rows["old"][3], rows["old"][4])
            for arm in ("old", "old_scored", "new", "oo"):
                fo.write(
                    "\t".join([label, flags, arm, str(rep), *map(str, rows[arm])])
                    + "\n"
                )
            fo.flush()
            print(f"done {label} {flags} rep {rep}", flush=True)


def table(tsv):
    cell = defaultdict(
        lambda: {"cl": None, "t": float("inf"), "i": float("inf"), "top": "", "lv": ""}
    )
    with open(tsv) as f:
        lines = f.read().splitlines()
    for line in lines:
        label, flags, arm, rep, cl, t, ins, top, lv = line.split("\t")
        c = cell[label, flags, arm]
        if rep == "0":
            c["cl"], c["top"], c["lv"] = cl, top, lv
        if t not in ("", "NA"):
            c["t"] = min(c["t"], float(t))
        if ins not in ("", "NA"):
            c["i"] = min(c["i"], int(ins))

    def pct(a, b):
        return f"{100 * (float(a) - float(b)) / float(b):+.3f}%"

    def secs(t):
        return f"{t:.3f}s" if t < 1 else f"{t:.2f}s"

    def gi(i):
        return f"{i / 1e9:.2f}G" if i < 1e10 else f"{i / 1e9:.1f}G"

    print(
        '<table>\n<thead>\n<tr><th rowspan="2">network</th><th rowspan="2">flags</th>'
        '<th colspan="4">columnar (old)</th><th colspan="6">columnar (new)</th><th colspan="4">object-oriented</th></tr>'
    )
    print(
        "<tr><th>reported</th><th>scored with the prior</th><th>time</th><th>instr</th>"
        "<th>codelength (vs old scored)</th><th>vs OO</th><th>time (vs old)</th><th>instr (vs old)</th><th>top</th><th>lvls</th>"
        "<th>codelength</th><th>time</th><th>instr (new vs OO)</th><th>top</th></tr>\n</thead>\n<tbody>"
    )
    for label, _ in NETWORKS:
        for flags in FLAGS:
            o, s, n, oo = (
                cell[label, flags, a] for a in ("old", "old_scored", "new", "oo")
            )
            if n["cl"] is None:
                continue
            short = flags.replace("--regularized ", "")
            print(
                f"<tr><td>{label}</td><td><code>{short}</code></td><td>{o['cl']}</td><td>{s['cl']}</td><td>{secs(o['t'])}</td><td>{gi(o['i'])}</td>"
                f"<td>{n['cl']} ({pct(n['cl'], s['cl'])})</td><td>{pct(n['cl'], oo['cl'])}</td>"
                f"<td>{secs(n['t'])} ({pct(n['t'], o['t'])})</td><td>{gi(n['i'])} ({pct(n['i'], o['i'])})</td><td>{n['top']}</td><td>{n['lv']}</td>"
                f"<td>{oo['cl']}</td><td>{secs(oo['t'])}</td><td>{gi(oo['i'])} ({pct(n['i'], oo['i'])})</td><td>{oo['top']}</td></tr>"
            )
    print("</tbody></table>")


def main():
    argv = sys.argv[1:]
    if len(argv) < 1:
        sys.exit(__doc__)
    if "--table" in argv:
        table(argv[0])
        return
    bins = dict(a.split("=", 1) for a in argv[1:] if "=" in a)
    measure(argv[0], {k: os.path.abspath(v) for k, v in bins.items()})


if __name__ == "__main__":
    main()
