#!/usr/bin/env python3
"""Offline raggedization probe for columnar equal-depth trees.

Parses an .ftree (leaf lines + *Links module lines with enter/exit flow),
reconstructs the hierarchical map equation, then greedily applies two local
prune operations wherever they lower L:

  dissolve(m): splice module m out — its children (modules or leaves) become
               children of m's parent. Only the parent's codebook term and the
               removal of m's term change.
  flatten(m):  remove ALL internal structure below m — every leaf in m's
               subtree attaches directly to m. Catches multi-level gains that
               single dissolves miss.

Both only remove structure, so the result is a ragged coarsening of the input
tree with identical leaf partitions at surviving levels.

Usage: prune_ftree.py <file.ftree> <out.tree> [--report-only]
"""

import math
import sys
from collections import defaultdict

LOG2 = math.log2


def plogp(x):
    return x * LOG2(x) if x > 0 else 0.0


class Node:
    __slots__ = ("children", "enter", "exit", "leaves", "parent", "path")

    def __init__(self, path):
        self.path = path
        self.enter = 0.0
        self.exit = 0.0
        self.children = []  # list of Node
        self.leaves = []  # list of (flow, name, nid) directly attached
        self.parent = None


def parse_ftree(fname):
    nodes = {}  # path tuple -> Node
    root = Node(())
    nodes[()] = root
    leaf_lines = []
    links = {}
    with open(fname) as f:
        for line in f:
            if line.startswith("#"):
                continue
            if line.startswith("*Links"):
                parts = line.split()
                if parts[1] in ("directed", "undirected"):
                    continue
                path = (
                    ()
                    if parts[1] == "root"
                    else tuple(int(x) for x in parts[1].split(":"))
                )
                links[path] = (float(parts[2]), float(parts[3]))
                continue
            parts = line.split(None, 2)
            if ":" not in parts[0]:
                continue
            fields = parts[0].split(":")
            modpath = tuple(int(x) for x in fields[:-1])
            flow = float(parts[1])
            rest = parts[2].rstrip("\n") if len(parts) > 2 else ""
            leaf_lines.append((modpath, flow, rest))
    # build module nodes from leaf paths (all prefixes)
    for modpath, flow, rest in leaf_lines:
        for d in range(1, len(modpath) + 1):
            p = modpath[:d]
            if p not in nodes:
                n = Node(p)
                nodes[p] = n
                par = nodes[p[:-1]]
                n.parent = par
                par.children.append(n)
        nodes[modpath].leaves.append((flow, rest))
    for path, (ent, ext) in links.items():
        if path in nodes:
            nodes[path].enter = ent
            nodes[path].exit = ext
    missing = [p for p in nodes if p != () and p not in links]
    if missing:
        raise SystemExit(
            f"modules without *Links flow data: {missing[:5]} (+{len(missing) - 5})"
        )
    return root, nodes


def term(node):
    """Map-equation codebook term of one internal node."""
    ext = 0.0 if node.parent is None else node.exit
    q = ext
    s = 0.0
    for c in node.children:
        q += c.enter
        s += plogp(c.enter)
    for fl, _ in node.leaves:
        q += fl
        s += plogp(fl)
    return plogp(q) - plogp(ext) - s


def total_codelength(root):
    L = 0.0
    stack = [root]
    while stack:
        n = stack.pop()
        L += term(n)
        stack.extend(n.children)
    return L


def subtree_terms_and_leaves(node):
    """Sum of codebook terms of node + all internal descendants, and all subtree leaves."""
    tsum = 0.0
    leaves = []
    stack = [node]
    while stack:
        n = stack.pop()
        tsum += term(n)
        leaves.extend(n.leaves)
        stack.extend(n.children)
    return tsum, leaves


