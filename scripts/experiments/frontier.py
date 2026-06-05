#!/usr/bin/env python3
"""Local speed+quality frontier: python-infomap vs Leiden (modularity + CPM) vs Louvain.

Run with the venv (infomap, igraph, leidenalg, sklearn, matplotlib):
  /tmp/leidenenv/bin/python scripts/experiments/frontier.py

For local exploration. Honesty guards:
- In-memory via each tool's Python API; we time only the *partition call*, after a warmup.
- OMP_NUM_THREADS=1 for all (algorithmic comparison; the tools are serial anyway).
- Infomap run BOTH two-level (apples-to-apples with flat modularity) AND default multilevel.
- Leiden shown at its fast default (n_iterations=2) AND as CPM tuned per graph (the
  modularity family's own answer to the resolution limit; tuned = upper bound for CPM).
- Quality = AMI vs ground truth; speed = median core-call time over seeds.
Outputs a CSV and three plots under build/benchmarks/.
"""
import csv
import glob
import os
import time
import numpy as np
import igraph as ig
import leidenalg
from infomap import Infomap
from sklearn.metrics import adjusted_mutual_info_score
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

CORPUS = "build/benchmarks/lfr"
OUTDIR = "build/benchmarks"
SEEDS = [1, 2, 3, 4, 5]
# Fair tunable modularity = RBConfigurationVertexPartition (degree-corrected; handles
# LFR's heterogeneous community sizes, unlike CPM). Both oracle-tuned per graph (best AMI)
# = each method's BEST single-resolution case. Louvain (resolution=1) is the untuned baseline.
RB_GRID = [0.5, 1, 2, 4, 8, 16, 32, 64]
CPM_GRID = [1e-4, 3e-4, 1e-3, 3e-3, 1e-2, 3e-2, 1e-1, 3e-1]
os.environ["OMP_NUM_THREADS"] = "1"


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
                p = s.split()
                if len(p) > 1:
                    n = int(p[1])
                continue
            if low.startswith(("*edges", "*arcs")):
                section = "e"
                continue
            if section == "v":
                n = max(n, int(s.split()[0]) + 1)
            elif section == "e":
                a, b = s.split()[:2]
                edges.append((int(a), int(b)))
    return n, edges


def load_truth(path):
    truth = {}
    with open(path) as f:
        for line in f:
            nid, c = line.split()
            truth[int(nid)] = int(c)
    return truth


def timed(fn):
    fn()  # warmup
    t0 = time.perf_counter()
    out = fn()
    return out, time.perf_counter() - t0


def memb_to_pred(m, n):
    return m if isinstance(m, dict) else {i: m[i] for i in range(n)}


def ami_of(pred, truth, nodes):
    return adjusted_mutual_info_score(truth, [pred.get(x, -1) for x in nodes])


def run_infomap(edges, n, seed, two_level):
    def go():
        im = Infomap(num_trials=1, seed=seed, two_level=two_level, silent=True)
        im.add_nodes(range(n))
        im.add_links(edges)
        im.run()
        return im.get_modules()
    m, dt = timed(go)
    return m, dt, len(set(m.values()))


def run_leiden(g, seed, ptype, **kw):
    def go():
        return leidenalg.find_partition(g, ptype, seed=seed, n_iterations=2, **kw).membership
    m, dt = timed(go)
    return m, dt, len(set(m))


def run_louvain(g, seed):
    def go():
        return g.community_multilevel().membership
    m, dt = timed(go)
    return m, dt, len(set(m))


