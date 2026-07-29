#!/usr/bin/env python3
"""Vectorized (numpy/scipy) implementation of the atlas equation search.

Why this exists: the atlas equation's additive separability makes a fully
vectorized batch search *sound*, not just possible. Each sweep proposes the
best move for every dirty node against the current per-module aggregates
(two sparse matmuls over the dirty rows), then accepts a maximal set of
proposals whose module pairs are pairwise disjoint. By the locality theorem
(README.md, Theorems 2-3), every delta in such a batch stays exact when the
whole batch is applied at once, so the accepted deltas sum to the true
codelength change and the per-module aggregates of the touched modules can
simply be *assigned* their precomputed after-values -- the vectorized twin of
the two-lock commit in bench/parallel_bench.cpp. The same batch scheme under
the map equation is inexact, because applying any move reprices the global
plogp(q) term for every other move in the batch; `demo_map_batch_drift`
measures that drift.

Everything is in bits (log2). Directed and undirected flows supported;
undirected networks are represented as symmetric flow matrices.

Usage:  python3 atlas_numpy.py [--quick]
"""

from __future__ import annotations

import argparse
import time

import numpy as np
from scipy import sparse

RNG = np.random.default_rng


def plogp(x: np.ndarray) -> np.ndarray:
    return np.where(x > 1e-300, x * np.log2(np.maximum(x, 1e-300)), 0.0)


# ---------------------------------------------------------------------------
# Flow networks: sparse flow matrix F (F[u, v] = walk flow u -> v) + p
# ---------------------------------------------------------------------------


def undirected_flow(n: int, u, v, w) -> tuple[sparse.csr_array, np.ndarray]:
    """Edge lists -> symmetric flow matrix with entries w/(2W), p = strength/2W.

    Self-links are dropped, matching the pure-python prototype. They must not
    survive: `propose()` classifies each incident link as going to the old
    module, the new module, or elsewhere, and a self-link would be counted on
    both sides of a move without the compensating -F[x, x] term, silently
    corrupting the boundary-after values.
    """
    u = np.asarray(u)
    v = np.asarray(v)
    w = np.asarray(w, dtype=float)
    keep = u != v
    u, v, w = u[keep], v[keep], w[keep]
    total = w.sum()
    rows = np.concatenate([u, v])
    cols = np.concatenate([v, u])
    data = np.concatenate([w, w]) / (2.0 * total)
    f = sparse.csr_array((data, (rows, cols)), shape=(n, n))
    f.sum_duplicates()
    p = np.asarray(f.sum(axis=1)).ravel()
    return f, p


def planted_partition(
    num_blocks: int, block_size: int, k_in: float, k_out: float, seed: int
):
    """Sparse planted partition, vectorized generation."""
    rng = RNG(seed)
    n = num_blocks * block_size
    # within-block edges: sample once per block from the upper triangle
    m_in = rng.poisson(k_in * block_size / 2.0, size=num_blocks)
    us, vs = [], []
    for b in range(num_blocks):
        cnt = m_in[b]
        a = rng.integers(0, block_size, size=2 * cnt).reshape(2, -1)
        keep = a[0] != a[1]
        us.append(b * block_size + np.minimum(a[0][keep], a[1][keep]))
        vs.append(b * block_size + np.maximum(a[0][keep], a[1][keep]))
    # cross-block edges: uniform pairs, keep those crossing blocks
    m_out = rng.poisson(k_out * n / 2.0)
    a = rng.integers(0, n, size=2 * m_out).reshape(2, -1)
    keep = a[0] // block_size != a[1] // block_size
    us.append(np.minimum(a[0][keep], a[1][keep]))
    vs.append(np.maximum(a[0][keep], a[1][keep]))
    u = np.concatenate(us)
    v = np.concatenate(vs)
    pairs = np.unique(u.astype(np.int64) * n + v)
    u, v = pairs // n, pairs % n
    truth = np.arange(n) // block_size
    f, p = undirected_flow(n, u, v, np.ones(len(u)))
    return f, p, truth


# ---------------------------------------------------------------------------
# Objectives from per-module aggregates (vectorized)
# ---------------------------------------------------------------------------


