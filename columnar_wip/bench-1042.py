#!/usr/bin/env python3
"""#1042 snapshot benchmark: two binaries (arms "old" and "new") over every row of the current
performance snapshot, columnar_wip/om-controls-snapshot.tsv. The row set is read from that
file at startup: every distinct (key, label, flags) of keys C1, C10, C2_10, F1, F10, FAM, L10.
No object-oriented arm: a row whose flags do not select the columnar engine (-C/--columnar) is
skipped and listed. Interleaved by arm within a row; -N1 rows as 3 reps spread across the batch
(rep loop outermost), every other row once per arm, as bench-1127.py.

    bench-1042.py <out.tsv> old=<binary> new=<binary> [+arm=<binary> ...] [--dry-run] [--snapshot-arms]

+arm=<binary>    an extra arm for the per-feature attribution: it runs only on the searched
                 recorded-teleportation rows (--regularized / --recorded-teleportation, not the
                 --no-infomap planted scores), interleaved with old and new in the same session.
++arm=<binary>   the same, but on every row old and new run on.

--dry-run        print rows and runs per key and check every input path; runs nothing, exits
                 non-zero if an input, a flag path or a binary is missing.
--snapshot-arms  run the old arm only on rows that have one in the snapshot TSV. By default both
                 arms run on every row; note that assemble-hier-gaps.py then lists the base
                 networks' F10 rows in the "`-F` on the family, old vs new" table too.

Inputs are the fetched benchmark set (networks/columnar-benchmark, benchmark-networks.md). The
snapshot's lazega rows name the pre-#1136 metadata path; it is rewritten to the fetched copy,
which the dry run checks to be byte-identical (FLAG_SUBST). Planted rows take the .clu next to
their network, as bench-om-controls.py does.

Row: key label arm rep flags codelength total_s instr top levels note one_level
The twelfth column is the one-level codelength, written for the family table's planted rows
only (assemble-hier-gaps.py reads it from there); it is empty on every other row.
Resumable: existing (key, label, arm, rep) rows with a codelength are skipped.
"""

import filecmp
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SNAPSHOT = f"{R}/columnar_wip/om-controls-snapshot.tsv"
NB = "networks/columnar-benchmark"  # fetch-benchmark-networks.py output
BASE = {  # snapshot label (before any " `flags`" suffix) -> input network
    "ninetriangles": f"{NB}/ninetriangles.net",
    "jazz": f"{NB}/jazz.txt",
    "netscience": f"{NB}/netscience.txt",
    "powergrid": f"{NB}/powergrid.txt",
    "polblogs": f"{NB}/polblogs.txt",
    "word_assoc": f"{NB}/word_assoc.txt",
    "word_assoc (pref.)": f"{NB}/word_assoc.txt",
    "web-NotreDame": f"{NB}/web-NotreDame.txt",
    "lazega": f"{NB}/lazega.net",
    "multilayer (ex.)": f"{NB}/multilayer.net",
    "malaria": f"{NB}/malaria.net",
    "air30k": f"{NB}/air30k.net",
    "air30k (reg.)": f"{NB}/air30k.net",
    "air30k (meta)": f"{NB}/air30k.net",
    "wikispeedia": f"{NB}/wikispeedia_states.net",
}
FLAG_SUBST = {"networks/meta/lazega.meta": f"{NB}/lazega.meta"}  # byte-identical copies
OM = re.compile(r"^(?:overlapping )?om(\d)_N256_E(\d+)k\b")
ARMS = ("old", "new")


def network(label):
    """Input network of a snapshot label, or None if the label is not known."""
    m = OM.match(label)
    if m:
        om, e = int(m[1]), int(m[2]) * 1000
        return f"{NB}/om/overlapping-memory-n256-om{om}-nc64-E{e}-mu0.1-seed1.net"
    return BASE.get(label.split(" `")[0])


def resolve_flags(flags, net):
    """The snapshot flags with FLAG_SUBST applied and -c pointing at the network's .clu."""
    toks = flags.split()
    for i, t in enumerate(toks):
        toks[i] = FLAG_SUBST.get(t, t)
        if i > 0 and toks[i - 1] == "-c" and net:
            toks[i] = net[: -len(".net")] + ".clu"
    return " ".join(toks)


def is_columnar(flags):
    return bool({"-C", "--columnar"} & set(flags.split()))


def is_n1(flags):
    return "-N1" in flags.split()


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


