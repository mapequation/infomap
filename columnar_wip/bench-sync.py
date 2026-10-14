#!/usr/bin/env python3
"""Master-sync snapshot benchmark: two binaries (arms "old" and "new") over every row of the
current snapshot, on BOTH engines. A sync changes shared code, so unlike a columnar sub-PR it
can move the object-oriented numbers too, and the OO arms are measured rather than carried.

    bench-sync.py <out.tsv> old=<binary> new=<binary> [--rows <results.tsv>] [--dry-run]

--rows     the results TSV whose old/new rows define the row set (default: the current
           snapshot's, columnar_wip/1042-ab-results.tsv). Its flags are taken as run, already
           resolved to the fetched inputs and the planted .clu paths.
--dry-run  print rows and runs per key and check every input path; runs nothing, exits non-zero
           if an input, a flag path or a binary is missing.

Columnar rows: every (key, label, flags) of the row set, both arms. OO rows: every C10 and C2_10
row of the two "OO vs columnar" tables with -C removed, keys OO10 and OO2_10, both arms. Not the
om family (no OO table shows it) and not air30k (meta), which OO does not finish in budget (#1134). Interleaved by arm within a row; -N1 rows as 3 reps spread
across the batch (rep loop outermost), every other row once per arm, as bench-1042.py.

Row: key label arm rep flags codelength total_s instr top levels note one_level
The twelfth column is the one-level codelength, written for the family table's planted rows
only (assemble-hier-gaps.py reads it from there); it is empty on every other row.
Resumable: existing (key, label, arm, rep) rows with a codelength are skipped.
"""

import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROWS = f"{R}/columnar_wip/1042-ab-results.tsv"
NB = "networks/columnar-benchmark"  # fetch-benchmark-networks.py output
ARMS = ("old", "new")
OO_KEYS = {"C10": "OO10", "C2_10": "OO2_10"}
OO_SKIP = {"air30k (meta)"}  # OO does not finish it (#1134)
OM = re.compile(r"^(?:overlapping )?om\d")  # family rows: no OO table shows them


def is_n1(flags):
    return "-N1" in flags.split()


def oo_flags(flags):
    """The columnar flags with the engine switch removed: `-C -2d` -> `-2d`, `-C` -> ``."""
    return " ".join(t for t in flags.split() if t not in ("-C", "--columnar"))


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def read_rows(path):
    """[(key, label, net, flags, reps)] in TSV order: the row set's old/new rows, then OO."""
    seen, cols = {}, []
    with open(path) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            # OO rows of an earlier sync's TSV are rebuilt from its C10 / C2_10 rows below.
            if len(p) < 10 or p[2] not in ARMS or p[0] in OO_KEYS.values():
                continue
            k = (p[0], p[1], " ".join(p[4].split()))
            if k not in seen:
                seen[k] = None
                cols.append(k)
    nets = net_of_label(path)
    rows = [(k, lb, nets[k, lb], fl, 3 if is_n1(fl) else 1) for k, lb, fl in cols]
    for k, lb, fl in cols:
        if k in OO_KEYS and lb not in OO_SKIP and not OM.match(lb):
            rows.append((OO_KEYS[k], lb, nets[k, lb], oo_flags(fl), 1))
    return rows


def net_of_label(path):
    """(key, label) -> input network, from the bench-1042 module that wrote the row set."""
    import importlib.util

    spec = importlib.util.spec_from_file_location(
        "bench1042", f"{R}/columnar_wip/bench-1042.py"
    )
    bench = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bench)
    out = {}
    with open(path) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) >= 10:
                out[p[0], p[1]] = bench.network(p[1])
    return out


def parse_args(argv):
    out, bins, rows, dry = None, {}, ROWS, False
    it = iter(argv)
    for a in it:
        if a == "--dry-run":
            dry = True
        elif a == "--rows":
            rows = os.path.abspath(next(it))
        elif "=" in a and a.split("=", 1)[0] in ARMS:
            arm, path = a.split("=", 1)
            bins[arm] = os.path.abspath(path)
        elif out is None and not a.startswith("-"):
            out = a
        else:
            sys.exit(f"unknown argument {a!r}\n\n{__doc__}")
    if out is None or set(bins) != set(ARMS):
        sys.exit(__doc__)
    return out, bins, rows, dry


