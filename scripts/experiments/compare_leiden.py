#!/usr/bin/env python3
"""Compare Infomap vs Leiden on the LFR corpus: AMI vs ground truth + scaling with N.

Run with the venv python that has igraph/leidenalg/sklearn:
  /tmp/leidenenv/bin/python scripts/experiments/compare_leiden.py

Infomap runs via subprocess (the built ./Infomap); Leiden via leidenalg (single-thread).
Wall comparison is constant-factor-confounded (the map equation's per-move cost is dearer
than modularity's), so the comparable signal is the SCALING EXPONENT (slope of time vs N)
and the AMI (quality), not absolute speed.
"""
import glob
import os
import re
import subprocess
import time
import numpy as np
import igraph as ig
import leidenalg
from sklearn.metrics import adjusted_mutual_info_score

CORPUS = "build/benchmarks/lfr"
INFOMAP = "./Infomap"
TMP = "/tmp/cmp_clu"
SEEDS = [1, 2, 3, 4, 5]


def read_net(path):
    edges, n, section = [], 0, None
    with open(path) as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            low = s.lower()
            if low.startswith("*vertices"):
                section = "v"
                parts = s.split()
                if len(parts) > 1:
                    n = int(parts[1])
                continue
            if low.startswith(("*edges", "*arcs")):
                section = "e"
                continue
            if section == "v":
                n = max(n, int(s.split()[0]) + 1)
            elif section == "e":
                p = s.split()
                edges.append((int(p[0]), int(p[1])))
    return n, edges


def load_truth(path):
    truth = {}
    with open(path) as f:
        for line in f:
            nid, c = line.split()
            truth[int(nid)] = int(c)
    return truth


def infomap_run(net, seed):
    name = os.path.splitext(os.path.basename(net))[0]
    t0 = time.perf_counter()
    subprocess.run([INFOMAP, net, TMP, "--silent", "--clu", "--seed", str(seed),
                    "--num-trials", "1"],
                   env=dict(os.environ, OMP_NUM_THREADS="1"),
                   capture_output=True, text=True)
    dt = time.perf_counter() - t0
    pred = {}
    with open(os.path.join(TMP, f"{name}.clu")) as f:
        for line in f:
            if line.startswith("#"):
                continue
            p = line.split()
            if len(p) >= 2:
                pred[int(p[0])] = int(p[1])
    return pred, dt


def leiden_run(g, seed):
    t0 = time.perf_counter()
    part = leidenalg.find_partition(g, leidenalg.ModularityVertexPartition, seed=seed)
    dt = time.perf_counter() - t0
    return part.membership, dt


def main():
    os.makedirs(TMP, exist_ok=True)
    nets = sorted(glob.glob(os.path.join(CORPUS, "*.net")),
                  key=lambda p: os.path.getsize(p))
    print(f"{'graph':<16}{'N':>8}{'Infomap AMI':>14}{'Leiden AMI':>13}"
          f"{'IM time(s)':>12}{'Leiden t(s)':>12}")
    rows = []
    for net in nets:
        name = os.path.splitext(os.path.basename(net))[0]
        lab = net.replace(".net", ".labels")
        if not os.path.exists(lab):
            continue
        truth_map = load_truth(lab)
        n, edges = read_net(net)
        g = ig.Graph(n=n, edges=edges)
        nodes = sorted(truth_map)
        truth = [truth_map[x] for x in nodes]

        im_ami, le_ami, im_t, le_t = [], [], [], []
        for s in SEEDS:
            pred, dt = infomap_run(net, s)
            im_ami.append(adjusted_mutual_info_score(truth, [pred.get(x, -1) for x in nodes]))
            im_t.append(dt)
            memb, dl = leiden_run(g, s)
            le_ami.append(adjusted_mutual_info_score(truth, [memb[x] for x in nodes]))
            le_t.append(dl)
        imt, let = np.median(im_t), np.median(le_t)
        rows.append((name, n, imt, let))
        print(f"{name:<16}{n:>8}{np.mean(im_ami):>14.4f}{np.mean(le_ami):>13.4f}"
              f"{imt:>12.3f}{let:>12.3f}")

    # Scaling exponent (fit log(time) ~ alpha*log(N)) on the mu03 series.
    series = sorted([r for r in rows if "mu03" in r[0]], key=lambda r: r[1])
    if len(series) >= 3:
        N = np.array([r[1] for r in series], float)
        imt = np.array([r[2] for r in series], float)
        let = np.array([r[3] for r in series], float)
        a_im = np.polyfit(np.log(N), np.log(imt), 1)[0]
        a_le = np.polyfit(np.log(N), np.log(let), 1)[0]
        print(f"\nScaling exponent (time ~ N^alpha) on mu=0.3 series:")
        print(f"  Infomap alpha={a_im:.2f}   Leiden alpha={a_le:.2f}   "
              f"(1.0=linear; lower is better scaling)")


if __name__ == "__main__":
    main()
