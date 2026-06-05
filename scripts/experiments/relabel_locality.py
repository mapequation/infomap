#!/usr/bin/env python3
"""Relabel a Pajek .net by node ordering to test the cache-locality hypothesis.

--order rcm      Reverse Cuthill-McKee: graph-adjacent nodes get nearby ids
--order random   random relabel (control: same shuffle cost, no locality)
--order identity copy through unchanged

If RCM lowers time-per-move-eval vs BOTH identity and random, the bottleneck is
memory locality of module-indexed access, not relabeling per se. Result-preserving
is NOT a goal here — this only confirms/quantifies the locality ceiling.
"""
import argparse
import numpy as np
from scipy.sparse import csr_matrix
from scipy.sparse.csgraph import reverse_cuthill_mckee


def read_net(path):
    verts, edges, section = [], [], None
    with open(path) as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            low = s.lower()
            if low.startswith("*vertices"):
                section = "v"
                continue
            if low.startswith(("*edges", "*arcs")):
                section = "e"
                continue
            if section == "v":
                verts.append(int(s.split()[0]))
            elif section == "e":
                p = s.split()
                edges.append((int(p[0]), int(p[1])))
    return verts, edges


def build_perm(n, edges, order, seed):
    if order == "identity":
        return np.arange(n)
    if order == "random":
        rng = np.random.default_rng(seed)
        p = np.arange(n)
        rng.shuffle(p)
        return p
    # rcm
    r = np.array([e[0] for e in edges])
    c = np.array([e[1] for e in edges])
    data = np.ones(len(edges))
    A = csr_matrix((data, (r, c)), shape=(n, n))
    A = A + A.T
    return reverse_cuthill_mckee(A.tocsr(), symmetric_mode=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inp")
    ap.add_argument("out")
    ap.add_argument("--order", choices=["rcm", "random", "identity"], default="rcm")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    verts, edges = read_net(args.inp)
    n = len(verts)
    id_to_idx = {v: i for i, v in enumerate(verts)}  # node ids -> 0..n-1
    eidx = [(id_to_idx[a], id_to_idx[b]) for a, b in edges]

    perm = build_perm(n, eidx, args.order, args.seed)  # perm[new_pos] = old_idx
    newpos = np.empty(n, dtype=np.int64)
    newpos[perm] = np.arange(n)  # newpos[old_idx] = new label

    with open(args.out, "w") as f:
        f.write(f"*Vertices {n}\n")
        for i in range(n):
            f.write(f'{i} "{i}"\n')
        f.write("*Edges\n")
        for a, b in eidx:
            f.write(f"{newpos[a]} {newpos[b]}\n")
    print(f"{args.order}: n={n} edges={len(eidx)} -> {args.out}")


if __name__ == "__main__":
    main()