def gain_dissolve(m):
    p = m.parent
    before = term(p) + term(m)
    # after: p's children = (p.children - m) + m.children, p.leaves += m.leaves
    ext = 0.0 if p.parent is None else p.exit
    q = ext
    s = 0.0
    for c in p.children:
        if c is m:
            continue
        q += c.enter
        s += plogp(c.enter)
    for c in m.children:
        q += c.enter
        s += plogp(c.enter)
    for fl, _ in p.leaves:
        q += fl
        s += plogp(fl)
    for fl, _ in m.leaves:
        q += fl
        s += plogp(fl)
    after = plogp(q) - plogp(ext) - s
    return before - after


def apply_dissolve(m):
    p = m.parent
    p.children.remove(m)
    for c in m.children:
        c.parent = p
        p.children.append(c)
    p.leaves.extend(m.leaves)
    m.children = []
    m.leaves = []


def gain_flatten(m):
    if not m.children:
        return 0.0
    before, leaves = subtree_terms_and_leaves(m)
    ext = 0.0 if m.parent is None else m.exit
    q = ext
    s = 0.0
    for (
        fl,
        _,
    ) in leaves:  # subtree_terms_and_leaves already includes m's own direct leaves
        q += fl
        s += plogp(fl)
    after = plogp(q) - plogp(ext) - s
    return before - after


def internal_descendants(m):
    out = []
    stack = list(m.children)
    while stack:
        n = stack.pop()
        out.append(n)
        stack.extend(n.children)
    return out


def apply_flatten(m):
    _, leaves = subtree_terms_and_leaves(m)
    for d in internal_descendants(m):  # detached: make them inert for any later visitor
        d.children = []
        d.leaves = []
        d.parent = None
    m.children = []
    m.leaves = leaves


def all_internal(root, include_root=False):
    out = []
    stack = [root]
    while stack:
        n = stack.pop()
        if (n.children or n is not root) and (n is not root or include_root):
            out.append(n)
        stack.extend(n.children)
    return out


def depth_of(n):
    d = 0
    while n.parent is not None:
        d += 1
        n = n.parent
    return d


def greedy_prune(root, eps=1e-12, homogeneous=False):
    """homogeneous=True keeps every module's children all-leaf or all-module
    (the only tree shape the engine accepts): dissolve is then restricted to
    modules whose children are modules, and a leaf module can only disappear
    via flatten() of its parent."""
    ops = []
    sweep = 0
    while True:
        sweep += 1
        applied = 0
        nodes = sorted(all_internal(root), key=depth_of, reverse=True)
        for n in nodes:
            if n.parent is None:
                continue
            if not n.children and not n.leaves:
                continue  # already dissolved
            can_dissolve = not homogeneous or (n.children and not n.leaves)
            gd = gain_dissolve(n) if can_dissolve else 0.0
            gf = gain_flatten(n) if n.children else 0.0
            if gd <= eps and gf <= eps:
                continue
            if gd >= gf:
                ops.append(("dissolve", depth_of(n), gd))
                apply_dissolve(n)
            else:
                ops.append(("flatten", depth_of(n), gf))
                apply_flatten(n)
            applied += 1
        if applied == 0:
            break
    return ops, sweep


