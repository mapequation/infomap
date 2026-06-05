#!/usr/bin/env python3
"""Generate LFR benchmark networks with ground-truth communities.

LFR (Lancichinetti-Fortunato-Radicchi) graphs have realistic power-law degree and
community-size distributions and a tunable mixing parameter mu (fraction of a node's
edges that leave its community). The standard testbed for community detection.

Writes a Pajek .net (0-based, undirected) and a .labels file (node_id<TAB>community)
per network. Deterministic given the seed.
"""
import argparse
import os
import networkx as nx


def write_net(path, G, mapping):
    n = G.number_of_nodes()
    with open(path, "w") as f:
        f.write(f"*Vertices {n}\n")
        for i in range(n):
            f.write(f'{i} "{i}"\n')
        f.write("*Edges\n")
        for u, v in G.edges():
            if u == v:
                continue
            f.write(f"{mapping[u]} {mapping[v]}\n")


def write_labels(path, G, mapping):
    # Each node's community is a frozenset shared by its members; assign an id per set.
    comm_id = {}
    rows = []
    for node in G.nodes():
        comm = frozenset(G.nodes[node]["community"])
        if comm not in comm_id:
            comm_id[comm] = len(comm_id)
        rows.append((mapping[node], comm_id[comm]))
    rows.sort()
    with open(path, "w") as f:
        for nid, c in rows:
            f.write(f"{nid}\t{c}\n")
    return len(comm_id)


def gen(n, mu, seed, avg_deg=20, tau1=3.0, tau2=1.5):
    # networkx LFR is finicky; retry with larger min_community and varied seeds.
    for attempt in range(8):
        min_comm = 50 + attempt * 25
        try:
            G = nx.LFR_benchmark_graph(
                n, tau1, tau2, mu,
                average_degree=avg_deg,
                min_community=min_comm,
                max_community=max(min_comm * 6, n // 8),
                seed=seed + attempt,
                max_iters=1000,
            )
            return G
        except nx.ExceededMaxIterations:
            continue
        except Exception as e:
            print(f"    (retry {attempt}: {type(e).__name__})")
            continue
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="build/benchmarks/lfr")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--mu-series", action="store_true",
                    help="generate a fixed-N=10k detectability sweep across mu instead of the scaling series")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    if args.mu_series:
        specs = [(f"lfr_10k_mu{int(mu*100):02d}", 10000, mu)
                 for mu in (0.30, 0.40, 0.45, 0.50, 0.55, 0.60, 0.65)]
    else:
        # (name, n, mu) — mu is the mixing parameter (higher = harder).
        specs = [
            ("lfr_1k_mu01", 1000, 0.1),
            ("lfr_1k_mu03", 1000, 0.3),
            ("lfr_1k_mu05", 1000, 0.5),
            # scaling-curve series at fixed mu=0.3
            ("lfr_5k_mu03", 5000, 0.3),
            ("lfr_20k_mu03", 20000, 0.3),
            ("lfr_50k_mu03", 50000, 0.3),
            ("lfr_100k_mu03", 100000, 0.3),
        ]
    for name, n, mu in specs:
        G = gen(n, mu, args.seed)
        if G is None:
            print(f"{name}: FAILED to converge, skipped")
            continue
        G.remove_edges_from(nx.selfloop_edges(G))
        mapping = {node: i for i, node in enumerate(G.nodes())}
        write_net(os.path.join(args.out, f"{name}.net"), G, mapping)
        k = write_labels(os.path.join(args.out, f"{name}.labels"), G, mapping)
        print(f"{name}: n={n} mu={mu} edges={G.number_of_edges()} communities={k} "
              f"avg_deg={2*G.number_of_edges()/n:.1f}")


if __name__ == "__main__":
    main()
