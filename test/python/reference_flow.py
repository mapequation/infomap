"""Independent reference implementation of Infomap's flow models.

This module computes the node and link flow of a network from first
principles, with dense numpy linear algebra. Like ``reference_codelength`` it
does not import Infomap, so a flow that is wrong in the C++ core is not
repeated here. The directed stationary distribution is solved exactly as a
linear system rather than by power iteration, and the undirected one is taken
from its closed form.

Inputs are plain Python data: the node ids (isolated nodes included), the links
as ``(source, target, weight)``, and for teleportation to nodes the node
weights. Links with the same ``(source, target)`` are summed. The result is a
:class:`Flow` keyed by node id and by ``(source, target)``.

Notation: ``w_ab`` is the weight of the link from ``a`` to ``b``,
``out_a = sum_b w_ab`` and ``in_b = sum_a w_ab``, ``W = sum_ab w_ab``. A node
is dangling when ``out_a = 0``; a self-link counts as an out-link. ``alpha`` is
the teleportation probability and ``beta = 1 - alpha``.

Sources for each model
----------------------
Undirected (``--flow-model undirected``, the default)
    Rosvall & Bergstrom, "Maps of random walks on complex networks reveal
    community structure", PNAS 105, 1118 (2008): on an undirected network the
    walker's visit rate is the node's share of the total weighted degree,
    ``p_a = s_a / sum_c s_c``. A link ``a - b`` adds ``w_ab`` to the strength of
    both ends; a self-link adds its weight once (it is one link end that leads
    back, the convention Infomap documents as "treat self-links as directed").
    The reported flow of a link is the flow in both directions together,
    ``p_a w_ab / s_a + p_b w_ab / s_b``, so ``2 w_ab / sum_c s_c``, and
    ``w_aa / sum_c s_c`` for a self-link. Two input links ``a b`` and ``b a``
    stay separate links, each with its own weight.

Directed (``--flow-model directed``, ``-d``)
    Brin & Page (1998) PageRank with the teleportation variants of Lambiotte
    & Rosvall, "Ranking and clustering of nodes in networks with smart
    teleportation", PRE 85, 056107 (2012). The documented behaviour in
    ``interfaces/python/source/concepts/flow-and-random-walks.md`` is the spec
    for where the walker lands:

    * the walker follows an out-link of ``a`` chosen by weight with
      probability ``beta``, and teleports with probability ``alpha``. A
      dangling node has no link to follow and teleports with probability 1;
    * a teleport lands on node ``b`` with probability ``v_b``. With
      ``to_nodes`` ``v`` is the node weights, normalised (1 per node when the
      input gives none; ``--to-nodes`` "uses uniform node weights unless node
      weights are provided"). Teleporting to links picks a link by weight and
      lands at its source with unrecorded teleportation, so
      ``v_b = out_b / W``, and at its target with recorded teleportation, so
      ``v_b = in_b / W``.

    ``p`` is the stationary distribution of that walk, solved from
    ``p M = p`` with ``sum(p) = 1``, where
    ``M_ab = beta w_ab / out_a + alpha v_b`` for a non-dangling ``a`` and
    ``M_ab = v_b`` for a dangling one. The rate of steps along the link
    ``a -> b`` is ``r_ab = beta p_a w_ab / out_a``.

    Recorded teleportation (``-e``): every step is coded. Node flow is ``p``
    and link flow is ``r``, so link flow sums to ``beta (1 - dangling flow)``.
    Node ``a`` teleports ``alpha p_a``, or ``p_a`` when it is dangling, and a
    teleport lands on ``b`` with weight ``v_b``.

    Unrecorded teleportation (the default): teleportation only shapes the
    flow, and the coded walk is the sequence of link steps. Link flow is ``r``
    normalised to sum 1, and the flow of a node is the flow of the links into
    it, so a node with no in-links has flow 0 (the "re-weights it to count
    only link-traversal steps" of the docs).

``undirdir``, ``outdirdir``, ``rawdir``
    These "two-mode" models have no paper. The spec is the option list
    ("Choose how Infomap derives flow from the input links") and the flow
    method Infomap reports for each: "undirected links, directed steady
    state", "ingoing links only" and "directed links with raw flow". Read as:

    * ``undirdir``: start from the undirected visit rates ``p0`` above and take
      one step along the directed links only, ``f_ab = p0_a w_ab / s_a`` with
      ``s_a`` the undirected strength, normalised to sum 1;
    * ``outdirdir``: node flow counts the ingoing links only,
      ``p_b = in_b / W``, and each link carries its share of the weight,
      ``f_ab = w_ab / W``;
    * ``rawdir``: the link weights are the flow, ``f_ab = w_ab / W``.

    In each, the flow of a node is the flow of its in-links. Since
    ``p0_a / s_a`` is the same for every node, all three come out as
    ``f_ab = w_ab / W`` and ``p_b = in_b / W``; they differ in the codelength
    only through ``undirdir`` counting as undirected flow elsewhere.

Markov time
    Kheirkhahzadeh, Lancichinetti & Rosvall, PRE 93, 032309 (2016), and the
    ``--markov-time`` text "Scale link flow to change the cost of moving
    between modules". It scales the link flow the map equation sees when it
    counts module enter and exit flow, not the flow of the network: node and
    link flow are those of Markov time 1. A leaf's enter and exit flow is
    ``t`` times its flow along links to and from other nodes (half of each
    undirected link each way), plus the recorded teleportation terms
    ``t_a (1 - v_a)`` out and ``(T - t_a) v_a`` in, which are not scaled
    (the same convention as ``reference_codelength``).

Bipartite networks, undirected
    Kheirkhahzadeh, Lancichinetti & Rosvall, PRE 93, 032309 (2016), sec. on
    bipartite networks, and the comment in Infomap's flow code that "Markov
    time 2 on the full network will correspond to markov time 1 between the
    real nodes". The walker alternates between primary and feature nodes and
    only the primary nodes are coded. Their flow is the full walk's visit
    rate restricted to the primary side and renormalised, so it stays
    proportional to strength, and features get flow 0. Two steps of the full
    walk are one step between primary nodes, so link flow doubles. The
    directed projection is an open modelling question (#946) and is not
    covered; nor is ``--bipartite-teleportation``. With
    ``--skip-adjust-bipartite-flow`` the declaration must not change the
    flow at all, which is checked against the plain models (#945).

Multilayer flow is not covered.
"""

