#!/usr/bin/env python3
"""Bit-identity check for a refactor: two binaries (arms "old" and "new") on every columnar row
of a snapshot row set, once each, comparing the codelength AND the tree body.

    bench-identity.py <out.tsv> old=<binary> new=<binary> [--rows <results.tsv>] [--jobs N]

--rows  the results TSV whose rows define the set (default: bench-sync.py's default). Object-
        oriented rows are skipped: a columnar refactor cannot move them.
--jobs  parallel runs (default 4). Identity does not depend on timing, so rows run concurrently;
        the instructions column is still per run, but seconds are not recorded.

Row: key label arm flags codelength instr tree_md5
The tree md5 is over the body of every .tree file the run writes (a state or multilayer network
also writes a _states.tree): every line not starting with '#', so the header's timestamp and
timings do not count. A row is identical when both arms agree on codelength and
tree md5. Prints the rows that differ and a summary; exits non-zero if any row differs or fails.
"""

import concurrent.futures
import hashlib
import importlib.util
import os
import re
import subprocess
import sys
import tempfile

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_bench_sync():
    spec = importlib.util.spec_from_file_location("benchsync", f"{R}/columnar_wip/bench-sync.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def grab(pattern, text):
    m = re.search(pattern, text, re.MULTILINE)
    return m.group(1) if m else "NA"


def tree_md5(outdir):
    # A state or multilayer network writes a _states.tree beside the .tree: hash both.
    trees = sorted(f for f in os.listdir(outdir) if f.endswith(".tree"))
    if not trees:
        return "NA"
    h = hashlib.md5()
    for name in trees:
        h.update(name.encode() + b"\n")
        with open(os.path.join(outdir, name), "rb") as f:
            for line in f:
                if not line.startswith(b"#"):
                    h.update(line)
    return h.hexdigest()


def run(binary, net, flags):
    with tempfile.TemporaryDirectory(prefix="bench-identity-") as out:
        cmd = ["/usr/bin/time", "-l", binary, f"{R}/{net}", out] + flags.split() + ["--seed", "123", "-o", "tree"]
        p = subprocess.run(cmd, cwd=R, capture_output=True, text=True, check=False)
        text = p.stdout + p.stderr
        cl = grab(r"Best codelength\s+([0-9.eE+-]+)", text)
        instr = grab(r"(\d+)\s+instructions retired", text)
        return cl, instr, tree_md5(out) if cl != "NA" else "NA"


def main():
    args = sys.argv[1:]
    out = args.pop(0)
    bins, rows_path, jobs = {}, None, 4
    it = iter(args)
    for a in it:
        if a == "--rows":
            rows_path = next(it)
        elif a == "--jobs":
            jobs = int(next(it))
        elif "=" in a:
            arm, path = a.split("=", 1)
            bins[arm] = path
    bs = load_bench_sync()
    configs = [c for c in bs.read_rows(rows_path or bs.ROWS) if c[0] not in bs.OO_KEYS.values()]
    tasks = [(key, label, net, flags, arm) for key, label, net, flags, _ in configs for arm in ("old", "new")]
    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = {pool.submit(run, bins[t[4]], t[2], t[3]): t for t in tasks}
        for fut in concurrent.futures.as_completed(futures):
            results[futures[fut]] = fut.result()
    differ, failed = [], []
    with open(out, "w") as f:
        for key, label, net, flags, arm in tasks:
            cl, instr, md5 = results[key, label, net, flags, arm]
            f.write("\t".join([key, label, arm, flags, cl, instr, md5]) + "\n")
    for key, label, net, flags, _ in configs:
        o = results[key, label, net, flags, "old"]
        n = results[key, label, net, flags, "new"]
        if "NA" in (o[0], n[0], o[2], n[2]):
            failed.append((key, label, flags, o, n))
        elif o[0] != n[0] or o[2] != n[2]:
            differ.append((key, label, flags, o, n))
    for key, label, flags, o, n in differ + failed:
        print(f"{'DIFFER' if (key, label, flags, o, n) in differ else 'FAILED'} {key} {label} [{flags}] old {o[0]} {o[2][:8]} new {n[0]} {n[2][:8]}")
    print(f"{len(configs)} rows: {len(configs) - len(differ) - len(failed)} identical, {len(differ)} differ, {len(failed)} failed")
    sys.exit(1 if differ or failed else 0)


if __name__ == "__main__":
    main()
