#!/usr/bin/env python3
"""Generate uniform-block SBM networks (equal community sizes, fixed density) at scale.

Run with the venv (uses igraph's efficient SBM sampler):
  /tmp/leidenenv/bin/python scripts/experiments/gen_uniform.py

This is CPM's sweet spot: every community has the same size and internal density, so a
single CPM resolution fits all communities at every N (unlike LFR's power-law sizes,
where CPM collapsed at scale). Writes Pajek .net (0-based) + .labels per graph.
"""
import os
import random
import numpy as np
import igraph as ig

OUT = "build/benchmarks/uniform"
COMM_SIZE = 200      # fixed community size at all scales
DEG_IN = 16          # avg intra-community degree
DEG_EXT = 5          # avg inter-community degree  (mixing ~ 5/21 ~ 0.24)
SIZES_K = [10000, 50000, 100000]


def gen(n, seed):
    random.seed(seed)
    k = n // COMM_SIZE
    p_in = DEG_IN / (COMM_SIZE - 1)
    p_out = DEG_EXT / (n - COMM_SIZE)
    pref = np.full((k, k), p_out)
    np.fill_diagonal(pref, p_in)
    g = ig.Graph.SBM(pref.tolist(), [COMM_SIZE] * k)
    labels = np.repeat(np.arange(k), COMM_SIZE)
    return g, labels


def write(path_net, path_lab, g, labels):
    with open(path_net, "w") as f:
        f.write(f"*Vertices {g.vcount()}\n")
        for i in range(g.vcount()):
            f.write(f'{i} "{i}"\n')
        f.write("*Edges\n")
        for e in g.es:
            f.write(f"{e.source} {e.target}\n")
    with open(path_lab, "w") as f:
        for i, c in enumerate(labels):
            f.write(f"{i}\t{int(c)}\n")


def main():
    os.makedirs(OUT, exist_ok=True)
    for n in SIZES_K:
        g, labels = gen(n, seed=42)
        name = f"uniform_{n // 1000}k"
        write(os.path.join(OUT, f"{name}.net"), os.path.join(OUT, f"{name}.labels"), g, labels)
        print(f"{name}: n={n} k={n // COMM_SIZE} edges={g.ecount()} avg_deg={2 * g.ecount() / n:.1f}")


if __name__ == "__main__":
    main()