from __future__ import annotations

from collections.abc import Hashable, Iterable, Mapping
from dataclasses import dataclass

import numpy as np

Link = tuple[Hashable, Hashable]

MODELS = ("undirected", "directed", "undirdir", "outdirdir", "rawdir")


@dataclass
class Flow:
    """Node and link flow. Teleportation is filled in for the directed model."""

    node: dict[Hashable, float]
    link: dict[Link, float]
    teleport_flow: dict[Hashable, float] | None = None
    teleport_weight: dict[Hashable, float] | None = None


class _Network:
    def __init__(self, nodes, links):
        weights: dict[Link, float] = {}
        ids: dict[Hashable, int] = {}
        for node in nodes:
            ids.setdefault(node, len(ids))
        for source, target, weight in links:
            ids.setdefault(source, len(ids))
            ids.setdefault(target, len(ids))
            weights[(source, target)] = weights.get((source, target), 0.0) + weight
        self.ids = ids
        self.nodes = list(ids)
        self.weights = weights
        n = len(ids)
        self.w = np.zeros((n, n))
        for (source, target), weight in weights.items():
            self.w[ids[source], ids[target]] += weight

    def node_dict(self, values) -> dict[Hashable, float]:
        return {node: float(values[i]) for node, i in self.ids.items()}

    def in_flow(self, link_flow: Mapping[Link, float]) -> np.ndarray:
        flow = np.zeros(len(self.ids))
        for (_, target), value in link_flow.items():
            flow[self.ids[target]] += value
        return flow

    def undirected_strength(self) -> np.ndarray:
        return self.w.sum(axis=1) + self.w.sum(axis=0) - np.diag(self.w)


def stationary(transition: np.ndarray) -> np.ndarray:
    """The distribution ``p`` with ``p M = p`` and ``sum(p) = 1``, solved directly."""
    n = transition.shape[0]
    system = transition.T - np.eye(n)
    system[-1, :] = 1.0
    rhs = np.zeros(n)
    rhs[-1] = 1.0
    return np.linalg.solve(system, rhs)


def _undirected(net: _Network) -> Flow:
    strength = net.undirected_strength()
    p = strength / strength.sum()
    link = {}
    for (source, target), weight in net.weights.items():
        a, b = net.ids[source], net.ids[target]
        value = p[a] * weight / strength[a]
        if a != b:
            value += p[b] * weight / strength[b]
        link[(source, target)] = value
    return Flow(node=net.node_dict(p), link=link)


def teleport_targets(
    net: _Network, *, recorded: bool, to_nodes: bool, node_weights=None
) -> np.ndarray:
    if to_nodes:
        node_weights = node_weights or {}
        v = np.array([float(node_weights.get(node, 1.0)) for node in net.nodes])
    elif recorded:
        v = net.w.sum(axis=0)
    else:
        v = net.w.sum(axis=1)
    return v / v.sum()