# Rows the snapshot did not have: every non-family benchmark network under --regularized (and
# --recorded-teleportation where it is directed), the flow models #1042's fix changes. Key REG;
# the label carries the flags. air30k `-d --regularized` is already the tables' "air30k (reg.)".
# multilayer and malaria are left out: --regularized on multilayer input needs the
# regularized-multilayer feature build.
REG_NETWORKS = [  # (label, standard flags, directed)
    ("ninetriangles", "", False),
    ("jazz", "", False),
    ("netscience", "", False),
    ("powergrid", "", False),
    ("polblogs", "-d", True),
    ("word_assoc", "-d", True),
    ("web-NotreDame", "-d", True),
    ("lazega", f"--meta-data {NB}/lazega.meta", False),
    ("air30k", "", False),
    ("air30k (meta)", f"--meta-data {NB}/air30k_usstate.meta", False),
    ("wikispeedia", "-2d", True),
    ("wikispeedia", "-d", True),
]


def reg_rows():
    rows = []
    for n in ("-N10", "-N1"):
        for label, std, directed in REG_NETWORKS:
            models = ["--regularized"] + (
                ["--recorded-teleportation"] if directed else []
            )
            for model in models:
                flags = " ".join(f"-C {std} {model} {n}".split())
                rows.append(("REG", f"{label} `{flags[3:]}`", flags, {"old", "new"}))
                if n == "-N10" and model == "--regularized" and "-2" not in std.split():
                    flags2 = " ".join(f"-C -2 {std} {model} {n}".split())
                    rows.append(
                        ("REG", f"{label} `{flags2[3:]}`", flags2, {"old", "new"})
                    )
    return rows


def read_snapshot():
    """[(key, label, snapshot flags, snapshot arms)] in snapshot order, then the REG rows."""
    rows, arms = {}, {}
    with open(SNAPSHOT) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) < 10:
                continue
            k = (p[0], p[1], p[4])
            rows.setdefault(k, None)
            arms.setdefault(k, set()).add(p[2])
    return [(*k, arms[k]) for k in rows] + reg_rows()


def is_recorded_search(flags):
    f = flags.split()
    return (
        "--regularized" in f or "--recorded-teleportation" in f
    ) and "--no-infomap" not in f


everywhere = set()  # extra arms given as ++arm=<binary>


def parse_args(argv):
    out, bins, dry, snap_arms = None, {}, False, False
    for a in argv:
        if a.startswith("++") and "=" in a:
            arm, path = a[2:].split("=", 1)
            bins[arm] = os.path.abspath(path)
            everywhere.add(arm)
        elif a.startswith("+") and "=" in a:
            arm, path = a[1:].split("=", 1)
            bins[arm] = os.path.abspath(path)
        elif a == "--dry-run":
            dry = True
        elif a == "--snapshot-arms":
            snap_arms = True
        elif "=" in a and a.split("=", 1)[0] in ARMS:
            arm, path = a.split("=", 1)
            bins[arm] = os.path.abspath(path)
        elif out is None and not a.startswith("-"):
            out = a
        else:
            sys.exit(f"unknown argument {a!r}\n\n{__doc__}")
    if out is None or not set(ARMS) <= set(bins):
        sys.exit(__doc__)
    return out, bins, dry, snap_arms


def grab(pattern, text):
    m = re.search(pattern, text, re.MULTILINE)
    return m.group(1) if m else "NA"


