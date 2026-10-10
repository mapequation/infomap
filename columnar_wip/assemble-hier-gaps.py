#!/usr/bin/env python3
"""Regenerate the numeric tables of columnar-pr-performance-section.md from a
bench-hier-gaps.py results TSV.

Usage: assemble-hier-gaps.py <results.tsv> <snapshot.md> [--check]

Only table bodies are rewritten (the "What the change moves" markdown table and the
HTML tables); prose, headers and the hand-written per-feature attribution tables are
left byte-for-byte. Row order and row set follow the existing snapshot where a label is
already there; labels that are new in the TSV are appended in TSV order.

TSV row: key label arm rep flags codelength total_s instr top levels
Each (key, label, arm) is reduced to: codelength string of rep 1, minimum total_s over
all reps, minimum instructions over all reps (independently), top/levels of rep 1.

The OO cells of the two "OO vs columnar" tables are not in the TSV; they are parsed
from the existing snapshot and carried over (their deltas are recomputed from the
printed, rounded values).
"""

import difflib
import re
import sys
from collections import namedtuple

Cell = namedtuple("Cell", "bits_s bits time instr top lvls")

HEADS = {  # heading prefix -> list of table specs, in document order
    "What the change moves": ["moves"],
    "Old vs new columnar — standard search": [("pair", "C10")],
    "Old vs new columnar — two-level": [("pair", "C2_10")],
    "Single-trial runs": [("pair", "C1")],
    "The overlapping family in full": [("pair", "FAM")],
    "`-F` on the family, old vs new": [("pair", "F10"), ("pair", "F1")],
    "The fast dial `-F`": [("cross", "C10", "F10")],
    "The non-redundant map equation L": [("cross", "C10", "L10")],
    "OO vs columnar —": [("oo", "C2_10")],
    "OO vs columnar": [("oo", "C10")],
}
MOVES_TABLE = {  # key -> "table" column of the moves table, in row order
    "C1": "`-C -N1`",
    "C10": "`-C`",
    "C2_10": "`-C -2`",
    "F1": "`-C -F -N1`",
    "F10": "`-C -F`",
    "FAM": "family",
}


def load(path):
    reps = {}
    order = []
    with open(path) as fh:
        lines = fh.readlines()
    for line in lines:
        f = line.rstrip("\n").split("\t")
        if len(f) < 10:
            continue
        key, label, arm, rep = f[0], f[1], f[2], int(f[3])
        if (key, label) not in order:
            order.append((key, label))
        reps.setdefault((key, label, arm), []).append((rep, f))
    data = {}
    for k, lst in reps.items():
        lst.sort(key=lambda r: r[0])
        first = lst[0][1]
        for _, f in lst:
            if f[5] != first[5]:
                print(f"warning: codelength differs between reps: {k}", file=sys.stderr)
        _, best = min(lst, key=lambda r: float(r[1][6]))
        data[k] = Cell(
            first[5],
            float(first[5]),
            float(best[6]),
            min(float(r[1][7]) for r in lst),
            first[8],
            first[9],
        )
    return data, order


def fmt_time(t):
    return f"{t:.3f}s" if t < 1 else f"{t:.2f}s" if t < 10 else f"{t:.1f}s"


def fmt_instr(i):
    return f"{i / 1e9:.1f}G"


def pct(new, old, nd):
    return f"{(new - old) / old * 100:+.{nd}f}%"


def tds(*vals):
    return "".join(f'<td align="right">{v}</td>' for v in vals)


def left_cells(c):
    return [c.bits_s, fmt_time(c.time), fmt_instr(c.instr), c.top, c.lvls]


def right_cells(c, ref):
    d = "=" if c.bits == ref.bits else pct(c.bits, ref.bits, 4)
    return [
        f"{c.bits_s} ({d})",
        f"{fmt_time(c.time)} ({pct(c.time, ref.time, 1)})",
        f"{fmt_instr(c.instr)} ({pct(c.instr, ref.instr, 2)})",
        c.top,
        c.lvls,
    ]


def row(label, left, right):
    return f"<tr>{tds(label, *left, *right)}</tr>"


def ordered(labels, existing):
    """Existing labels keep their snapshot order; new ones follow in the given order."""
    known = [x for x in existing if x in labels]
    return known + [x for x in labels if x not in known]


