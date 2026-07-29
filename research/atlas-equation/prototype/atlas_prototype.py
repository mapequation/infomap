#!/usr/bin/env python3
"""Numerical validation of the atlas equation prototype.

The atlas equation is a block-autonomous variant of the map equation: the
adaptive index codebook (Shannon-optimal for the module entry rates, hence
coupled across all modules through the total boundary flow) is replaced by
static module addresses of length -log2(P_i), where P_i is the stationary
flow mass of module i. See research/atlas-equation/README.md for the theory.

This script validates, with no dependencies beyond the standard library:

  1. Exactness of the local move deltas for both objectives
     (delta == recomputed-from-scratch difference).
  2. The identity  L_atlas = L_map + q * KL(entry rates || module masses).
  3. Search behavior: Louvain-style two-level search under both objectives
     on small reference networks (karate club, ring of cliques, planted
     partitions, and the repository's example networks).
  4. Agreement with the Infomap binary on undirected example networks
     (validates the map equation implementation used as the baseline here).

Everything is measured in bits (log base 2), matching Infomap.

Usage:  python3 atlas_prototype.py [--quick] [--infomap-binary PATH]
"""

from __future__ import annotations

import argparse
import math
import os
import random
import subprocess
import sys
import tempfile
from collections import defaultdict


def plogp(x: float) -> float:
    return x * math.log2(x) if x > 1e-300 else 0.0


def entropy(dist) -> float:
    return -sum(plogp(x) for x in dist)


# ---------------------------------------------------------------------------
# Flow networks: stationary node visit rates + directed link flows
# ---------------------------------------------------------------------------


class FlowNetwork:
    """Node visit rates p (summing to 1) and directed link flows.

    For undirected input each edge (u, v, w) carries flow w / (2W) in each
    direction, and p is proportional to node strength, matching Infomap's
    undirected flow model. For directed input, p is the PageRank vector with
    unrecorded uniform teleportation and link flows are the smoothed walk
    flows p_u * w_uv / s_u (teleportation steps are not encoded), matching
    Infomap's default directed flow model up to the teleportation target
    distribution.
    """

    def __init__(self, n, p, links):
        self.n = n
        self.p = p
        self.links = links  # list of (u, v, flow), u != v
        self.out_links = [[] for _ in range(n)]
        self.in_links = [[] for _ in range(n)]
        for u, v, f in links:
            self.out_links[u].append((v, f))
            self.in_links[v].append((u, f))

    @staticmethod
    def from_undirected(n, edges):
        strength = [0.0] * n
        total = 0.0
        for u, v, w in edges:
            if u == v:
                continue
            strength[u] += w
            strength[v] += w
            total += w
        p = [s / (2.0 * total) for s in strength]
        links = []
        for u, v, w in edges:
            if u == v:
                continue
            f = w / (2.0 * total)
            links.append((u, v, f))
            links.append((v, u, f))
        return FlowNetwork(n, p, links)

    @staticmethod
    def from_directed(n, edges, tau=0.15, tol=1e-14, max_iter=10_000):
        out_strength = [0.0] * n
        for u, v, w in edges:
            if u == v:
                continue
            out_strength[u] += w
        out_norm = [[] for _ in range(n)]
        for u, v, w in edges:
            if u == v:
                continue
            out_norm[u].append((v, w / out_strength[u]))
        p = [1.0 / n] * n
        for _ in range(max_iter):
            nxt = [0.0] * n
            dangling = 0.0
            for u in range(n):
                if out_norm[u]:
                    share = (1.0 - tau) * p[u]
                    for v, w in out_norm[u]:
                        nxt[v] += share * w
                else:
                    dangling += p[u]
            base = (tau * (1.0 - dangling) + dangling) / n
            nxt = [x + base for x in nxt]
            # dangling mass and teleportation are spread uniformly
            err = sum(abs(a - b) for a, b in zip(nxt, p, strict=True))
            p = nxt
            if err < tol:
                break
        links = []
        for u in range(n):
            for v, w in out_norm[u]:
                links.append((u, v, (1.0 - tau) * p[u] * w))
        return FlowNetwork(n, p, links)

    def aggregate(self, module_of):
        """Aggregate to the module-level flow network (self-flows dropped from
        links; they can never contribute to a coarser boundary)."""
        modules = sorted(set(module_of))
        remap = {m: i for i, m in enumerate(modules)}
        np_ = [0.0] * len(modules)
        for node, m in enumerate(module_of):
            np_[remap[m]] += self.p[node]
        flow = defaultdict(float)
        for u, v, f in self.links:
            mu, mv = remap[module_of[u]], remap[module_of[v]]
            if mu != mv:
                flow[(mu, mv)] += f
        links = [(u, v, f) for (u, v), f in flow.items()]
        return FlowNetwork(len(modules), np_, links), remap