def module_aggregates(f: sparse.csr_array, p: np.ndarray, module: np.ndarray, m: int):
    """P, Qin, Qout per module id in [0, m)."""
    coo = f.tocoo()
    cross = module[coo.row] != module[coo.col]
    big_p = np.bincount(module, weights=p, minlength=m)
    q_out = np.bincount(module[coo.row[cross]], weights=coo.data[cross], minlength=m)
    q_in = np.bincount(module[coo.col[cross]], weights=coo.data[cross], minlength=m)
    return big_p, q_in, q_out


def l_map(big_p, q_in, q_out, node_entropy: float) -> float:
    q = q_in.sum()
    index = plogp(np.array([q]))[0] - plogp(q_in).sum()
    modules = (plogp(big_p + q_out) - plogp(q_out)).sum()
    return float(index + modules + node_entropy)


def l_atlas(big_p, q_in, q_out, node_entropy: float) -> float:
    mask = (q_in > 0.0) & (big_p > 0.0)
    index = -(q_in[mask] * np.log2(big_p[mask])).sum()
    modules = (plogp(big_p + q_out) - plogp(q_out)).sum()
    return float(index + modules + node_entropy)


def g_atlas(big_p, q_in, q_out):
    """Per-module functional of the atlas equation, elementwise."""
    idx = np.where(
        (q_in > 1e-300) & (big_p > 1e-300),
        -q_in * np.log2(np.maximum(big_p, 1e-300)),
        0.0,
    )
    return idx + plogp(big_p + q_out) - plogp(q_out)


def kl_gap(big_p, q_in) -> float:
    q = q_in.sum()
    if q <= 0.0:
        return 0.0
    mask = q_in > 0.0
    return float((q_in[mask] * np.log2((q_in[mask] / q) / big_p[mask])).sum())


# ---------------------------------------------------------------------------
# One vectorized sweep: propose for dirty nodes, accept a disjoint batch
# ---------------------------------------------------------------------------


