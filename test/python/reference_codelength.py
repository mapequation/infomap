"""Independent reference implementation of the map equation.

This module recomputes the codelength of a partition from first principles:
the leaf nodes with their tree paths and visit rates, and the links with their
flow. It deliberately does not import Infomap, so a formula that is wrong in
the C++ core is not repeated here. Tests feed it the flow and the tree that a
run reports and compare its codelength with the one the run reports.

Inputs are plain Python data (see :class:`Partition`). Everything else, every
enter and exit rate and every codebook, is derived here.

Sources for each term
---------------------
Map equation, two-level
    Rosvall & Bergstrom, "Maps of random walks on complex networks reveal
    community structure", PNAS 105, 1118 (2008), eq. 1 and 3::

        L(M) = q H(Q) + sum_i p_i H(P_i)

    q_i is the rate at which the walker enters module i, ``q = sum_i q_i``.
    ``p_i = q_i_exit + sum_{a in i} p_a`` is the use rate of module i's
    codebook, which has one codeword per node (used at the node's visit rate
    ``p_a``) and an exit codeword (used at ``q_i_exit``).

Multilevel map equation
    Rosvall & Bergstrom, "Multilevel compression of random walks on networks
    reveals hierarchical organization in large integrated systems",
    PLoS ONE 6, e18209 (2011), eq. 4-6. Every module owns a codebook with one
    codeword per child module (used at the child's enter rate), one per child
    leaf node (used at its visit rate) and an exit codeword (used at the
    module's exit rate). The root's codebook has no exit codeword. The
    codelength is the sum over all codebooks of ``use rate * entropy``. Here
    that is computed in the expanded form
    ``plogp(sum_k u_k) - sum_k plogp(u_k)`` with ``u_k`` the codeword rates.

Enter and exit rates
    A step along a link from ``a`` to ``b`` exits every module that contains
    ``a`` but not ``b`` and enters every module that contains ``b`` but not
    ``a``. With undirected flow the reported flow of a link is the flow in
    both directions together, so half goes each way. Self-links never cross a
    boundary.

Markov time
    Kheirkhahzadeh, Lancichinetti & Rosvall, "Efficient community detection
    of network flows for varying Markov times and bipartite networks",
    PRE 93, 032309 (2016). Encoding the walk every ``t`` steps scales the
    rate of the link steps, and so every enter and exit rate from links, by
    ``t``; the node visit rates stay. Only a global ``t`` is covered here.

Teleportation
    Lambiotte & Rosvall, "Ranking and clustering of nodes in networks with
    smart teleportation", PRE 85, 056107 (2012). With unrecorded teleportation
    (Infomap's default for directed flow) teleportation only shapes the flow
    and the link flows are what is encoded, so nothing extra is added. With
    recorded teleportation each node ``a`` teleports flow ``t_a`` (``alpha *
    p_a``, or ``p_a`` for a dangling node) to node ``b`` with probability
    ``w_b``. Module ``M`` then exits ``t_M (1 - w_M)`` and enters
    ``(t_total - t_M) w_M`` on top of its link flow (sums over the leaves of
    ``M``). Teleporting from a module into itself is not coded.

Memory and state networks
    Rosvall, Esquivel, Lancichinetti, West & Lambiotte, "Memory in network
    flows and its effects on spreading dynamics and community detection",
    Nat. Commun. 5, 4630 (2014), and Edler, Bohlin & Rosvall, "Mapping
    higher-order network flows in memory and multilayer networks with
    Infomap", Algorithms 10, 112 (2017), eq. 5. The leaves are state nodes and
    the links are state links, but a module's codebook has one codeword per
    *physical* node, used at the summed visit rate of that physical node's
    state nodes in the module.

Metadata (``MetaMapEquation``)
    Emmons & Mucha, "Map equation with metadata: Varying the role of
    attributes in community detection", PRE 100, 022301 (2019)::

        L = L_map + eta * sum_i p_i H(X_i)

    over the leaf modules ``i``, where ``X_i`` is the distribution of the
    metadata categories of the nodes in ``i``, weighted by node flow
    (``p_i`` is the module's total node flow), or by ``1/N`` per node when the
    metadata is unweighted.

Module-count preference (``BiasedMapEquation``)
    The ``--preferred-number-of-modules`` option documents a penalty "by how
    far their number of modules differs from this value". The size of the
    penalty is not in a paper; Infomap charges one bit per module of
    difference, ``|K - K_pref|``, with ``K`` the number of top modules. That
    is taken as the definition here.

Entropy bias correction (``BiasedMapEquation``)
    Miller, "Note on the bias of information estimates" (1955): the plug-in
    entropy of a ``K``-symbol distribution estimated from ``n`` samples is
    biased low by ``(K - 1) / (2 n)`` nats. Each codebook with ``K`` codewords
    is charged ``(K - 1) / (2 D ln 2)`` bits, where ``D`` is the total degree,
    the number of observed walk steps (``2 * sum(w) - sum(w_self)``, or the
    unweighted count if that is larger). The usage rate of a codebook cancels
    against the ``1/(u D)`` sample size, so every codebook costs the same per
    free parameter. A module has an exit codeword unless the walker can never
    leave it, which is when it and all its ancestors below the root are only
    children. Scaled by ``--entropy-correction-strength``. This follows the
    derivation in the comments of ``src/core/BiasedMapEquation.cpp``; there
    is no published formula for the per-codebook form.
"""

