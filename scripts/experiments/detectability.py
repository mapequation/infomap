#!/usr/bin/env python3
"""Detectability sweep (fixed N, rising mu) with SYMMETRIC resolution tuning.

Run with the venv: /tmp/leidenenv/bin/python scripts/experiments/detectability.py

Where do the methods stop recovering planted structure as mixing rises? Fair version:
both sides get their resolution knob tuned — Infomap across markov times (its resolution
control), modularity across gamma (RB) and CPM across gamma. Infomap default (t=1) is
shown too, so the effect of markov-time tuning is visible.
"""
import glob
import os
import numpy as np
import igraph as ig
import leidenalg
from infomap import Infomap
from sklearn.metrics import adjusted_mutual_info_score
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

CORPUS = "build/benchmarks/lfr_mu"
OUTDIR = "build/benchmarks"
SEEDS = [1, 2, 3, 4, 5]
MARKOV_GRID = [0.25, 0.5, 0.75, 1.0, 1.5, 2.0]
RB_GRID = [0.5, 1, 2, 4, 8, 16, 32]
CPM_GRID = [1e-4, 3e-4, 1e-3, 3e-3, 1e-2, 3e-2, 1e-1, 3e-1]
os.environ["OMP_NUM_THREADS"] = "1"


def read_net(path):
    edges, n, sec = [], 0, None
    for line in open(path):
        s = line.strip()
        if not s:
            continue
        low = s.lower()
        if low.startswith("*vertices"):
            sec = "v"; p = s.split()
            if len(p) > 1:
                n = int(p[1])
            continue
        if low.startswith(("*edges", "*arcs")):
            sec = "e"; continue
        if sec == "v":
            n = max(n, int(s.split()[0]) + 1)
        elif sec == "e":
            a, b = s.split()[:2]; edges.append((int(a), int(b)))
    return n, edges


def load_truth(path):
    t = {}
    for line in open(path):
        nid, c = line.split(); t[int(nid)] = int(c)
    return t


def ami_im(mods, truth, nodes):
    return adjusted_mutual_info_score([truth[x] for x in nodes], [mods.get(x, -1) for x in nodes])


def ami_le(memb, truth, nodes):
    return adjusted_mutual_info_score([truth[x] for x in nodes], [memb[x] for x in nodes])


def infomap_ami(edges, n, seed, t, truth, nodes):
    im = Infomap(num_trials=1, seed=seed, two_level=True, markov_time=t, silent=True)
    im.add_nodes(range(n)); im.add_links(edges); im.run()
    return ami_im(im.get_modules(), truth, nodes)


def leiden_ami(g, seed, ptype, gamma, truth, nodes):
    m = leidenalg.find_partition(g, ptype, resolution_parameter=gamma, seed=seed, n_iterations=2).membership
    return ami_le(m, truth, nodes)


TUNE_SEEDS = [1, 2, 3]


def tune(score_one, grid):
    """Oracle-tune by MEAN AMI over TUNE_SEEDS (not a single seed) to avoid overfitting
    one noisy seed. Since the default param is in each grid, tuning can't underperform it."""
    best_p, best = grid[0], -1.0
    for p in grid:
        a = np.mean([score_one(p, s) for s in TUNE_SEEDS])
        if a > best:
            best, best_p = a, p
    return best_p


def main():
    nets = sorted(glob.glob(os.path.join(CORPUS, "*.net")),
                  key=lambda p: float(os.path.basename(p).split("mu")[-1].split(".")[0]))
    results = {}  # tool -> list of (mu, ami_mean, ami_std)
    print(f"{'mu':>5}{'infomap t=1':>14}{'infomap tuned':>15}{'leiden-RB tuned':>17}{'CPM tuned':>12}{'louvain':>10}")
    for net in nets:
        name = os.path.splitext(os.path.basename(net))[0]
        mu = int(name.split("mu")[-1]) / 100
        truth = load_truth(net.replace(".net", ".labels"))
        nodes = sorted(truth)
        n, edges = read_net(net)
        g = ig.Graph(n=n, edges=edges)

        t_star = tune(lambda t, s: infomap_ami(edges, n, s, t, truth, nodes), MARKOV_GRID)
        rb_star = tune(lambda r, s: leiden_ami(g, s, leidenalg.RBConfigurationVertexPartition, r, truth, nodes), RB_GRID)
        cpm_star = tune(lambda r, s: leiden_ami(g, s, leidenalg.CPMVertexPartition, r, truth, nodes), CPM_GRID)

        cols = {}
        cols["infomap t=1"] = [infomap_ami(edges, n, s, 1.0, truth, nodes) for s in SEEDS]
        cols["infomap tuned"] = [infomap_ami(edges, n, s, t_star, truth, nodes) for s in SEEDS]
        cols["leiden-RB tuned"] = [leiden_ami(g, s, leidenalg.RBConfigurationVertexPartition, rb_star, truth, nodes) for s in SEEDS]
        cols["CPM tuned"] = [leiden_ami(g, s, leidenalg.CPMVertexPartition, cpm_star, truth, nodes) for s in SEEDS]
        cols["louvain"] = [ami_le(g.community_multilevel().membership, truth, nodes) for _ in SEEDS]

        for tool, vals in cols.items():
            results.setdefault(tool, []).append((mu, np.mean(vals), np.std(vals)))
        print(f"{mu:>5.2f}{np.mean(cols['infomap t=1']):>14.3f}{np.mean(cols['infomap tuned']):>15.3f}"
              f"{np.mean(cols['leiden-RB tuned']):>17.3f}{np.mean(cols['CPM tuned']):>12.3f}"
              f"{np.mean(cols['louvain']):>10.3f}   (t*={t_star:g}, RBγ*={rb_star:g})")

    plt.figure(figsize=(7.5, 4.8))
    for tool, pts in results.items():
        pts.sort()
        mus = [p[0] for p in pts]; ami = [p[1] for p in pts]
        style = "--" if tool == "infomap t=1" else "o-"
        plt.plot(mus, ami, style, label=tool, markersize=5)
    plt.xlabel("mixing parameter mu (higher = weaker structure)")
    plt.ylabel("AMI vs ground truth")
    plt.title("Detectability sweep (LFR N=10k), symmetric resolution tuning")
    plt.legend(fontsize=8); plt.grid(alpha=0.3); plt.tight_layout()
    plt.savefig(f"{OUTDIR}/detectability.png", dpi=130); plt.close()
    print(f"\nWrote {OUTDIR}/detectability.png")


if __name__ == "__main__":
    main()