def main():
    out, bins, dry, snap_arms = parse_args(sys.argv[1:])

    configs, oo, unmapped = [], [], []  # config: (key, label, net, flags, arms, reps)
    rewrites = []  # (key, label, snapshot flags, flags run) where the two differ
    for key, label, flags, arms in read_snapshot():
        if not is_columnar(flags):
            oo.append((key, label, flags))
            continue
        net = network(label)
        if net is None:
            unmapped.append((key, label))
            continue
        run_arms = ARMS if not snap_arms else tuple(a for a in ARMS if a in arms)
        if is_recorded_search(flags):
            run_arms = (*run_arms, *(a for a in bins if a not in ARMS))
        else:
            run_arms = (*run_arms, *(a for a in bins if a in everywhere))
        reps = 3 if is_n1(flags) else 1
        resolved = resolve_flags(flags, net)
        if resolved != " ".join(flags.split()):
            rewrites.append((key, label, flags, resolved))
        configs.append((key, label, net, resolved, run_arms, reps))

    done = set()
    if os.path.exists(out):
        with open(out) as f:
            for line in f:
                p = line.rstrip("\n").split("\t")
                # A row counts as done only if it captured a codelength; NA rows are retried.
                if len(p) >= 6 and p[5] != "NA":
                    done.add((p[0], p[1], p[2], p[3]))

    if dry:
        sys.exit(dry_run(configs, oo, unmapped, rewrites, bins, done))
    if unmapped:
        sys.exit(f"unmapped snapshot rows (run --dry-run): {unmapped}")
    note = {arm: f"bench-1042 {arm} md5 {md5(bins[arm])[:8]}" for arm in bins}

    with tempfile.TemporaryDirectory(prefix="bench-1042-") as tmp:

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

        n1 = [c for c in configs if c[5] == 3]
        n10 = [c for c in configs if c[5] == 1]
        # Three interleaved slices of the -N10 work, one per -N1 rep.
        chunks = [n10[i::3] for i in range(3)]
        for rep in (1, 2, 3):
            for key, label, net, flags, arms, _ in n1:
                for arm in arms:
                    run(key, label, net, flags, arm, rep)
            for key, label, net, flags, arms, _ in chunks[rep - 1]:
                for arm in arms:
                    run(key, label, net, flags, arm, 1)
    with open(out, "a") as f:
        f.write("BENCH-DONE\n")


def dry_run(configs, oo, unmapped, rewrites, bins, done):
    """Print the plan and check every path. Returns the exit status."""
    bad = 0
    for arm in ARMS:
        ok = os.path.isfile(bins[arm]) and os.access(bins[arm], os.X_OK)
        print(
            f"binary {arm}: {bins[arm]} "
            + (f"md5 {md5(bins[arm])}" if ok else "MISSING")
        )
        bad += not ok

    per_key = {}
    for key, _, _, _, arms, reps in configs:
        k = per_key.setdefault(key, [0, 0])
        k[0] += 1
        k[1] += len(arms) * reps
    remaining = sum(
        1
        for key, label, _, _, arms, reps in configs
        for arm in arms
        for rep in range(1, reps + 1)
        if (key, label, arm, str(rep)) not in done
    )
    print(f"\n{'key':6} {'rows':>5} {'runs':>5}")
    for key, (nrows, nruns) in per_key.items():
        print(f"{key:6} {nrows:>5} {nruns:>5}")
    total_rows = sum(v[0] for v in per_key.values())
    total_runs = sum(v[1] for v in per_key.values())
    print(
        f"{'total':6} {total_rows:>5} {total_runs:>5}  ({remaining} not yet in the output)"
    )
    n1 = sum(1 for c in configs if c[5] == 3)
    print(f"-N1 rows (3 reps): {n1}; other rows (1 rep): {len(configs) - n1}")

    paths = {}
    for key, label, net, flags, _, _ in configs:
        paths.setdefault(net, (key, label))
        for t in flags.split():
            if "/" in t:
                paths.setdefault(t, (key, label))
    missing = [(p, kl) for p, kl in paths.items() if not os.path.isfile(f"{R}/{p}")]
    print(f"\ninput paths checked: {len(paths)}, missing: {len(missing)}")
    for p, (key, label) in missing:
        print(f"  MISSING {p} (first used by {key} {label})")
    for a, b in FLAG_SUBST.items():
        same = os.path.isfile(f"{R}/{a}") and filecmp.cmp(f"{R}/{a}", f"{R}/{b}", False)
        print(f"flag rewrite {a} -> {b}: " + ("byte-identical" if same else "DIFFERS"))
        bad += not same
    clu_rows = [c for c in configs if "-c" in c[3].split()]
    print(f"planted (-c) rows: {len(clu_rows)}")
    print(f"rows whose flags differ from the snapshot's: {len(rewrites)}")
    for key, label, a, b in rewrites:
        print(f"  {key} {label}: {a!r} -> {b!r}")
        # Only FLAG_SUBST may change a flag; a -c path the label does not lead to is an error.
        bad += any(t not in FLAG_SUBST for t in set(a.split()) - set(b.split()))

    print(f"\nOO rows skipped: {len(oo)}")
    for key, label, flags in oo:
        print(f"  {key} {label} :: {flags}")
    print(f"unmapped rows: {len(unmapped)}")
    for key, label in unmapped:
        print(f"  {key} {label}")
    return 1 if bad or missing or unmapped else 0


if __name__ == "__main__":
    main()
