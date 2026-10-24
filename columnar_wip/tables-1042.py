#!/usr/bin/env python3
"""Hand-written tables of the #1042 snapshot, from bench-1042.py's results TSV.

Usage: tables-1042.py <results.tsv> reg|attribution|assembly

reg          old vs new on the REG rows (every non-family benchmark network under
             --regularized, and --recorded-teleportation where directed), styled as the
             snapshot's other old-vs-new tables.
attribution  every recorded-teleportation search row, and every other row where a feature
             arm changed the codelength, with the old arm and each feature arm as the change
             in bits and in seconds against the old arm: fixonly (leaf rates stripped; run on
             the recorded-teleportation searches only, "—" elsewhere, where it is the old
             binary's search), cmp (fixonly + collapsed-winner comparison) and new (cmp +
             stalled-aggregation stop).
assembly     the TSV assemble-hier-gaps.py reads, on stdout: the old and new arms only, the
             old arm dropped where the snapshot tables never showed one (the base networks'
             F10 rows and L10), and every family row that repeats a -C, -C -2 or -C -N1 row
             (same network, same flags) given that row's measurement, so the two tables
             show one run, as F66 had it.

Cells are reduced exactly as assemble-hier-gaps.py reduces them (its load(): codelength
of rep 1, minimum time and minimum instructions over the reps), so a configuration that
also sits in an assembled table shows the same numbers here.
"""

import importlib.util
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, os.path.join(here, filename))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


asm = module("assemble", "assemble-hier-gaps.py")

FEATURES = ("fixonly", "cmp", "new")
FLAGS = {"C10": "`-C -N10`", "C2_10": "`-C -2 -N10`", "C1": "`-C -N1`"}


def is_recorded(label):
    return "regularized" in label or "recorded" in label or label == "air30k (reg.)"


def redundant(label):
    # bench-1042.py's REG rows add -2 to every network without it, which repeats
    # wikispeedia's own -2d row (as `-2 -2d` and `-2 -d`).
    return label.startswith("wikispeedia `-2 ")


def reg_table(data, order):
    out = [
        "<table>",
        "<thead>",
        "<tr>",
        '<th rowspan="2">network</th>',
        '<th colspan="5">columnar (old)</th>',
        '<th colspan="5">columnar (this PR)</th>',
        "</tr>",
        "<tr>",
        "<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>",
        "<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>",
        "</tr></thead><tbody>",
    ]
    for key, label in order:
        if key != "REG" or redundant(label):
            continue
        if (key, label, "old") not in data or (key, label, "new") not in data:
            continue
        old, new = data[key, label, "old"], data[key, label, "new"]
        out.append(asm.row(label, asm.left_cells(old), asm.right_cells(new, old)))
    out.append("</tbody></table>")
    return "\n".join(out)


def attribution_table(data, order):
    head = "".join(f"<th>{a}</th>" for a in FEATURES)
    out = [
        "<table>",
        "<thead>",
        (
            f'<tr><th rowspan="2">row</th><th colspan="2">old</th><th colspan="{len(FEATURES)}">'
            "change against old: bits / seconds</th></tr>"
        ),
        f"<tr><th>codelength</th><th>time</th>{head}</tr>",
        "</thead><tbody>",
    ]
    for key, label in order:
        if redundant(label) or (key, label, "old") not in data:
            continue
        if any((key, label, a) not in data for a in ("cmp", "new")):
            continue
        old = data[key, label, "old"]
        moved = any(data[key, label, a].bits != old.bits for a in ("cmp", "new"))
        if not (is_recorded(label) and "no-infomap" not in label) and not moved:
            continue
        name = f"{label} {FLAGS[key]}" if key in FLAGS else label
        cells = []
        for a in FEATURES:
            c = data.get((key, label, a))
            if c is None:
                cells.append("—")
                continue
            bits = "=" if c.bits == old.bits else asm.pct(c.bits, old.bits, 2)
            cells.append(f"{bits} / {asm.pct(c.time, old.time, 0)}")
        out.append(
            f"<tr>{asm.tds(name, old.bits_s, asm.fmt_time(old.time), *cells)}</tr>"
        )
    out.append("</tbody></table>")
    return "\n".join(out)


def assembly_tsv(path):
    bench = module("bench", "bench-1042.py")
    with open(path) as fh:
        rows = [f for f in (ln.rstrip("\n").split("\t") for ln in fh) if len(f) >= 10]
    main_runs = {}  # (network, flags, arm, rep) -> measured fields of the -C/-C -2/-C -N1 row
    for f in rows:
        if f[0] in ("C10", "C2_10", "C1"):
            main_runs[bench.network(f[1]), " ".join(f[4].split()), f[2], f[3]] = f[5:10]
    out = []
    for f in rows:
        if f[2] not in ("old", "new") or f[0] == "REG":
            continue
        if f[2] == "old" and (
            f[0] == "L10" or (f[0] == "F10" and not bench.OM.match(f[1]))
        ):
            continue
        if f[0] == "FAM":
            run = main_runs.get(
                (bench.network(f[1]), " ".join(f[4].split()), f[2], f[3])
            )
            if run is not None:
                f = f[:5] + run + f[10:]
        out.append("\t".join(f))
    return "\n".join(out)


def main():
    if len(sys.argv) != 3 or sys.argv[2] not in ("reg", "attribution", "assembly"):
        sys.exit(__doc__)
    if sys.argv[2] == "assembly":
        print(assembly_tsv(sys.argv[1]))
        return
    data, order = asm.load(sys.argv[1])
    print(
        reg_table(data, order)
        if sys.argv[2] == "reg"
        else attribution_table(data, order)
    )


if __name__ == "__main__":
    main()