from __future__ import annotations

import math
from collections import defaultdict
from collections.abc import Hashable, Iterable, Mapping
from dataclasses import dataclass, field

import numpy as np

Path = tuple[int, ...]


def plogp(values) -> float:
    """Sum of ``p * log2(p)`` over ``values``, with ``0 log 0 = 0``."""
    p = np.asarray(values, dtype=float).ravel()
    p = p[p > 0]
    return float(np.sum(p * np.log2(p)))


def codebook_length(rates: Iterable[float]) -> float:
    """Use rate times entropy of a codebook with the given codeword rates.

    ``sum(u) * H(u / sum(u)) = plogp(sum(u)) - sum(plogp(u))``.
    """
    u = np.asarray(list(rates), dtype=float)
    return plogp([u.sum()]) - plogp(u)


@dataclass
class Leaf:
    """One leaf of the tree: a node, or a state node in a higher-order network."""

    path: Path
    flow: float
    physical_id: Hashable | None = None
    meta: Hashable | None = None
    teleport_flow: float = 0.0
    teleport_weight: float = 0.0


@dataclass
class Partition:
    """Everything the map equation needs, as plain data.

    ``leaves`` maps a leaf id to its :class:`Leaf`. ``links`` is a list of
    ``(source_id, target_id, flow)``. With ``undirected_links`` the flow of a
    link is the flow in both directions together.
    """

    leaves: dict[Hashable, Leaf]
    links: list[tuple[Hashable, Hashable, float]]
    undirected_links: bool = False
    markov_time: float = 1.0
    memory: bool = False
    recorded_teleportation: bool = False
    meta_rate: float | None = None
    meta_unweighted: bool = False
    preferred_num_modules: int = 0
    entropy_correction_strength: float | None = None
    total_degree: float = 0.0


@dataclass
class Codelength:
    total: float
    index: float
    module: float
    per_codebook: dict[Path, float] = field(default_factory=dict)
    enter: dict[Path, float] = field(default_factory=dict)
    exit: dict[Path, float] = field(default_factory=dict)
    flow: dict[Path, float] = field(default_factory=dict)


def total_degree(
    weights: Iterable[tuple[Hashable, Hashable, float]], *, undirected: bool = False
) -> float:
    """Number of observed walk steps: ``2 sum(w) - sum(w_self)``.

    Falls back to the unweighted link count when the weights average below 1,
    since the bias correction is about discrete counts. On undirected input a
    pair listed as ``a b`` and ``b a`` is one link.
    """
    weighted = 0.0
    links = set()
    for source, target, weight in weights:
        self_link = source == target
        weighted += weight if self_link else 2 * weight
        links.add(frozenset((source, target)) if undirected else (source, target))
    unweighted = sum(1 if len(set(link)) == 1 else 2 for link in links)
    return max(weighted, unweighted)


def modules_of(leaves: Mapping[Hashable, Leaf]) -> list[Path]:
    """Every module path, the root ``()`` included, parents before children."""
    modules = {()}
    for leaf in leaves.values():
        for k in range(1, len(leaf.path)):
            modules.add(leaf.path[:k])
    return sorted(modules, key=lambda p: (len(p), p))


def _children(leaves: Mapping[Hashable, Leaf], modules: list[Path]):
    child_modules: dict[Path, list[Path]] = defaultdict(list)
    child_leaves: dict[Path, list[Hashable]] = defaultdict(list)
    for module in modules:
        if module:
            child_modules[module[:-1]].append(module)
    for leaf_id, leaf in leaves.items():
        child_leaves[leaf.path[:-1]].append(leaf_id)
    return child_modules, child_leaves