# ---------------------------------------------------------------------------
# Two-level objectives over a partition
# ---------------------------------------------------------------------------


class Partition:
    """Module assignment plus the per-module aggregates both objectives need:
    P (flow mass), Qin (enter flow), Qout (exit flow)."""

    def __init__(self, net: FlowNetwork, assignment):
        self.net = net
        self.module = list(assignment)
        m = max(self.module) + 1
        self.P = [0.0] * m
        self.Qin = [0.0] * m
        self.Qout = [0.0] * m
        for node, mod in enumerate(self.module):
            self.P[mod] += net.p[node]
        for u, v, f in net.links:
            if self.module[u] != self.module[v]:
                self.Qout[self.module[u]] += f
                self.Qin[self.module[v]] += f
        self.node_entropy = -sum(plogp(x) for x in net.p)

    # -- codelengths ---------------------------------------------------------

    def L_map(self) -> float:
        q = sum(self.Qin)
        index = plogp(q) - sum(plogp(x) for x in self.Qin)
        modules = sum(
            plogp(self.P[i] + self.Qout[i]) - plogp(self.Qout[i])
            for i in range(len(self.P))
        )
        return index + modules + self.node_entropy

    def L_atlas(self) -> float:
        index = -sum(
            self.Qin[i] * math.log2(self.P[i]) if self.Qin[i] > 0.0 else 0.0
            for i in range(len(self.P))
        )
        modules = sum(
            plogp(self.P[i] + self.Qout[i]) - plogp(self.Qout[i])
            for i in range(len(self.P))
        )
        return index + modules + self.node_entropy

    def kl_gap(self) -> float:
        """q * KL(entry distribution || module mass distribution)."""
        q = sum(self.Qin)
        if q <= 0.0:
            return 0.0
        return sum(
            qi * math.log2((qi / q) / self.P[i])
            for i, qi in enumerate(self.Qin)
            if qi > 0.0
        )

    # -- local moves ---------------------------------------------------------

    def _link_sums(self, node):
        """Flow between `node` and each adjacent module: (to_mod, from_mod)."""
        to_mod = defaultdict(float)
        from_mod = defaultdict(float)
        for v, f in self.net.out_links[node]:
            to_mod[self.module[v]] += f
        for u, f in self.net.in_links[node]:
            from_mod[self.module[u]] += f
        return to_mod, from_mod

    def _module_terms_after(self, node, old, new, to_mod, from_mod):
        """Aggregates of the two touched modules before/after moving node."""
        p = self.net.p[node]
        node_out = sum(f for _, f in self.net.out_links[node])
        node_in = sum(f for _, f in self.net.in_links[node])
        # Old module after removal: links node<->old become boundary,
        # links node<->elsewhere stop counting against old.
        old_after = (
            self.P[old] - p,
            self.Qin[old] - (node_in - from_mod[old]) + to_mod[old],
            self.Qout[old] - (node_out - to_mod[old]) + from_mod[old],
        )
        new_after = (
            self.P[new] + p,
            self.Qin[new] + (node_in - from_mod[new]) - to_mod[new],
            self.Qout[new] + (node_out - to_mod[new]) - from_mod[new],
        )
        return old_after, new_after

    @staticmethod
    def _g_atlas(P, Qin, Qout):
        idx = -Qin * math.log2(P) if (Qin > 1e-300 and P > 1e-300) else 0.0
        return idx + plogp(P + Qout) - plogp(Qout)

    def delta_atlas(self, node, new):
        """Exact codelength change of moving node to module `new`.
        Touches only the two module aggregates -- no global quantity."""
        old = self.module[node]
        if old == new:
            return 0.0
        to_mod, from_mod = self._link_sums(node)
        (oP, oQi, oQo), (nP, nQi, nQo) = self._module_terms_after(
            node, old, new, to_mod, from_mod
        )
        before = self._g_atlas(
            self.P[old], self.Qin[old], self.Qout[old]
        ) + self._g_atlas(self.P[new], self.Qin[new], self.Qout[new])
        after = self._g_atlas(oP, oQi, oQo) + self._g_atlas(nP, nQi, nQo)
        return after - before

    def delta_map(self, node, new):
        """Exact codelength change under the map equation. Note the extra
        global term through q = sum(Qin): every move reprices it."""
        old = self.module[node]
        if old == new:
            return 0.0
        to_mod, from_mod = self._link_sums(node)
        (oP, oQi, oQo), (nP, nQi, nQo) = self._module_terms_after(
            node, old, new, to_mod, from_mod
        )
        q_before = sum(self.Qin)
        q_after = q_before - self.Qin[old] - self.Qin[new] + oQi + nQi

        def g(P, Qin, Qout):
            return -plogp(Qin) + plogp(P + Qout) - plogp(Qout)

        before = (
            plogp(q_before)
            + g(self.P[old], self.Qin[old], self.Qout[old])
            + g(self.P[new], self.Qin[new], self.Qout[new])
        )
        after = plogp(q_after) + g(oP, oQi, oQo) + g(nP, nQi, nQo)
        return after - before

    def move(self, node, new):
        old = self.module[node]
        if old == new:
            return
        to_mod, from_mod = self._link_sums(node)
        (
            (self.P[old], self.Qin[old], self.Qout[old]),
            (
                self.P[new],
                self.Qin[new],
                self.Qout[new],
            ),
        ) = self._module_terms_after(node, old, new, to_mod, from_mod)
        self.module[node] = new