def grab(pattern, text):
    m = re.search(pattern, text, re.MULTILINE)
    return m.group(1) if m else "NA"


def main():
    out, bins, rows_path, dry = parse_args(sys.argv[1:])
    configs = read_rows(rows_path)

    done = set()
    if os.path.exists(out):
        with open(out) as f:
            for line in f:
                p = line.rstrip("\n").split("\t")
                # A row counts as done only if it captured a codelength; NA rows are retried.
                if len(p) >= 6 and p[5] != "NA":
                    done.add((p[0], p[1], p[2], p[3]))

    if dry:
        sys.exit(dry_run(configs, bins, done))
    note = {arm: f"bench-sync {arm} md5 {md5(bins[arm])[:8]}" for arm in bins}

    with tempfile.TemporaryDirectory(prefix="bench-sync-") as tmp:

        def run(key, label, net, flags, arm, rep):
            if (key, label, arm, str(rep)) in done:
                return
            tj = f"{tmp}/timing.json"
            if os.path.exists(tj):
                os.remove(tj)
            cmd = (
                ["/usr/bin/time", "-l", bins[arm], f"{R}/{net}", tmp]
                + flags.split()
                + ["--seed", "123", "--no-file-output", "--timing-json", tj]
            )
            p = subprocess.run(cmd, cwd=R, capture_output=True, text=True, check=False)
            text = p.stdout + p.stderr
            cl = grab(r"Best codelength\s+([0-9.eE+-]+)", text)
            try:
                with open(tj) as f:
                    total = json.load(f)["timing"]["total_s"]
            except (OSError, KeyError, ValueError):
                total = "NA"
            one_level = ""
            if key == "FAM" and " planted, " in label:
                one_level = grab(r"^\s*One-level codelength\s+([0-9.eE+-]+)", text)
            fields = [
                key,
                label,
                arm,
                rep,
                flags,
                cl,
                total,
                grab(r"(\d+)\s+instructions retired", text),
                grab(r"^\s*Top modules\s+(\d+)", text),
                grab(r"^\s*Levels\s+(\d+)", text),
                note[arm],
                one_level,
            ]
            with open(out, "a") as f:
                f.write("\t".join(map(str, fields)) + "\n")
            if cl == "NA":
                with open(out + ".err", "a") as f:
                    f.write(f"### {key} {label} {arm} {rep} :: {' '.join(cmd)}\n")
                    f.write(f"{text[-3000:]}\n")

        n1 = [c for c in configs if c[4] == 3]
        n10 = [c for c in configs if c[4] == 1]
        # Three interleaved slices of the -N10 work, one per -N1 rep.
        chunks = [n10[i::3] for i in range(3)]
        for rep in (1, 2, 3):
            for key, label, net, flags, _ in n1:
                for arm in ARMS:
                    run(key, label, net, flags, arm, rep)
            for key, label, net, flags, _ in chunks[rep - 1]:
                for arm in ARMS:
                    run(key, label, net, flags, arm, 1)
    with open(out, "a") as f:
        f.write("BENCH-DONE\n")


def dry_run(configs, bins, done):
    bad = 0
    for arm, path in bins.items():
        ok = os.access(path, os.X_OK)
        bad += not ok
        print(f"{arm}: {path} {md5(path) if ok else 'MISSING'}")
    per_key, runs, todo = {}, 0, 0
    for key, label, net, flags, reps in configs:
        per_key[key] = per_key.get(key, 0) + 1
        for arm in ARMS:
            for rep in range(1, reps + 1):
                runs += 1
                todo += (key, label, arm, str(rep)) not in done
        paths = [net] + [t for t in flags.split() if "/" in t]
        for p in paths:
            if p is None or not os.path.exists(f"{R}/{p}"):
                bad += 1
                print(f"MISSING {key} {label}: {p}")
    print(" ".join(f"{k}={n}" for k, n in per_key.items()))
    print(f"configs {len(configs)}, runs {runs}, still to run {todo}")
    return 1 if bad else 0


if __name__ == "__main__":
    main()