def _enter_exit(partition: Partition, modules: list[Path]):
    enter = dict.fromkeys(modules, 0.0)
    exit_ = dict.fromkeys(modules, 0.0)
    leaves = partition.leaves

    def step(source, target, flow):
        a = leaves[source].path[:-1]
        b = leaves[target].path[:-1]
        common = 0
        while common < min(len(a), len(b)) and a[common] == b[common]:
            common += 1
        for k in range(common + 1, len(a) + 1):
            exit_[a[:k]] += flow
        for k in range(common + 1, len(b) + 1):
            enter[b[:k]] += flow

    for source, target, flow in partition.links:
        if source == target:
            continue
        flow *= partition.markov_time
        if partition.undirected_links:
            step(source, target, flow / 2)
            step(target, source, flow / 2)
        else:
            step(source, target, flow)

    if partition.recorded_teleportation:
        tele = dict.fromkeys(modules, 0.0)
        weight = dict.fromkeys(modules, 0.0)
        for leaf in leaves.values():
            for k in range(len(leaf.path)):
                tele[leaf.path[:k]] += leaf.teleport_flow
                weight[leaf.path[:k]] += leaf.teleport_weight
        tele_total = tele[()]
        for module in modules:
            if module:
                exit_[module] += tele[module] * (1 - weight[module])
                enter[module] += (tele_total - tele[module]) * weight[module]
    return enter, exit_


def _has_exit_codeword(module: Path, child_count: Mapping[Path, int]) -> bool:
    node = module
    while node:
        if child_count[node[:-1]] > 1:
            return True
        node = node[:-1]
    return False


def codelength(partition: Partition) -> Codelength:
    """Codelength of ``partition`` in bits, from the formulas in the module docstring."""
    leaves = partition.leaves
    modules = modules_of(leaves)
    child_modules, child_leaves = _children(leaves, modules)
    enter, exit_ = _enter_exit(partition, modules)

    flow = dict.fromkeys(modules, 0.0)
    for leaf in leaves.values():
        for k in range(len(leaf.path)):
            flow[leaf.path[:k]] += leaf.flow

    child_count = {m: len(child_modules[m]) + len(child_leaves[m]) for m in modules}
    bits_per_parameter = 0.0
    if partition.entropy_correction_strength is not None:
        bits_per_parameter = partition.entropy_correction_strength / (
            2 * partition.total_degree * math.log(2)
        )
    n_nodes = len(leaves)

    per_codebook: dict[Path, float] = {}
    for module in modules:
        rates = [enter[c] for c in child_modules[module]]
        if partition.memory:
            physical: dict[Hashable, float] = defaultdict(float)
            for leaf_id in child_leaves[module]:
                physical[leaves[leaf_id].physical_id] += leaves[leaf_id].flow
            rates.extend(physical.values())
        else:
            rates.extend(leaves[leaf_id].flow for leaf_id in child_leaves[module])
        has_exit = bool(module) and _has_exit_codeword(module, child_count)
        if module:
            rates.append(exit_[module])
        length = codebook_length(rates)

        if partition.meta_rate is not None and child_leaves[module]:
            meta_flow: dict[Hashable, float] = defaultdict(float)
            for leaf_id in child_leaves[module]:
                leaf = leaves[leaf_id]
                meta_flow[leaf.meta] += (
                    1 / n_nodes if partition.meta_unweighted else leaf.flow
                )
            length += partition.meta_rate * codebook_length(meta_flow.values())

        if bits_per_parameter:
            num_codewords = child_count[module] + (1 if has_exit else 0)
            length += bits_per_parameter * max(num_codewords - 1, 0)

        per_codebook[module] = length

    index = per_codebook[()]
    module_length = sum(v for k, v in per_codebook.items() if k)
    if partition.preferred_num_modules > 0:
        module_length += abs(child_count[()] - partition.preferred_num_modules)
    return Codelength(
        total=index + module_length,
        index=index,
        module=module_length,
        per_codebook=per_codebook,
        enter=enter,
        exit=exit_,
        flow=flow,
    )


def one_module(partition: Partition) -> Partition:
    """The same network with every leaf in a single top module."""
    leaves = {
        leaf_id: Leaf(
            path=(1, i + 1),
            flow=leaf.flow,
            physical_id=leaf.physical_id,
            meta=leaf.meta,
            teleport_flow=leaf.teleport_flow,
            teleport_weight=leaf.teleport_weight,
        )
        for i, (leaf_id, leaf) in enumerate(partition.leaves.items())
    }
    return Partition(**{**partition.__dict__, "leaves": leaves})


def tree_depth(partition: Partition) -> int:
    """Number of levels below the root, i.e. the length of the longest leaf path."""
    return max(len(leaf.path) for leaf in partition.leaves.values())