# ---------------------------------------------------------------------------
# Louvain-style two-level search (objective-agnostic)
# ---------------------------------------------------------------------------


def local_moving(net, objective, rng, max_sweeps=100):
    part = Partition(net, list(range(net.n)))
    delta = part.delta_atlas if objective == "atlas" else part.delta_map
    order = list(range(net.n))
    moved_any = False
    for _ in range(max_sweeps):
        rng.shuffle(order)
        moved = 0
        for node in order:
            candidates = set()
            for v, _ in net.out_links[node]:
                candidates.add(part.module[v])
            for u, _ in net.in_links[node]:
                candidates.add(part.module[u])
            candidates.discard(part.module[node])
            best, best_delta = part.module[node], -1e-12
            for cand in candidates:
                d = delta(node, cand)
                if d < best_delta:
                    best, best_delta = cand, d
            if best != part.module[node]:
                part.move(node, best)
                moved += 1
        if moved == 0:
            break
        moved_any = True
    return part, moved_any


def louvain(net, objective, seed=1):
    """Local moving + aggregation until no further improvement.
    Returns the induced leaf-level partition."""
    rng = random.Random(seed)
    leaf_assignment = list(range(net.n))
    current = net
    while True:
        part, moved = local_moving(current, objective, rng)
        modules = sorted(set(part.module))
        if len(modules) == current.n or not moved:
            break
        agg, remap = current.aggregate(part.module)
        leaf_assignment = [remap[part.module[m]] for m in leaf_assignment]
        current = agg
    # Normalize module ids
    ids = {m: i for i, m in enumerate(sorted(set(leaf_assignment)))}
    return [ids[m] for m in leaf_assignment]


def best_of(net, objective, seeds):
    best_part, best_L = None, float("inf")
    for seed in seeds:
        assignment = louvain(net, objective, seed)
        part = Partition(net, assignment)
        L = part.L_atlas() if objective == "atlas" else part.L_map()
        if L < best_L:
            best_part, best_L = part, L
    return best_part


# ---------------------------------------------------------------------------
# Partition similarity (normalized mutual information)
# ---------------------------------------------------------------------------


def nmi(a, b):
    n = len(a)
    ca, cb, joint = defaultdict(int), defaultdict(int), defaultdict(int)
    for x, y in zip(a, b, strict=True):
        ca[x] += 1
        cb[y] += 1
        joint[(x, y)] += 1
    hx = entropy(c / n for c in ca.values())
    hy = entropy(c / n for c in cb.values())
    mi = sum(
        (c / n) * math.log2((c / n) / ((ca[x] / n) * (cb[y] / n)))
        for (x, y), c in joint.items()
    )
    if hx == 0.0 and hy == 0.0:
        return 1.0
    denom = math.sqrt(hx * hy)
    return mi / denom if denom > 0 else 0.0


# ---------------------------------------------------------------------------
# Test networks
# ---------------------------------------------------------------------------