def greedy_prune_best_first(root, eps=1e-12, homogeneous=False):
    """Same operation set as greedy_prune, but always applies the single largest
    remaining gain (heap with per-node version stamps), recomputing only the
    candidates an operation can change: the parent, the parent's children
    (their dissolve gain reads the parent's term) and every ancestor (flatten
    gains read the subtree). Order-independent, unlike the bottom-up sweep."""
    import heapq

    heap = []
    version = {}

    def ancestors(n):
        n = n.parent
        while n is not None and n.parent is not None:
            yield n
            n = n.parent

    def refresh(n):
        version[id(n)] = version.get(id(n), 0) + 1
        if n.parent is None or (not n.children and not n.leaves):
            return
        can_dissolve = not homogeneous or (n.children and not n.leaves)
        gd = gain_dissolve(n) if can_dissolve else 0.0
        gf = gain_flatten(n) if n.children else 0.0
        g, op = (gd, "dissolve") if gd >= gf else (gf, "flatten")
        if g > eps:
            heapq.heappush(heap, (-g, version[id(n)], id(n), op, n))

    for n in all_internal(root):
        refresh(n)
    ops = []
    while heap:
        negg, v, _, op, n = heapq.heappop(heap)
        if version.get(id(n)) != v:
            continue
        p = n.parent
        d = depth_of(n)
        if op == "dissolve":
            apply_dissolve(n)
            refresh(n)
            refresh(p)
            for c in p.children:
                refresh(c)
            for a in ancestors(p):
                refresh(a)
        else:
            removed = internal_descendants(n)
            apply_flatten(n)
            for dead in removed:
                version[id(dead)] = version.get(id(dead), 0) + 1  # kill their heap entries
            refresh(n)
            for a in ancestors(n):
                refresh(a)
        ops.append((op, d, -negg))
    return ops, 1


def write_tree(root, fname):
    lines = []

    def rec(node, prefix):
        idx = 1
        for c in node.children:
            rec(c, prefix + [idx])
            idx += 1
        for fl, rest in node.leaves:
            path = ":".join(str(x) for x in prefix + [idx])
            lines.append(f"{path} {fl:.9g} {rest}\n")
            idx += 1

    rec(root, [])
    with open(fname, "w") as f:
        f.writelines(lines)


def depth_histogram(root):
    hist = defaultdict(int)

    def rec(n, d):
        for _ in n.leaves:
            hist[d + 1] += 1
        for c in n.children:
            rec(c, d + 1)

    rec(root, 0)
    return dict(sorted(hist.items()))


def per_level_terms(root):
    lv = defaultdict(float)
    cnt = defaultdict(int)
    stack = [(root, 0)]
    while stack:
        n, d = stack.pop()
        lv[d] += term(n)
        cnt[d] += 1
        for c in n.children:
            stack.append((c, d + 1))
    return {d: (lv[d], cnt[d]) for d in sorted(lv)}


def main():
    homogeneous = "--homogeneous" in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    ftree = args[0]
    out = args[1] if len(args) > 1 else None
    root, _nodes = parse_ftree(ftree)
    L0 = total_codelength(root)
    nmod0 = len(all_internal(root))
    print(
        f"parsed: {nmod0} internal modules (excl root), leaf depth histogram {depth_histogram(root)}"
    )
    print(f"reconstructed L = {L0:.9f}")
    print("per-level codebook terms (depth: bits, #modules):")
    for d, (t, c) in per_level_terms(root).items():
        print(f"  depth {d}: {t:.6f} bits over {c} codebooks")
    best_first = "--best-first" in sys.argv
    if homogeneous:
        print("mode: homogeneous (engine-accepted shapes only)")
    if best_first:
        print("order: best-first (largest remaining gain each step)")
        ops, sweeps = greedy_prune_best_first(root, homogeneous=homogeneous)
    else:
        ops, sweeps = greedy_prune(root, homogeneous=homogeneous)
    gain = sum(g for _, _, g in ops)
    L1 = total_codelength(root)
    print(
        f"\ngreedy prune: {len(ops)} ops in {sweeps} sweeps, predicted gain {gain:.9f} bits"
    )
    print(f"L after prune = {L1:.9f}  ({(L1 - L0) / L0 * 100:+.4f}%)")
    bytype = defaultdict(lambda: [0, 0.0])
    for op, d, g in ops:
        k = (op, d)
        bytype[k][0] += 1
        bytype[k][1] += g
    for (op, d), (c, g) in sorted(bytype.items()):
        print(f"  {op} at depth {d}: {c} ops, {g:.6f} bits")
    print(f"leaf depth histogram after: {depth_histogram(root)}")
    if out:
        write_tree(root, out)
        print(f"wrote {out}")


if __name__ == "__main__":
    main()
