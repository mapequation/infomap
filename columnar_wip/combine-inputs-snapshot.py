#!/usr/bin/env python3
"""Build the snapshot TSV for the fetched benchmark inputs, and patch the OO cells.

    combine-inputs-snapshot.py <session.tsv> <1127.tsv> <out.tsv> <snapshot.md>

Rows whose input changed come from this session (columnar_wip/bench-inputs.py: old input
file vs the fetched one, on one binary). Every other row's input is byte-identical, and the
binary is #1127's new one, so its #1127 new-arm row stands for both arms (only where #1127
had an old arm, so the pair tables keep their row sets). The session's OO rows are written
into the two "OO vs columnar" tables of the snapshot, whose other OO cells are carried;
columnar_wip/assemble-hier-gaps.py then regenerates every table from the TSV.
"""

import re
import sys
from pathlib import Path

CHANGED = {
    "jazz",
    "powergrid",
    "web-NotreDame",
    "malaria",
    "air30k",
    "air30k (reg.)",
    "air30k (meta)",
}


def rows(path):
    for line in Path(path).read_text().splitlines():
        p = line.split("\t")
        if len(p) >= 10:
            yield p


def is_changed(label):
    return label in CHANGED or re.match(r"(?:overlapping )?om\d", label) is not None


def fmt_time(t):
    return f"{t:.3f}s" if t < 1 else f"{t:.2f}s" if t < 10 else f"{t:.1f}s"


def oo_cells(p):
    if p[5] == "NA":  # killed after 9 h 25 min: #1134
        return [
            "did not finish, #1134",
            "&gt;9 h",
            f"&gt;{float(p[7]) / 1e9:.0f}G",
            "—",
            "—",
        ]
    return [p[5], fmt_time(float(p[6])), f"{float(p[7]) / 1e9:.1f}G", p[8], p[9]]


def main():
    session, old1127, out, md = sys.argv[1:]
    sess = list(rows(session))
    has_old = {(p[0], p[1]) for p in rows(old1127) if p[2] == "old"}
    lines = []
    for p in rows(old1127):
        if p[2] == "new" and not is_changed(p[1]):
            arms = ("old", "new") if (p[0], p[1]) in has_old else ("new",)
            note = "carried: 1127-ab-results.tsv new arm"
            lines += ["\t".join([*p[:2], arm, *p[3:10], note]) for arm in arms]
    lines += ["\t".join([*p[:10], "this session"]) for p in sess if p[2] != "oo"]
    Path(out).write_text("".join(f"{line}\n" for line in lines))

    oo = {(p[0], p[1]): p for p in sess if p[2] == "oo"}
    text = Path(md).read_text().split("\n")
    section = None
    for i, line in enumerate(text):
        if line.startswith("### OO vs columnar"):
            section = "O2_10" if "two-level" in line else "O10"
        elif line.startswith("### "):
            section = None
        if not (section and line.startswith("<tr><td")):
            continue
        tds = re.findall(r"<td[^>]*>.*?</td>", line)
        label = re.sub(r"<[^>]+>", "", tds[0])
        if (section, label) in oo:
            for j, v in enumerate(oo_cells(oo[section, label]), start=1):
                tds[j] = f'<td align="right">{v}</td>'
            text[i] = "<tr>" + "".join(tds) + "</tr>"
            print("OO", section, label, *oo_cells(oo[section, label]))
    Path(md).write_text("\n".join(text))


if __name__ == "__main__":
    main()
