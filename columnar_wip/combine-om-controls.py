#!/usr/bin/env python3
"""Build the snapshot TSV for the planted-control family table (F66), and rename the om
rows of the snapshot.

    combine-om-controls.py <inputs-snapshot.tsv> <session.tsv> <out.tsv> <snapshot.md>

No engine code changes and no input changes, so every row of the #1136 snapshot is carried:
its new arm stands for both arms (one measurement), and a row that had no old arm keeps
none. The om labels name the network in full: `om5 E50000` becomes `om5_N256_E50k`, and a
bare `om<k>` the density it was first drawn at (om2: E50000, om3-om8: E100000).

The overlapping-family table becomes a grid: 14 networks x {plain, --regularized} x
{planted, -2d -N10, -d -N10, -2d -N1, -d -N1}. Its plain search rows are the same runs as
the main tables' (the -C, -C -2 and -C -N1 rows of that network), so the two show the same
numbers. Rows no snapshot had before come from this session (bench-om-controls.py) with a
new arm only. Planted rows carry the one-level codelength of their run in a twelfth column.
The snapshot's family table body is emptied so that columnar_wip/assemble-hier-gaps.py
lays it out in grid order.
"""

import re
import sys

MAIN_DENSITY = {
    2: 50000,
    3: 100000,
    4: 100000,
    5: 100000,
    6: 100000,
    7: 100000,
    8: 100000,
}
CARRIED = "carried: inputs-snapshot.tsv new arm"
SESSION = "this session (om-controls-ab-results.tsv)"


def name(om, e):
    return f"om{om}_N256_E{e // 1000}k"


def relabel(label):
    def sub(m):
        om = int(m[1])
        return name(om, int(m[2]) if m[2] else MAIN_DENSITY[om])

    return re.sub(r"\bom(\d)(?: E(\d+))?(?=[ ,])", sub, label, count=1)


def rows(path):
    with open(path) as f:
        return [p for p in (ln.rstrip("\n").split("\t") for ln in f) if len(p) >= 10]


src, session_tsv, out_tsv, md = sys.argv[1:]

carried, has_old, order = {}, set(), []
for p in rows(src):
    key, label = p[0], relabel(p[1])
    if p[2] == "old":
        has_old.add((key, label))
    if p[2] != "new":
        continue
    if (key, label) not in carried:
        order.append((key, label))
    carried.setdefault((key, label), []).append(p)
session = {}
for p in rows(session_tsv):
    session.setdefault(p[1], []).append(p)

out = []


def emit(key, label, reps, arms, note, one_level=""):
    for arm in arms:
        for p in reps:
            out.append([key, label, arm, *p[3:10], note, one_level])


for key, label in order:
    if key != "FAM":
        arms = ("old", "new") if (key, label) in has_old else ("new",)
        emit(key, label, carried[key, label], arms, CARRIED)

for om in range(2, 9):
    for e in (50000, 100000):
        net = name(om, e)
        for reg in ("", " --regularized"):
            planted = f"{net} planted, `-2d{reg} --no-infomap -c`"
            one_level = session[planted][0][10]
            if ("FAM", planted) in carried:
                old_bits = carried["FAM", planted][0][5]
                assert old_bits == session[planted][0][5], planted
                emit(
                    "FAM",
                    planted,
                    carried["FAM", planted],
                    ("old", "new"),
                    CARRIED,
                    one_level,
                )
            else:
                emit("FAM", planted, session[planted], ("new",), SESSION, one_level)
            for flags in ("-2d", "-d"):
                for trials in ("-N10", "-N1"):
                    label = f"{net} `{flags}{reg} {trials}`"
                    if reg:
                        source = ("FAM", label)
                    else:
                        table = {
                            "-N10": "C2_10" if flags == "-2d" else "C10",
                            "-N1": "C1",
                        }
                        source = (table[trials], f"overlapping {net} `{flags}`")
                    if source in carried:
                        note = (
                            CARRIED if reg else f"{CARRIED} ({source[0]} {source[1]})"
                        )
                        emit("FAM", label, carried[source], ("old", "new"), note)
                    else:
                        emit("FAM", label, session[label], ("new",), SESSION)

with open(out_tsv, "w") as f:
    for p in out:
        f.write("\t".join(map(str, p)) + "\n")

with open(md) as f:
    lines = f.read().split("\n")
in_family = False
kept = []
for ln in lines:
    if ln.startswith("#"):
        in_family = ln.startswith("### The overlapping family in full")
    if in_family and ln.startswith("<tr><td"):
        continue  # assemble-hier-gaps.py rebuilds the family table in grid order
    kept.append(
        re.sub(r'^(<tr><td align="right">)([^<]*)', lambda m: m[1] + relabel(m[2]), ln)
    )
with open(md, "w") as f:
    f.write("\n".join(kept))