def parse_rows(lines):
    out = {}
    for ln in lines:
        m = re.findall(r"<td[^>]*>(.*?)</td>", ln)
        out[m[0]] = m[1:]
    return out


def html_rows(spec, data, order, old_lines):
    existing = list(parse_rows(old_lines))
    kind, key = spec[0], spec[1]
    if kind == "pair":
        labels = [
            lb
            for k, lb in order
            if k == key and (key, lb, "old") in data and (key, lb, "new") in data
        ]
        return [
            row(
                lb,
                left_cells(data[key, lb, "old"]),
                right_cells(data[key, lb, "new"], data[key, lb, "old"]),
            )
            for lb in ordered(labels, existing)
        ]
    if kind == "cross":
        k2 = spec[2]
        labels = [lb for k, lb in order if k == k2 and (key, lb, "new") in data]
        return [
            row(
                lb,
                left_cells(data[key, lb, "new"]),
                right_cells(data[k2, lb, "new"], data[key, lb, "new"]),
            )
            for lb in ordered(labels, existing)
        ]
    # oo: left cells carried from the snapshot, right from the TSV
    carried = parse_rows(old_lines)
    out = []
    for lb in existing:
        left = carried[lb][:5]
        try:
            ref = Cell(
                left[0],
                float(left[0]),
                float(left[1][:-1]),
                float(left[2][:-1]) * 1e9,
                left[3],
                left[4],
            )
        except ValueError:  # an OO run that did not finish: no deltas
            out.append(row(lb, left, left_cells(data[key, lb, "new"])))
            continue
        out.append(row(lb, left, right_cells(data[key, lb, "new"], ref)))
    return out


def moves_rows(data, order, old_lines):
    rows = []
    for key, table in MOVES_TABLE.items():
        for k, lb in order:
            o, n = data.get((key, lb, "old")), data.get((key, lb, "new"))
            if k != key or not o or not n or o.bits == n.bits:
                continue
            tname = table
            if key == "FAM":
                lb2, tname = lb, "family"
            else:
                lb2 = lb
            rows.append((tname, lb2, o, n))
    out = []
    for tname, lb, o, n in rows:
        out.append(
            f"| {lb} | {tname} | {o.bits_s} | **{n.bits_s}** | **{pct(n.bits, o.bits, 4)}** "
            f"| {fmt_instr(o.instr)} | {fmt_instr(n.instr)} | {pct(n.instr, o.instr, 2)} "
            f"| {fmt_time(o.time)} | {fmt_time(n.time)} | {pct(n.time, o.time, 1)} |"
        )
    return out


def spec_for(heading):
    best = None
    for prefix in HEADS:
        if heading.startswith(prefix) and (best is None or len(prefix) > len(best)):
            best = prefix
    return HEADS.get(best) if best else None


def assemble(tsv, text):
    data, order = load(tsv)
    lines = text.split("\n")
    out = []
    i = 0
    specs, ti = None, 0
    while i < len(lines):
        ln = lines[i]
        if ln.startswith("#"):
            specs, ti = spec_for(ln.lstrip("# ")), 0
        if specs and ln == "<table>" and ti < len(specs):
            spec = specs[ti]
            ti += 1
            j = i
            while lines[j] != "<tbody>" and not lines[j].endswith("<tbody>"):
                j += 1
            out.extend(lines[i : j + 1])
            k = j + 1
            while lines[k] != "</tbody>":
                k += 1
            out.extend(html_rows(spec, data, order, lines[j + 1 : k]))
            i = k
            continue
        if specs and specs[0] == "moves" and ln.startswith("|---"):
            out.append(ln)
            k = i + 1
            while lines[k].startswith("|"):
                k += 1
            out.extend(moves_rows(data, order, lines[i + 1 : k]))
            i = k
            continue
        out.append(ln)
        i += 1
    return "\n".join(out)


def main():
    args = [a for a in sys.argv[1:] if a != "--check"]
    check = "--check" in sys.argv
    if len(args) != 2:
        sys.exit(__doc__)
    tsv, md = args
    with open(md) as fh:
        old = fh.read()
    new = assemble(tsv, old)
    if check:
        sys.stdout.writelines(
            difflib.unified_diff(
                old.splitlines(True), new.splitlines(True), md, md + " (assembled)", n=0
            )
        )
    else:
        with open(md, "w") as fh:
            fh.write(new)


if __name__ == "__main__":
    main()