def tune_res(g, truth, nodes, n, ptype, grid):
    """Oracle-tune the resolution parameter (best AMI) = this method's best single-resolution case."""
    best_r, best_ami = grid[0], -1.0
    for r in grid:
        m = leidenalg.find_partition(g, ptype, resolution_parameter=r, seed=1,
                                     n_iterations=2).membership
        a = ami_of(memb_to_pred(m, n), truth, nodes)
        if a > best_ami:
            best_ami, best_r = a, r
    return best_r


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", default=CORPUS)
    ap.add_argument("--tag", default="")
    args = ap.parse_args()
    tag = ("_" + args.tag) if args.tag else ""
    nets = sorted(glob.glob(os.path.join(args.corpus, "*.net")), key=os.path.getsize)
    rows = []  # (graph, N, mu, tool, ami_mean, ami_std, time_med, k_med)
    for net in nets:
        name = os.path.splitext(os.path.basename(net))[0]
        lab = net.replace(".net", ".labels")
        if not os.path.exists(lab):
            continue
        mu = float(name.split("mu")[-1]) / 10 if "mu" in name else None
        truth_map = load_truth(lab)
        nodes = sorted(truth_map)
        truth = [truth_map[x] for x in nodes]
        n, edges = read_net(net)
        g = ig.Graph(n=n, edges=edges)
        rb = tune_res(g, truth, nodes, n, leidenalg.RBConfigurationVertexPartition, RB_GRID)
        cpm = tune_res(g, truth, nodes, n, leidenalg.CPMVertexPartition, CPM_GRID)

        tools = [
            ("infomap-2level", lambda s: run_infomap(edges, n, s, True)),
            ("infomap-multilevel", lambda s: run_infomap(edges, n, s, False)),
            ("louvain", lambda s: run_louvain(g, s)),
            ("leiden-modularity(tuned)",
             lambda s: run_leiden(g, s, leidenalg.RBConfigurationVertexPartition, resolution_parameter=rb)),
            ("leiden-CPM(tuned)",
             lambda s: run_leiden(g, s, leidenalg.CPMVertexPartition, resolution_parameter=cpm)),
        ]
        print(f"\n=== {name} (N={n}, k_true={len(set(truth))}, RB γ*={rb:g}, CPM γ*={cpm:g}) ===")
        print(f"{'tool':<24}{'AMI':>14}{'time(s)':>10}{'#comm':>8}")
        for label, runner in tools:
            amis, ts, ks = [], [], []
            for s in SEEDS:
                m, dt, k = runner(s)
                amis.append(ami_of(memb_to_pred(m, n), truth, nodes))
                ts.append(dt); ks.append(k)
            am, asd, tm, km = np.mean(amis), np.std(amis), np.median(ts), int(np.median(ks))
            rows.append((name, n, mu, label, am, asd, tm, km))
            print(f"{label:<24}{am:>8.4f}±{asd:<5.3f}{tm:>10.3f}{km:>8}")

    os.makedirs(OUTDIR, exist_ok=True)
    with open(os.path.join(OUTDIR, f"frontier{tag}.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["graph", "N", "mu", "tool", "ami_mean", "ami_std", "time_med", "k_med"])
        w.writerows(rows)
    make_plots(rows, tag)
    print(f"\nWrote {OUTDIR}/frontier{tag}.csv and plots (frontier{tag}_*.png)")


def make_plots(rows, tag=""):
    # series = the scaling graphs (LFR mu=0.3, or any corpus with one graph per N like uniform)
    insrs = lambda r: r[2] == 0.3 or r[2] is None
    tools = []
    for r in rows:
        if r[3] not in tools:
            tools.append(r[3])
    colors = dict(zip(tools, plt.cm.tab10.colors))
    series = sorted({(r[0], r[1]) for r in rows if insrs(r)}, key=lambda x: x[1])
    Ns = [s[1] for s in series]

    def by_tool(metric_idx, tool):
        out = []
        for _, N in series:
            v = [r for r in rows if r[1] == N and r[3] == tool and insrs(r)]
            out.append(v[0][metric_idx] if v else np.nan)
        return out

    # 1) AMI vs N (quality at scale)
    plt.figure(figsize=(7, 4.5))
    for t in tools:
        plt.plot(Ns, by_tool(4, t), "o-", label=t, color=colors[t])
    lbl = tag.strip("_") or "LFR mu=0.3"
    plt.xscale("log"); plt.xlabel("N (nodes)"); plt.ylabel("AMI vs ground truth")
    plt.title(f"Quality at scale ({lbl})"); plt.legend(fontsize=8); plt.grid(alpha=0.3)
    plt.tight_layout(); plt.savefig(f"{OUTDIR}/frontier{tag}_ami_vs_n.png", dpi=130); plt.close()

    # 2) time vs N (speed scaling)
    plt.figure(figsize=(7, 4.5))
    for t in tools:
        plt.plot(Ns, by_tool(6, t), "o-", label=t, color=colors[t])
    plt.xscale("log"); plt.yscale("log"); plt.xlabel("N (nodes)"); plt.ylabel("core time (s)")
    plt.title(f"Speed scaling ({lbl}, single-thread)"); plt.legend(fontsize=8); plt.grid(alpha=0.3)
    plt.tight_layout(); plt.savefig(f"{OUTDIR}/frontier{tag}_time_vs_n.png", dpi=130); plt.close()

    # 3) frontier scatter at largest N (time vs AMI)
    Nmax = max(Ns)
    plt.figure(figsize=(7, 4.5))
    for t in tools:
        v = [r for r in rows if r[1] == Nmax and r[3] == t and insrs(r)]
        if v:
            plt.scatter(v[0][6], v[0][4], s=80, color=colors[t], zorder=3)
            plt.annotate(t, (v[0][6], v[0][4]), fontsize=8,
                         xytext=(5, 4), textcoords="offset points")
    plt.xscale("log"); plt.xlabel("core time (s) — log"); plt.ylabel("AMI vs ground truth")
    plt.title(f"Speed/quality frontier at N={Nmax} ({lbl})\n↖ better"); plt.grid(alpha=0.3)
    plt.tight_layout(); plt.savefig(f"{OUTDIR}/frontier{tag}_scatter_{Nmax}.png", dpi=130); plt.close()


if __name__ == "__main__":
    main()