def _directed(
    net: _Network, *, alpha: float, recorded: bool, to_nodes: bool, node_weights
) -> Flow:
    beta = 1 - alpha
    out = net.w.sum(axis=1)
    dangling = out == 0
    step = np.divide(
        net.w, out[:, None], out=np.zeros_like(net.w), where=~dangling[:, None]
    )
    v = teleport_targets(
        net, recorded=recorded, to_nodes=to_nodes, node_weights=node_weights
    )

    transition = beta * step + alpha * v[None, :]
    transition[dangling] = v
    p = stationary(transition)

    # p_a w_ab / out_a; times beta this is the link-step rate r_ab. Unrecorded
    # flow is normalised, so the common beta is left out there, which keeps
    # alpha = 1 (no link steps at all) at its limit instead of 0 / 0.
    link = {
        (source, target): p[net.ids[source]] * step[net.ids[source], net.ids[target]]
        for source, target in net.weights
    }
    if recorded:
        teleport = np.where(dangling, p, alpha * p)
        return Flow(
            node=net.node_dict(p),
            link={key: beta * value for key, value in link.items()},
            teleport_flow=net.node_dict(teleport),
            teleport_weight=net.node_dict(v),
        )
    total = sum(link.values())
    link = {key: value / total for key, value in link.items()}
    return Flow(
        node=net.node_dict(net.in_flow(link)),
        link=link,
        teleport_weight=net.node_dict(v),
    )


def _one_directed_step(net: _Network, start: np.ndarray, out: np.ndarray) -> Flow:
    link = {}
    for (source, target), weight in net.weights.items():
        a = net.ids[source]
        link[(source, target)] = start[a] * weight / out[a] if out[a] > 0 else 0.0
    total = sum(link.values())
    link = {key: value / total for key, value in link.items()}
    return Flow(node=net.node_dict(net.in_flow(link)), link=link)


def flow(
    nodes: Iterable[Hashable],
    links: Iterable[tuple[Hashable, Hashable, float]],
    model: str = "undirected",
    *,
    teleportation_probability: float = 0.15,
    recorded: bool = False,
    to_nodes: bool = False,
    node_weights: Mapping[Hashable, float] | None = None,
) -> Flow:
    """Node and link flow of the network under ``model``, from the formulas above."""
    net = _Network(nodes, links)
    if model == "undirected":
        return _undirected(net)
    if model == "directed":
        return _directed(
            net,
            alpha=teleportation_probability,
            recorded=recorded,
            to_nodes=to_nodes,
            node_weights=node_weights,
        )
    if model == "undirdir":
        strength = net.undirected_strength()
        return _one_directed_step(net, strength / strength.sum(), strength)
    if model == "outdirdir":
        in_weight = net.w.sum(axis=0)
        link = {key: weight / in_weight.sum() for key, weight in net.weights.items()}
        return Flow(node=net.node_dict(in_weight / in_weight.sum()), link=link)
    if model == "rawdir":
        total = sum(net.weights.values())
        link = {key: weight / total for key, weight in net.weights.items()}
        return Flow(node=net.node_dict(net.in_flow(link)), link=link)
    raise ValueError(f"unknown flow model {model!r}, expected one of {MODELS}")


def bipartite_projection(network_flow: Flow, features: Iterable[Hashable]) -> Flow:
    """Undirected bipartite flow from the full network's flow, from the rule above."""
    features = set(features)
    primary = sum(v for node, v in network_flow.node.items() if node not in features)
    node = {
        node: 0.0 if node in features else value / primary
        for node, value in network_flow.node.items()
    }
    link = {key: 2 * value for key, value in network_flow.link.items()}
    return Flow(node=node, link=link)


def leaf_enter_exit(
    result_flow: Flow, *, undirected: bool, markov_time: float = 1.0
) -> tuple[dict[Hashable, float], dict[Hashable, float]]:
    """Enter and exit flow of each node as its own module, from the Markov time rule above."""
    enter = dict.fromkeys(result_flow.node, 0.0)
    exit_ = dict.fromkeys(result_flow.node, 0.0)
    for (source, target), value in result_flow.link.items():
        if source == target:
            continue
        value *= markov_time
        if undirected:
            for a in (source, target):
                enter[a] += value / 2
                exit_[a] += value / 2
        else:
            exit_[source] += value
            enter[target] += value
    if result_flow.teleport_flow is not None:
        total = sum(result_flow.teleport_flow.values())
        for node, tele in result_flow.teleport_flow.items():
            weight = result_flow.teleport_weight[node]
            exit_[node] += tele * (1 - weight)
            enter[node] += (total - tele) * weight
    return enter, exit_