def candidate_link_sums(f, ft, rows_idx, module, m):
    """For every (dirty node, adjacent module) pair: flow node->module (k_out)
    and module->node (k_in), aligned over the union sparsity. Row indices in
    the returned arrays are local to `rows_idx`."""
    n = f.shape[0]
    h = sparse.csr_array((np.ones(n), (np.arange(n), module)), shape=(n, m))
    k_out = (f[rows_idx] @ h).tocoo()
    k_in = (ft[rows_idx] @ h).tocoo()
    stride = np.int64(m)
    keys = np.concatenate(
        [
            k_out.row.astype(np.int64) * stride + k_out.col,
            k_in.row.astype(np.int64) * stride + k_in.col,
        ]
    )
    uniq, inverse = np.unique(keys, return_inverse=True)
    out_aligned = np.zeros(len(uniq))
    in_aligned = np.zeros(len(uniq))
    np.add.at(out_aligned, inverse[: len(k_out.data)], k_out.data)
    np.add.at(in_aligned, inverse[len(k_out.data) :], k_in.data)
    return (
        (uniq // stride).astype(np.int64),
        (uniq % stride).astype(np.int64),
        out_aligned,
        in_aligned,
    )


def propose(f, ft, dirty_idx, p, w_out, w_in, module, big_p, q_in, q_out, objective):
    """Best improving move per dirty node. Returns a dict of aligned arrays:
    node, old, new, delta, and the exact after-aggregates of both modules."""
    m = len(big_p)
    local_row, col, k_out, k_in = candidate_link_sums(f, ft, dirty_idx, module, m)
    row = dirty_idx[local_row]

    own = module[row]
    k_own_out = np.zeros(len(dirty_idx))
    k_own_in = np.zeros(len(dirty_idx))
    own_mask = col == own
    k_own_out[local_row[own_mask]] = k_out[own_mask]
    k_own_in[local_row[own_mask]] = k_in[own_mask]

    cand = ~own_mask
    local_row, row, col = local_row[cand], row[cand], col[cand]
    k_out, k_in = k_out[cand], k_in[cand]
    own = module[row]

    pv, wo, wi = p[row], w_out[row], w_in[row]
    koa, kia = k_own_out[local_row], k_own_in[local_row]

    old_p = big_p[own] - pv
    old_qi = q_in[own] - (wi - kia) + koa
    old_qo = q_out[own] - (wo - koa) + kia
    new_p = big_p[col] + pv
    new_qi = q_in[col] + (wi - k_in) - k_out
    new_qo = q_out[col] + (wo - k_out) - k_in

    if objective == "atlas":
        delta = (
            g_atlas(old_p, old_qi, old_qo)
            - g_atlas(big_p[own], q_in[own], q_out[own])
            + g_atlas(new_p, new_qi, new_qo)
            - g_atlas(big_p[col], q_in[col], q_out[col])
        )
    else:  # map equation: per-module part + global plogp(q) repricing

        def g_map(bp, qi, qo):
            return -plogp(qi) + plogp(bp + qo) - plogp(qo)

        q_tot = q_in.sum()
        q_after = q_tot - q_in[own] - q_in[col] + old_qi + new_qi
        delta = (
            plogp(q_after)
            - plogp(np.array([q_tot]))[0]
            + g_map(old_p, old_qi, old_qo)
            - g_map(big_p[own], q_in[own], q_out[own])
            + g_map(new_p, new_qi, new_qo)
            - g_map(big_p[col], q_in[col], q_out[col])
        )

    improving = delta < -1e-12
    if not improving.any():
        return None
    idx = np.flatnonzero(improving)
    row_f, delta_f = row[idx], delta[idx]
    # best candidate per node: sort by (node, delta), take first per node
    order = np.lexsort((delta_f, row_f))
    first = np.unique(row_f[order], return_index=True)[1]
    best_local = order[first]
    best = idx[best_local]
    return {
        "node": row[best],
        "old": own[best],
        "new": col[best],
        "delta": delta_f[best_local],
        "old_p": old_p[best],
        "old_qi": old_qi[best],
        "old_qo": old_qo[best],
        "new_p": new_p[best],
        "new_qi": new_qi[best],
        "new_qo": new_qo[best],
    }


def disjoint_batch(prop, m, rng):
    """Greedy-maximal set of proposals with pairwise-disjoint module pairs:
    repeat the vectorized first-occurrence filter, dropping proposals that
    share a module with anything accepted so far. Sound: within the batch
    every delta stays exact (locality theorem)."""
    delta = prop["delta"]
    order = np.lexsort((rng.random(len(delta)), delta))
    used = np.zeros(m, dtype=bool)
    selected = []
    remaining = order
    while len(remaining) > 0:
        old, new = prop["old"][remaining], prop["new"][remaining]
        flat = np.stack([old, new], axis=1).ravel()
        first_pos = np.zeros(m, dtype=np.int64)
        uniq, first = np.unique(flat, return_index=True)
        first_pos[uniq] = first
        is_first = first_pos[flat] == np.arange(len(flat))
        accept = is_first.reshape(-1, 2).all(axis=1)
        acc = remaining[accept]
        if len(acc) == 0:
            break
        selected.append(acc)
        used[prop["old"][acc]] = True
        used[prop["new"][acc]] = True
        rem = remaining[~accept]
        keep = ~(used[prop["old"][rem]] | used[prop["new"][rem]])
        remaining = rem[keep]
    return np.concatenate(selected) if selected else np.array([], dtype=np.int64)


def batch_local_moving(f, ft, p, module, objective: str, rng, max_sweeps=1000):
    """Vectorized local moving with dirty-node tracking and incremental
    aggregate assignment. Returns (module, level_consistency) where the
    consistency is |L_recomputed - (L_init + sum of all accepted deltas)|."""
    n = f.shape[0]
    m = n
    w_out = np.asarray(f.sum(axis=1)).ravel()
    w_in = np.asarray(f.sum(axis=0)).ravel()
    big_p, q_in, q_out = module_aggregates(f, p, module, m)
    l_fn = l_atlas if objective == "atlas" else l_map
    l_tracked = l_fn(big_p, q_in, q_out, 0.0)
    dirty_idx = np.arange(n)
    verified = False
    for _ in range(max_sweeps):
        prop = (
            propose(
                f, ft, dirty_idx, p, w_out, w_in, module, big_p, q_in, q_out, objective
            )
            if len(dirty_idx) > 0
            else None
        )
        if prop is None:
            # Neighbour-of-mover dirty marking alone is not sufficient for
            # convergence: a move changes the aggregates of both modules, so
            # non-adjacent nodes bordering them get stale deltas too. Confirm
            # the local optimum with a full sweep before stopping.
            if verified:
                break
            dirty_idx = np.arange(n)
            verified = True
            continue
        verified = False
        sel = disjoint_batch(prop, m, rng)
        nodes = prop["node"][sel]
        module[nodes] = prop["new"][sel]
        # Disjointness makes the update a plain assignment of after-values.
        big_p[prop["old"][sel]] = prop["old_p"][sel]
        q_in[prop["old"][sel]] = prop["old_qi"][sel]
        q_out[prop["old"][sel]] = prop["old_qo"][sel]
        big_p[prop["new"][sel]] = prop["new_p"][sel]
        q_in[prop["new"][sel]] = prop["new_qi"][sel]
        q_out[prop["new"][sel]] = prop["new_qo"][sel]
        l_tracked += prop["delta"][sel].sum()
        # movers and their neighbourhoods become dirty
        dirty_idx = np.unique(
            np.concatenate([nodes, f[nodes].indices, ft[nodes].indices])
        )
    big_p2, q_in2, q_out2 = module_aggregates(f, p, module, m)
    consistency = abs(l_fn(big_p2, q_in2, q_out2, 0.0) - l_tracked)
    return module, consistency


def louvain(f, p, objective: str, seed=1):
    """Batch local moving + aggregation until no shrink. Returns the leaf
    assignment and the worst per-level tracking consistency."""
    rng = RNG(seed)
    leaf = np.arange(f.shape[0])
    worst = 0.0
    while True:
        ft = f.T.tocsr()
        module = np.arange(f.shape[0])
        module, consistency = batch_local_moving(f, ft, p, module, objective, rng)
        worst = max(worst, consistency)
        ids, compact = np.unique(module, return_inverse=True)
        leaf = compact[leaf]
        if len(ids) == f.shape[0] or len(ids) <= 1:
            break
        # aggregate: F <- H^T F H with the diagonal (intra flow) dropped
        m = len(ids)
        h = sparse.csr_array(
            (np.ones(f.shape[0]), (np.arange(f.shape[0]), compact)),
            shape=(f.shape[0], m),
        )
        coarse = (h.T @ f @ h).tocoo()
        off = coarse.row != coarse.col
        f = sparse.csr_array(
            (coarse.data[off], (coarse.row[off], coarse.col[off])), shape=(m, m)
        )
        p = np.bincount(compact, weights=p, minlength=m)
    return leaf, worst


# ---------------------------------------------------------------------------
# Experiments
# ---------------------------------------------------------------------------


def check(label, ok, detail=""):
    status = "PASS" if ok else "FAIL"
    print(f"  [{status}] {label}{'  (' + detail + ')' if detail else ''}")
    return ok


def experiment_equivalence():
    """Numpy codelengths == pure-python prototype codelengths."""
    import atlas_prototype as pp

    print("== 1. Agreement with the pure-python prototype ==")
    rng = RNG(3)
    ok = True
    for trial in range(5):
        n = int(rng.integers(20, 60))
        m_edges = int(rng.integers(2 * n, 4 * n))
        u = rng.integers(0, n, size=m_edges)
        v = rng.integers(0, n, size=m_edges)
        keep = u != v
        u, v = u[keep], v[keep]
        w = rng.uniform(0.5, 3.0, size=len(u))
        net = pp.FlowNetwork.from_undirected(
            n, list(zip(u.tolist(), v.tolist(), w.tolist(), strict=True))
        )
        f, p = undirected_flow(n, u, v, w)
        assignment = rng.integers(0, max(2, n // 5), size=n)
        part = pp.Partition(net, assignment.tolist())
        big_p, q_in, q_out = module_aggregates(
            f, p, assignment, int(assignment.max()) + 1
        )
        node_entropy = -plogp(p).sum()
        d_map = abs(l_map(big_p, q_in, q_out, node_entropy) - part.L_map())
        d_atlas = abs(l_atlas(big_p, q_in, q_out, node_entropy) - part.L_atlas())
        identity = abs(
            l_atlas(big_p, q_in, q_out, 0.0)
            - l_map(big_p, q_in, q_out, 0.0)
            - kl_gap(big_p, q_in)
        )
        ok &= check(
            f"trial {trial}: L_map, L_atlas, KL identity",
            d_map < 1e-10 and d_atlas < 1e-10 and identity < 1e-10,
            f"diffs {d_map:.1e} {d_atlas:.1e} {identity:.1e}",
        )
    return ok


def experiment_karate():
    import atlas_prototype as pp

    print("\n== 2. Karate club, batch search (20 restarts) ==")
    edges = [(u, v, 1.0) for u, v in pp.KARATE_EDGES]
    u, v, w = (np.array(x) for x in zip(*edges, strict=True))
    f, p = undirected_flow(34, u, v, w)
    node_entropy = -plogp(p).sum()
    for objective in ("map", "atlas"):
        best_l, best_modules = np.inf, 0
        for seed in range(1, 21):
            leaf, _ = louvain(f, p, objective, seed=seed)
            big_p, q_in, q_out = module_aggregates(f, p, leaf, int(leaf.max()) + 1)
            fn = l_atlas if objective == "atlas" else l_map
            val = fn(big_p, q_in, q_out, node_entropy)
            if val < best_l:
                best_l, best_modules = val, int(leaf.max()) + 1
        print(f"  {objective:<5}: best L = {best_l:.4f} bits, {best_modules} modules")


def experiment_scale(quick: bool):
    blocks, size = (50, 400) if quick else (200, 1000)
    print(f"\n== 3. Scale + batch soundness: planted partition {blocks}x{size} ==")
    f, p, _truth = planted_partition(blocks, size, 8.0, 2.0, seed=1)
    node_entropy = -plogp(p).sum()
    print(f"  {f.shape[0]} nodes, {f.nnz // 2} edges, one-level L = {node_entropy:.4f}")
    for objective in ("atlas", "map"):
        t0 = time.perf_counter()
        leaf, worst = louvain(f, p, objective, seed=1)
        dt = time.perf_counter() - t0
        big_p, q_in, q_out = module_aggregates(f, p, leaf, int(leaf.max()) + 1)
        print(
            f"  {objective:<5}: {dt:6.1f} s, {int(leaf.max()) + 1:>4} modules, "
            f"L_map = {l_map(big_p, q_in, q_out, node_entropy):.6f}, "
            f"L_atlas = {l_atlas(big_p, q_in, q_out, node_entropy):.6f}, "
            f"level consistency = {worst:.2e}"
        )
    print("  (atlas batches are exact by construction; map batches drift, see 4.)")


def demo_map_batch_drift(quick: bool):
    """One batch sweep from singletons under both objectives: the accepted
    deltas sum to the true codelength change for the atlas equation only."""
    blocks, size = (50, 400) if quick else (100, 1000)
    print(f"\n== 4. Batch exactness, single sweep from singletons ({blocks}x{size}) ==")
    f, p, _truth = planted_partition(blocks, size, 8.0, 2.0, seed=2)
    ft = f.T.tocsr()
    n = f.shape[0]
    w_out = np.asarray(f.sum(axis=1)).ravel()
    w_in = np.asarray(f.sum(axis=0)).ravel()
    for objective in ("atlas", "map"):
        rng = RNG(7)
        module = np.arange(n)
        big_p, q_in, q_out = module_aggregates(f, p, module, n)
        l_fn = l_atlas if objective == "atlas" else l_map
        l0 = l_fn(big_p, q_in, q_out, 0.0)
        prop = propose(
            f, ft, np.arange(n), p, w_out, w_in, module, big_p, q_in, q_out, objective
        )
        if prop is None:
            print(f"  {objective:<5}: no improving moves from singletons")
            continue
        sel = disjoint_batch(prop, n, rng)
        module[prop["node"][sel]] = prop["new"][sel]
        big_p, q_in, q_out = module_aggregates(f, p, module, n)
        l1 = l_fn(big_p, q_in, q_out, 0.0)
        drift = abs(l1 - (l0 + prop["delta"][sel].sum()))
        print(
            f"  {objective:<5}: {len(sel):>6} moves in one batch, "
            f"|dL - sum(deltas)| = {drift:.2e}"
        )


def main():
    import sys

    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    args = ap.parse_args()

    ok = experiment_equivalence()
    experiment_karate()
    experiment_scale(args.quick)
    demo_map_batch_drift(args.quick)
    if not ok:
        sys.exit(1)


if __name__ == "__main__":
    main()
