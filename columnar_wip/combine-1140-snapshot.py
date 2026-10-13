#!/usr/bin/env python3
"""Build the #1140 snapshot TSV on the table layout #1158 introduced (F66).

    combine-1140-snapshot.py <session.tsv> <controls.tsv> <out.tsv>

<session.tsv> is columnar_wip/1140-ab-results.tsv (2026-10-11, the #1136 configuration set
under its old labels), <controls.tsv> columnar_wip/1140-controls.tsv (2026-10-13, the
family rows #1158 added). Both measured old and new on the same two binaries. The om labels
are renamed as combine-om-controls.py renames them, and the family table is laid out as the
same grid: planted rows from the controls session (they carry the one-level codelength in a
twelfth column), regularized searches from the family rows, plain searches from the -C,
-C -2 and -C -N1 rows of that network (the same runs, so the tables agree).
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
SESSION = "session 2026-10-11 (1140-ab-results.tsv)"
CONTROLS = "session 2026-10-13 (1140-controls.tsv)"


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


session_tsv, controls_tsv, out_tsv = sys.argv[1:]

session, order = {}, []
for p in rows(session_tsv):
    key, label = p[0], relabel(p[1])
    if (key, label) not in session:
        order.append((key, label))
    session.setdefault((key, label), []).append(p)
controls = {}
for p in rows(controls_tsv):
    controls.setdefault(p[1], []).append(p)

out = []


def emit(key, label, reps, note, one_level=""):
    for p in reps:
        out.append([key, label, *p[2:10], note, one_level])


for key, label in order:
    if key != "FAM":
        emit(key, label, session[key, label], SESSION)

for om in range(2, 9):
    for e in (50000, 100000):
        net = name(om, e)
        for reg in ("", " --regularized"):
            planted = f"{net} planted, `-2d{reg} --no-infomap -c`"
            one_level = controls[planted][0][10]
            emit("FAM", planted, controls[planted], CONTROLS, one_level)
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
                    if source in session:
                        note = (
                            SESSION if reg else f"{SESSION} ({source[0]} {source[1]})"
                        )
                        emit("FAM", label, session[source], note)
                    else:
                        emit("FAM", label, controls[label], CONTROLS)

with open(out_tsv, "w") as f:
    for p in out:
        f.write("\t".join(map(str, p)) + "\n")