KARATE_EDGES = [
    (0, 1),
    (0, 2),
    (0, 3),
    (0, 4),
    (0, 5),
    (0, 6),
    (0, 7),
    (0, 8),
    (0, 10),
    (0, 11),
    (0, 12),
    (0, 13),
    (0, 17),
    (0, 19),
    (0, 21),
    (0, 31),
    (1, 2),
    (1, 3),
    (1, 7),
    (1, 13),
    (1, 17),
    (1, 19),
    (1, 21),
    (1, 30),
    (2, 3),
    (2, 7),
    (2, 8),
    (2, 9),
    (2, 13),
    (2, 27),
    (2, 28),
    (2, 32),
    (3, 7),
    (3, 12),
    (3, 13),
    (4, 6),
    (4, 10),
    (5, 6),
    (5, 10),
    (5, 16),
    (6, 16),
    (8, 30),
    (8, 32),
    (8, 33),
    (9, 33),
    (13, 33),
    (14, 32),
    (14, 33),
    (15, 32),
    (15, 33),
    (18, 32),
    (18, 33),
    (19, 33),
    (20, 32),
    (20, 33),
    (22, 32),
    (22, 33),
    (23, 25),
    (23, 27),
    (23, 29),
    (23, 32),
    (23, 33),
    (24, 25),
    (24, 27),
    (24, 31),
    (25, 31),
    (26, 29),
    (26, 33),
    (27, 33),
    (28, 31),
    (28, 33),
    (29, 32),
    (29, 33),
    (30, 32),
    (30, 33),
    (31, 32),
    (31, 33),
    (32, 33),
]


def karate():
    return FlowNetwork.from_undirected(34, [(u, v, 1.0) for u, v in KARATE_EDGES])


def ring_of_cliques(num_cliques, clique_size=5, ring_weight=1.0):
    edges = []
    for c in range(num_cliques):
        base = c * clique_size
        for i in range(clique_size):
            for j in range(i + 1, clique_size):
                edges.append((base + i, base + j, 1.0))
        nxt = ((c + 1) % num_cliques) * clique_size
        edges.append((base, nxt + 1 if num_cliques > 1 else nxt, ring_weight))
    n = num_cliques * clique_size
    return FlowNetwork.from_undirected(n, edges), n


