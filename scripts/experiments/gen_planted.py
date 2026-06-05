#!/usr/bin/env python3
"""Generate planted-partition (SBM) networks with ground-truth community labels.

Writes a Pajek .net and a .labels file (node_id<TAB>community) per network.
Deterministic given --seed. Node ids are 1-based to match Infomap/Pajek.
"""
import argparse
import os
import numpy as np


def gen_planted(n, k, p_in, p_out, rng):
    """n nodes, k equal-sized communities, intra prob p_in, inter prob p_out."""
    sizes = [n // k] * k
    for i in range(n - sum(sizes)):
        sizes[i] += 1
    labels = np.repeat(np.arange(k), sizes)[:n]
    rng.shuffle(labels)  # mix node ids across communities so structure isn't trivially ordered
    edges = []
    # upper triangle, vectorized per block-pair would be faster, but n<=20k here is fine
    for i in range(n):
        li = labels[i]
        # sample only j>i
        probs = np.where(labels[i + 1:] == li, p_in, p_out)
        draws = rng.random(probs.shape[0]) < probs
        for off in np.nonzero(draws)[0]:
            edges.append((i, i + 1 + off))
    return labels, edges


def write_net(path, n, edges):
    with open(path, "w") as f:
        f.write(f"*Vertices {n}\n")
        for i in range(n):
            f.write(f'{i + 1} "{i + 1}"\n')
        f.write(f"*Edges {len(edges)}\n")
        for a, b in edges:
            f.write(f"{a + 1} {b + 1} 1\n")


def write_labels(path, labels):
    with open(path, "w") as f:
        for i, c in enumerate(labels):
            f.write(f"{i + 1}\t{int(c)}\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="build/benchmarks/planted")
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    # (name, n, k, p_in, p_out) — mu = p_out controls difficulty (mixing).
    specs = [
        ("planted_easy_2k", 2000, 20, 0.05, 0.0008),
        ("planted_med_2k", 2000, 20, 0.04, 0.0020),
        ("planted_hard_2k", 2000, 20, 0.03, 0.0035),
        ("planted_easy_5k", 5000, 50, 0.04, 0.0004),
        ("planted_med_5k", 5000, 50, 0.03, 0.0010),
    ]
    rng = np.random.default_rng(args.seed)
    for name, n, k, p_in, p_out in specs:
        labels, edges = gen_planted(n, k, p_in, p_out, rng)
        write_net(os.path.join(args.out, f"{name}.net"), n, edges)
        write_labels(os.path.join(args.out, f"{name}.labels"), labels)
        deg = 2 * len(edges) / n
        print(f"{name}: n={n} k={k} edges={len(edges)} avg_deg={deg:.1f}")


if __name__ == "__main__":
    main()