def ring_partition_codelengths(num_cliques, clique_size=5):
    """Codelength of 'b consecutive cliques per module' for each divisor b."""
    net, n = ring_of_cliques(num_cliques, clique_size)
    rows = []
    for b in [d for d in range(1, num_cliques + 1) if num_cliques % d == 0]:
        assignment = [(node // clique_size) // b for node in range(n)]
        part = Partition(net, assignment)
        rows.append((b, part.L_map(), part.L_atlas()))
    return rows


def planted_partition(n_blocks, block_size, k_in, k_out, rng, directed=False):
    """Sparse planted-partition graph via expected-degree sampling."""
    n = n_blocks * block_size
    p_in = k_in / (block_size - 1)
    p_out = k_out / (n - block_size)
    edges = set()
    for u in range(n):
        bu = u // block_size
        # within-block
        for v in range(bu * block_size, (bu + 1) * block_size):
            if v <= u:
                continue
            if rng.random() < p_in:
                edges.add((u, v))
        # between-block: sample number of partners
        expected = p_out * (n - block_size)
        count = int(expected) + (1 if rng.random() < expected - int(expected) else 0)
        for _ in range(count):
            v = rng.randrange(n)
            if v // block_size != bu:
                edges.add((min(u, v), max(u, v)))
    edge_list = [(u, v, 1.0) for u, v in edges]
    if directed:
        directed_edges = []
        for u, v, w in edge_list:
            if rng.random() < 0.5:
                directed_edges.append((u, v, w))
            else:
                directed_edges.append((v, u, w))
        net = FlowNetwork.from_directed(n, directed_edges)
    else:
        net = FlowNetwork.from_undirected(n, edge_list)
    truth = [u // block_size for u in range(n)]
    return net, truth


def parse_net_file(path):
    """Minimal Pajek/link-list parser for the repository's undirected
    examples: `*Vertices` may lack a count, and a bare link list (no section
    headers at all) is valid input."""
    edges = []
    mode = "edges"  # a headerless file is a plain link list
    max_id = 0
    with open(path) as fh:
        for line in fh:
            line = line.split("#")[0].strip()
            if not line:
                continue
            low = line.lower()
            if low.startswith("*vertices"):
                mode = "vertices"
                # honor a declared count so isolated vertices are kept even
                # when the explicit vertex list is omitted
                parts = line.split()
                if len(parts) > 1 and parts[1].isdigit():
                    max_id = max(max_id, int(parts[1]))
                continue
            if low.startswith("*edges") or low.startswith("*links"):
                mode = "edges"
                continue
            if low.startswith("*"):
                mode = None
                continue
            if mode == "vertices":
                max_id = max(max_id, int(line.split()[0]))
            elif mode == "edges":
                parts = line.split()
                u, v = int(parts[0]) - 1, int(parts[1]) - 1
                w = float(parts[2]) if len(parts) > 2 else 1.0
                max_id = max(max_id, u + 1, v + 1)
                edges.append((u, v, w))
    return FlowNetwork.from_undirected(max_id, edges)


# ---------------------------------------------------------------------------
# Validation experiments
# ---------------------------------------------------------------------------


def check(label, ok, detail=""):
    status = "PASS" if ok else "FAIL"
    print(f"  [{status}] {label}{'  (' + detail + ')' if detail else ''}")
    return ok


def experiment_deltas_and_identity(quick):
    print("\n== 1. Move deltas are exact; L_atlas = L_map + q*KL identity ==")
    rng = random.Random(42)
    all_ok = True
    trials = 3 if quick else 8
    for trial in range(trials):
        n = rng.randint(20, 60)
        m = rng.randint(int(1.5 * n), 3 * n)
        edges = set()
        while len(edges) < m:
            u, v = rng.randrange(n), rng.randrange(n)
            if u != v:
                edges.add((min(u, v), max(u, v)))
        if trial % 2 == 0:
            net = FlowNetwork.from_undirected(
                n, [(u, v, rng.uniform(0.5, 3.0)) for u, v in edges]
            )
        else:
            net = FlowNetwork.from_directed(
                n, [(u, v, rng.uniform(0.5, 3.0)) for u, v in edges]
            )
        assignment = [rng.randrange(max(2, n // 6)) for _ in range(n)]
        # densify ids
        ids = {mm: i for i, mm in enumerate(sorted(set(assignment)))}
        part = Partition(net, [ids[x] for x in assignment])

        gap = part.L_atlas() - part.L_map() - part.kl_gap()
        all_ok &= check(
            f"identity, trial {trial} ({'undirected' if trial % 2 == 0 else 'directed'})",
            abs(gap) < 1e-10,
            f"residual {gap:.2e}",
        )

        max_err = 0.0
        for _ in range(40):
            node = rng.randrange(n)
            new = rng.randrange(len(part.P))
            for _name, delta_fn, L_fn in (
                ("atlas", part.delta_atlas, Partition.L_atlas),
                ("map", part.delta_map, Partition.L_map),
            ):
                d = delta_fn(node, new)
                before = L_fn(part)
                saved = part.module[node]
                part.move(node, new)
                after = L_fn(part)
                part.move(node, saved)
                max_err = max(max_err, abs((after - before) - d))
        all_ok &= check(
            f"move deltas exact, trial {trial}",
            max_err < 1e-10,
            f"max err {max_err:.2e}",
        )
    return all_ok


def experiment_karate():
    print("\n== 2. Karate club: search under each objective ==")
    net = karate()
    seeds = range(1, 21)
    p_map = best_of(net, "map", seeds)
    p_atlas = best_of(net, "atlas", seeds)
    print(
        f"  map-equation optimum : {max(p_map.module) + 1} modules, "
        f"L_map={p_map.L_map():.4f}, L_atlas={p_map.L_atlas():.4f}"
    )
    print(
        f"  atlas optimum        : {max(p_atlas.module) + 1} modules, "
        f"L_map={p_atlas.L_map():.4f}, L_atlas={p_atlas.L_atlas():.4f}"
    )
    print(f"  NMI(map, atlas) = {nmi(p_map.module, p_atlas.module):.4f}")
    print(f"  one-level reference L = {p_map.node_entropy:.4f} bits")


def experiment_ring(quick):
    print("\n== 3. Ring of 5-cliques: optimal block size (resolution behavior) ==")
    print("  r (cliques) | argmin_b L_map | argmin_b L_atlas   (b = cliques/module)")
    sizes = [4, 8, 16, 32] if quick else [4, 8, 16, 32, 64, 128, 256]
    for r in sizes:
        rows = ring_partition_codelengths(r)
        best_map = min(rows, key=lambda t: t[1])[0]
        best_atlas = min(rows, key=lambda t: t[2])[0]
        print(f"  {r:>11} | {best_map:>14} | {best_atlas:>16}")


def experiment_planted(quick):
    print("\n== 4. Planted partition (10 blocks x 100): recovery vs mixing ==")
    print("  k_out/k_in | NMI map | NMI atlas | modules map/atlas | NMI(map,atlas)")
    rng = random.Random(7)
    mixes = (
        [(8.0, 1.0), (8.0, 2.0), (8.0, 3.0)]
        if quick
        else [
            (8.0, 0.5),
            (8.0, 1.0),
            (8.0, 2.0),
            (8.0, 3.0),
            (8.0, 4.0),
        ]
    )
    for k_in, k_out in mixes:
        net, truth = planted_partition(10, 100, k_in, k_out, rng)
        seeds = range(1, 4)
        p_map = best_of(net, "map", seeds)
        p_atlas = best_of(net, "atlas", seeds)
        print(
            f"  {k_out / k_in:>10.3f} | {nmi(truth, p_map.module):>7.3f} |"
            f" {nmi(truth, p_atlas.module):>9.3f} |"
            f" {max(p_map.module) + 1:>5} / {max(p_atlas.module) + 1:<5} |"
            f" {nmi(p_map.module, p_atlas.module):>8.3f}"
        )


def experiment_directed(quick):
    print("\n== 5. Directed planted partition: recovery vs mixing ==")
    print("  k_out/k_in | NMI map | NMI atlas | q*KL gap at atlas optimum")
    rng = random.Random(11)
    mixes = [(8.0, 1.0), (8.0, 2.0)] if quick else [(8.0, 1.0), (8.0, 2.0), (8.0, 3.0)]
    for k_in, k_out in mixes:
        net, truth = planted_partition(10, 100, k_in, k_out, rng, directed=True)
        seeds = range(1, 4)
        p_map = best_of(net, "map", seeds)
        p_atlas = best_of(net, "atlas", seeds)
        print(
            f"  {k_out / k_in:>10.3f} | {nmi(truth, p_map.module):>7.3f} |"
            f" {nmi(truth, p_atlas.module):>9.3f} | {p_atlas.kl_gap():.5f} bits"
        )


def experiment_examples(binary, repo_root):
    print("\n== 6. Repository examples: agreement with the Infomap binary ==")
    if not binary or not os.path.exists(binary):
        print("  (Infomap binary not found; skipping)")
        return
    for name in ("twotriangles.net", "ninetriangles.net"):
        path = os.path.join(repo_root, "examples", "networks", name)
        if not os.path.exists(path):
            print(f"  ({name} not found; skipping)")
            continue
        net = parse_net_file(path)
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run(
                [binary, path, tmp, "--two-level", "--silent", "--seed", "123"],
                check=True,
                capture_output=True,
            )
            tree_file = os.path.join(tmp, name.replace(".net", ".tree"))
            codelength_reported = None
            assignment = [0] * net.n
            with open(tree_file) as fh:
                for line in fh:
                    low = line.lower()
                    if (
                        line.startswith("#")
                        and low.startswith("# codelength")
                        and codelength_reported is None
                    ):
                        for token in line.replace(",", " ").split():
                            try:
                                codelength_reported = float(token)
                                break
                            except ValueError:
                                continue
                    if line.startswith("#") or not line.strip():
                        continue
                    parts = line.split()
                    module = int(parts[0].split(":")[0]) - 1
                    node_id = int(parts[-1]) - 1
                    assignment[node_id] = module
            part = Partition(net, assignment)
            ours = part.L_map()
            print(
                f"  {name:<18} Infomap L = {codelength_reported} bits, "
                f"prototype L_map on same partition = {ours:.9f} bits"
            )
            if codelength_reported is not None:
                # The .tree header rounds to 6 significant digits.
                ok = abs(ours - codelength_reported) < 1e-4
                check(
                    f"{name} codelength match",
                    ok,
                    f"|diff| = {abs(ours - codelength_reported):.2e}",
                )
            print(
                f"  {name:<18} atlas on same partition: L_atlas = {part.L_atlas():.9f} "
                f"(gap = {part.kl_gap():.9f} bits)"
            )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--infomap-binary", default=None)
    args = ap.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(here, "..", "..", ".."))
    binary = args.infomap_binary or os.path.join(repo_root, "Infomap")

    ok = experiment_deltas_and_identity(args.quick)
    experiment_karate()
    experiment_ring(args.quick)
    experiment_planted(args.quick)
    experiment_directed(args.quick)
    experiment_examples(binary, repo_root)
    print()
    if not ok:
        sys.exit(1)


if __name__ == "__main__":
    main()
