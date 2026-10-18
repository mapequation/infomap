"""Compare Infomap's two-level search with the exhaustive optimum on tiny networks.

A network with ``n`` nodes has Bell(n) two-level partitions, 21147 for n = 9,
so on tiny networks every one of them can be scored. Each is scored with
``reference_codelength`` over the flow from ``reference_flow``: neither the
score nor the flow goes through Infomap (#1171). Then, for each flow model and
objective:

* Infomap's two-level result is never below the optimum. Below it would mean
  that the search prices some partition too low.
* Infomap reports the reference's codelength for the partition it returns.
* ``--cluster-data <partition> --no-infomap`` reports the reference's
  codelength for a sample of partitions.

How often the search misses the optimum is search quality, not correctness.
Over 60 networks (n = 5 to 9, the four kinds of ``tiny_network``, three seeds),
the 5 settings in ``FLOWS`` and the 5 in ``OBJECTIVES``, with 5 trials per run,
on macOS:

* plain map equation: 3 of 300 runs missed, all with unrecorded directed flow,
  by at most 0.018 bits. In the n = 5 case the optimum puts a node with no
  in-links, and so no flow, in a module of its own; 22 of 100 seeds find it.
* Markov time 2 and the entropy correction: no misses.
* ``--preferred-number-of-modules``: 10 of 300 missed with 2 preferred
  modules and 74 of 300 with 3, by up to 0.69 bits. The search ends at the
  preferred number of modules, but not at the best partition with that
  number. A move that empties or opens a module changes ``|K - K_pref|`` by a
  whole bit, which a single node move rarely pays for.

``test_search_quality`` holds the objectives without the preference to that
level, so a systematic loss shows.

The default run covers n <= 7. The sweep up to n = 9 is marked slow
(``pytest --run-slow``).

Multilevel
----------
Counting every tree Infomap can return (``hierarchies``) gives 612 for n = 5
and 8374 for n = 6, so the multilevel search can be checked against the
optimum over all of them too. Scoring the 8374 trees takes about 0.6 s, and
n = 7, with 140408 trees, about 10 s per run, so the default run covers two
networks of five nodes and the slow sweep n = 5 and 6. For each run:

* the multilevel result is never below the optimum over every tree, and it
  reports the reference's codelength for the tree it returns;
* the returned tree codes no worse than its flattenings to top and to leaf
  modules, all three priced by the reference, and no worse than two-level
  search with the same seed (``flattening.py``, #1257).

Over the sweep (n = 5 with three seeds, n = 6 with two, the five flows), on
macOS, a tree deeper than two levels beat every two-level partition in two
settings only:

* 2 preferred modules, 2 of 100 runs: the search found both;
* Markov time 0.25, 11 of 100 runs: the search found none, by up to 0.048
  bits, and returned the best two-level partition each time.

These optima put nodes in modules of their own under a common module. The
search does not reach them for two reasons. It skips the hierarchical search
when the two-level partition has every node in a module of its own, and it
rejects a sub-partition that puts every node of a module in a module of its
own. A low Markov time favours such modules. The other misses are the
two-level misses above: the node without in-links, and the preferred number
of modules.
"""

from __future__ import annotations

import math
from pathlib import Path

import numpy as np
import pytest
from flattening import Flattenings, codes_no_worse, flatten
from infomap import run
from reference_codelength import Leaf, Partition, codelength, total_degree
from reference_flow import flow

pytestmark = pytest.mark.fast

REPO = Path(__file__).resolve().parents[2]
EXAMPLES = REPO / "examples" / "networks"

REL = 1e-10
ABS = 1e-12

FLOWS = {
    "undirected": {},
    "directed": {"directed": True},
    "directed-to-nodes": {"directed": True, "to_nodes": True},
    "recorded": {"directed": True, "recorded_teleportation": True},
    "recorded-to-nodes": {
        "directed": True,
        "recorded_teleportation": True,
        "to_nodes": True,
    },
}

# Engine options, and the matching reference objective.
OBJECTIVES = {
    "plain": ({}, {}),
    "markov-time": ({"markov_time": 2.0}, {"markov_time": 2.0}),
    "preferred-2": (
        {"preferred_number_of_modules": 2},
        {"preferred_num_modules": 2},
    ),
    "preferred-3": (
        {"preferred_number_of_modules": 3},
        {"preferred_num_modules": 3},
    ),
    "entropy": ({"entropy_corrected": True}, {"entropy_correction_strength": 1.0}),
}


def set_partitions(n: int):
    """Every partition of ``n`` items, as restricted growth strings.

    ``labels[i]`` is the block of item ``i``; item 0 is in block 0 and each
    later item joins an earlier block or opens the next one. Yields Bell(n)
    tuples.
    """
    labels = [0] * n

    def extend(i, num_blocks):
        if i == n:
            yield tuple(labels)
            return
        for block in range(num_blocks + 1):
            labels[i] = block
            yield from extend(i + 1, max(num_blocks, block + 1))

    yield from extend(1, 1)


def test_set_partitions_are_the_bell_numbers():
    assert [sum(1 for _ in set_partitions(n)) for n in range(1, 10)] == [
        1,
        2,
        5,
        15,
        52,
        203,
        877,
        4140,
        21147,
    ]
    assert len(set(set_partitions(6))) == 203


def tiny_network(kind: str, n: int, seed: int, *, directed: bool):
    """A small weighted network; ``kind`` picks the structure.

    ``random``: links with probability 0.35. ``two-groups``: two random groups,
    dense inside. ``three-groups``: unweighted, three groups by node id mod 3.
    ``ring``: a weighted ring with a few chords. Nodes that get no link stay in
    the network as isolated nodes.
    """
    rng = np.random.default_rng([seed, n])
    pairs = [
        (u, v)
        for u in range(1, n + 1)
        for v in range(1, n + 1)
        if u != v and (directed or u < v)
    ]
    links = {}
    if kind == "random":
        for u, v in pairs:
            if rng.random() < 0.35:
                links[(u, v)] = float(rng.uniform(0.5, 2.0))
    elif kind == "two-groups":
        group = rng.integers(0, 2, n + 1)
        for u, v in pairs:
            if rng.random() < (0.8 if group[u] == group[v] else 0.1):
                links[(u, v)] = float(rng.uniform(0.5, 2.0))
    elif kind == "three-groups":
        for u, v in pairs:
            if rng.random() < (0.9 if u % 3 == v % 3 else 0.15):
                links[(u, v)] = 1.0
    elif kind == "ring":
        for u in range(1, n + 1):
            links[(u, u % n + 1)] = float(rng.uniform(0.5, 3.0))
            target = int(rng.integers(1, n + 1))
            if target != u and rng.random() < 0.3:
                links[(u, target)] = links.get((u, target), 0.0) + 0.5
    else:
        raise ValueError(kind)
    return list(range(1, n + 1)), [(s, t, w) for (s, t), w in links.items()]


def _two_triangles():
    links = [(1, 2), (1, 3), (2, 3), (3, 4), (4, 5), (4, 6), (5, 6)]
    return list(range(1, 7)), [(s, t, 1.0) for s, t in links]


def _write_pajek(path, nodes, links, *, directed):
    lines = [f"*Vertices {len(nodes)}"] + [f'{node} "{node}"' for node in nodes]
    lines.append("*Arcs" if directed else "*Edges")
    lines += [f"{source} {target} {weight!r}" for source, target, weight in links]
    path.write_text("\n".join(lines) + "\n")
    return path


class Scorer:
    """Reference codelength of any two-level partition of one network."""

    def __init__(self, nodes, links, flow_options, objective):
        self.nodes = nodes
        directed = flow_options.get("directed", False)
        self.recorded = flow_options.get("recorded_teleportation", False)
        self.flow = flow(
            nodes,
            links,
            "directed" if directed else "undirected",
            recorded=self.recorded,
            to_nodes=flow_options.get("to_nodes", False),
        )
        self.links = [(s, t, v) for (s, t), v in self.flow.link.items()]
        self.undirected = not directed
        self.total_degree = total_degree(links, undirected=not directed)
        self.objective = objective

    def __call__(self, labels) -> float:
        sizes = {}
        paths = {}
        for node, label in zip(self.nodes, labels, strict=True):
            sizes[label] = sizes.get(label, 0) + 1
            paths[node] = (label + 1, sizes[label])
        return self.tree(paths)

    def tree(self, paths) -> float:
        """Reference codelength of a tree, given as ``{node: path}``."""
        leaves = {}
        for node, path in paths.items():
            leaf = Leaf(path=path, flow=self.flow.node[node])
            if self.recorded:
                leaf.teleport_flow = self.flow.teleport_flow[node]
                leaf.teleport_weight = self.flow.teleport_weight[node]
            leaves[node] = leaf
        partition = Partition(
            leaves=leaves,
            links=self.links,
            undirected_links=self.undirected,
            recorded_teleportation=self.recorded,
            total_degree=self.total_degree,
            **self.objective,
        )
        return codelength(partition).total

    def optimum(self):
        scores = {labels: self(labels) for labels in set_partitions(len(self.nodes))}
        best = min(scores, key=scores.get)
        return best, scores[best], scores


def labels_of(result, nodes):
    """Infomap's top modules as a restricted growth string over ``nodes``."""
    modules = dict(result.modules())
    first_seen = {}
    return tuple(first_seen.setdefault(modules[n], len(first_seen)) for n in nodes)


def _check_against_optimum(tmp_path, nodes, links, flow_name, objective_name):
    flow_options = FLOWS[flow_name]
    engine, reference = OBJECTIVES[objective_name]
    scorer = Scorer(nodes, links, flow_options, reference)
    _, optimum, scores = scorer.optimum()

    path = _write_pajek(
        tmp_path / "net.net",
        nodes,
        links,
        directed=flow_options.get("directed", False),
    )
    result = run(
        str(path),
        silent=True,
        two_level=True,
        num_trials=5,
        seed=1,
        **flow_options,
        **engine,
    )
    found = labels_of(result, nodes)
    # Never below the optimum, and priced as the reference prices the same partition.
    assert result.codelength >= optimum - REL * max(1.0, optimum)
    assert result.codelength == pytest.approx(scores[found], rel=REL, abs=ABS)
    return result.codelength - optimum


# Name -> (kind, n, seed) for tiny_network, drawn directed for the directed flows.
FAST_NETWORKS = {
    "twotriangles": None,
    **{
        f"{kind}-{n}": (kind, n, 1)
        for kind in ("random", "two-groups", "three-groups", "ring")
        for n in (5, 6)
    },
    "two-groups-7": ("two-groups", 7, 2),
}


def _fast_network(name, flow_name):
    if FAST_NETWORKS[name] is None:
        return _two_triangles()
    kind, n, seed = FAST_NETWORKS[name]
    return tiny_network(kind, n, seed, directed=FLOWS[flow_name].get("directed", False))


@pytest.mark.parametrize("objective", OBJECTIVES)
@pytest.mark.parametrize("flow_name", FLOWS)
@pytest.mark.parametrize("network", FAST_NETWORKS)
def test_two_level_search_is_never_below_the_optimum(
    tmp_path, network, flow_name, objective
):
    nodes, links = _fast_network(network, flow_name)
    _check_against_optimum(tmp_path, nodes, links, flow_name, objective)


def _write_clu(path, nodes, labels):
    lines = ["# node module"] + [
        f"{node} {label + 1}" for node, label in zip(nodes, labels, strict=True)
    ]
    path.write_text("\n".join(lines) + "\n")
    return path


@pytest.mark.parametrize("objective", OBJECTIVES)
@pytest.mark.parametrize("flow_name", FLOWS)
@pytest.mark.parametrize("network", ["twotriangles", "two-groups-6", "ring-6"])
def test_scored_partitions_match_the_reference(tmp_path, network, flow_name, objective):
    nodes, links = _fast_network(network, flow_name)
    flow_options = FLOWS[flow_name]
    engine, reference = OBJECTIVES[objective]
    scorer = Scorer(nodes, links, flow_options, reference)
    best, _, _ = scorer.optimum()

    everything = list(set_partitions(len(nodes)))
    rng = np.random.default_rng(len(nodes))
    sample = {
        everything[0],  # one module
        everything[-1],  # every node on its own
        best,
        *(everything[i] for i in rng.choice(len(everything), 6, replace=False)),
    }
    path = _write_pajek(
        tmp_path / "net.net", nodes, links, directed=flow_options.get("directed", False)
    )
    for labels in sorted(sample):
        clu = _write_clu(tmp_path / "partition.clu", nodes, labels)
        result = run(
            str(path),
            silent=True,
            cluster_data=str(clu),
            no_infomap=True,
            **flow_options,
            **engine,
        )
        assert labels_of(result, nodes) == labels
        assert result.codelength == pytest.approx(scorer(labels), rel=REL, abs=ABS), (
            labels
        )


# --- the slow sweep ------------------------------------------------------------

SWEEP_KINDS = ("random", "two-groups", "three-groups", "ring")
SWEEP_SEEDS = range(1, 4)


@pytest.mark.slow
@pytest.mark.parametrize("flow_name", FLOWS)
@pytest.mark.parametrize("n", [7, 8, 9])
def test_search_quality(tmp_path, n, flow_name):
    # The hard checks of the default run on bigger networks, plus how often the
    # search misses. Only the objectives that never missed are held to it.
    directed = FLOWS[flow_name].get("directed", False)
    misses = {name: [] for name in OBJECTIVES}
    for kind in SWEEP_KINDS:
        for seed in SWEEP_SEEDS:
            nodes, links = tiny_network(kind, n, seed, directed=directed)
            for name in OBJECTIVES:
                gap = _check_against_optimum(tmp_path, nodes, links, flow_name, name)
                if gap > REL * 10:
                    misses[name].append((kind, seed, gap))
    runs = len(SWEEP_KINDS) * len(SWEEP_SEEDS)
    for name in ("plain", "markov-time", "entropy"):
        assert len(misses[name]) <= runs // 6, misses[name]


# --- multilevel ------------------------------------------------------------------


def hierarchies(nodes):
    """Every tree Infomap can return over ``nodes``, as ``{node: path}``.

    Either one module that holds every node, or two or more top modules. A
    module holds its nodes as leaves, or is split again into two or more
    modules. As in Infomap's trees, leaves never sit beside modules and no
    module is the only child of another. That gives 1, 2, 8, 58, 612, 8374
    and 140408 trees for n = 1 to 7.
    """
    nodes = list(nodes)
    yield {node: (1, i + 1) for i, node in enumerate(nodes)}
    for tree in _splits(nodes):
        yield dict(tree)


def _module(items):
    # Each arrangement of one module, as (item, path inside the module) pairs.
    yield tuple((item, (i + 1,)) for i, item in enumerate(items))
    yield from _splits(items)


def _splits(items):
    # Each split of items into two or more modules, each arranged by _module.
    for labels in set_partitions(len(items)):
        num_blocks = max(labels) + 1
        if num_blocks > 1:
            blocks = [
                [
                    item
                    for item, label in zip(items, labels, strict=True)
                    if label == block
                ]
                for block in range(num_blocks)
            ]
            yield from _arrange(blocks, 1)


def _arrange(blocks, index):
    if not blocks:
        yield ()
        return
    for inner in _module(blocks[0]):
        head = tuple((item, (index, *path)) for item, path in inner)
        for tail in _arrange(blocks[1:], index + 1):
            yield head + tail


def test_hierarchies_are_every_tree():
    counts = [sum(1 for _ in hierarchies(range(n))) for n in range(1, 7)]
    assert counts == [1, 2, 8, 58, 612, 8374]
    assert len({tuple(sorted(t.items())) for t in hierarchies(range(5))}) == 612


# A lower Markov time favours more modules. It is the objective under which
# a tree deeper than two levels is the optimum on networks this small.
MULTILEVEL_OBJECTIVES = {
    **OBJECTIVES,
    "markov-time-0.25": ({"markov_time": 0.25}, {"markov_time": 0.25}),
}


def _check_multilevel(tmp_path, nodes, links, flow_name, objective_name):
    flow_options = FLOWS[flow_name]
    engine, reference = MULTILEVEL_OBJECTIVES[objective_name]
    scorer = Scorer(nodes, links, flow_options, reference)
    optimum = deep_optimum = two_level_optimum = math.inf
    for tree in hierarchies(nodes):
        score = scorer.tree(tree)
        optimum = min(optimum, score)
        if all(len(path) == 2 for path in tree.values()):
            two_level_optimum = min(two_level_optimum, score)
        else:
            deep_optimum = min(deep_optimum, score)

    path = _write_pajek(
        tmp_path / "net.net",
        nodes,
        links,
        directed=flow_options.get("directed", False),
    )
    options = {"silent": True, "num_trials": 5, "seed": 1, **flow_options, **engine}
    result = run(str(path), **options)
    tree = {node.node_id: tuple(node.path) for node in result.tree() if node.is_leaf}
    # Never below the optimum over every tree, and priced as the reference
    # prices the same tree.
    assert result.codelength >= optimum - REL * max(1.0, optimum)
    assert result.codelength == pytest.approx(scorer.tree(tree), rel=REL, abs=ABS)
    # Never worse than its flattenings, all three priced by the reference, nor
    # than two-level search with the same seed (flattening.py).
    found = Flattenings(
        result.codelength,
        scorer.tree(flatten(tree, 1)),
        scorer.tree(flatten(tree, -1)),
    )
    assert not found.violations()
    two_level = run(str(path), two_level=True, **options)
    assert codes_no_worse(result.codelength, two_level.codelength)
    # The gap to the optimum, and how much a deeper tree saves, if anything.
    return result.codelength - optimum, max(0.0, two_level_optimum - deep_optimum)


# A path 2 - 1 - 4 and a ten times weaker triangle 3, 4, 5. At Markov time
# 0.25 the optimum puts 1, 2 and 4 in one module, each in a sub-module of its
# own, and 3 and 5 in modules of their own. Infomap returns the best two-level
# partition, 0.006 bits above it: {3, 5} together and the rest alone. A super
# module over 1, 2 and 4 codes worse than that partition. Splitting {3, 5}
# into two modules of one node is a sub-partition the search rejects, and the
# two-level partition with every node alone, which codes worse too, is one it
# does not search further (see the module docstring).
DEEP_5 = (
    [1, 2, 3, 4, 5],
    [(1, 2, 1.0), (1, 4, 1.0), (3, 4, 0.1), (3, 5, 0.1), (4, 5, 0.1)],
)
# The default run; the slow sweep covers the rest.
MULTILEVEL_FLOWS = ["undirected", "directed", "recorded"]


@pytest.mark.parametrize("objective", MULTILEVEL_OBJECTIVES)
@pytest.mark.parametrize("flow_name", MULTILEVEL_FLOWS)
@pytest.mark.parametrize("network", ["deep-5", "two-groups-5"])
def test_multilevel_search_is_never_below_the_optimum(
    tmp_path, network, flow_name, objective
):
    if network == "deep-5":
        nodes, links = DEEP_5
        if FLOWS[flow_name].get("directed"):
            links += [(t, s, w) for s, t, w in links]
    else:
        nodes, links = _fast_network(network, flow_name)
    _check_multilevel(tmp_path, nodes, links, flow_name, objective)


def test_a_deeper_tree_can_be_the_optimum(tmp_path):
    # The search misses it on macOS, a search limit (see DEEP_5). The tree it
    # returns depends on the standard library (#342), so it is held only to
    # the checks in _check_multilevel.
    _, saving = _check_multilevel(tmp_path, *DEEP_5, "undirected", "markov-time-0.25")
    assert saving > 0.005


@pytest.mark.slow
@pytest.mark.parametrize("flow_name", FLOWS)
@pytest.mark.parametrize("n", [5, 6])
def test_multilevel_search_quality(tmp_path, n, flow_name):
    # The checks of the default run on every kind of tiny network. How often
    # the search misses, and how often a deeper tree is the optimum, is in the
    # module docstring.
    directed = FLOWS[flow_name].get("directed", False)
    for kind in SWEEP_KINDS:
        for seed in SWEEP_SEEDS if n == 5 else SWEEP_SEEDS[:2]:
            nodes, links = tiny_network(kind, n, seed, directed=directed)
            for name in MULTILEVEL_OBJECTIVES:
                _check_multilevel(tmp_path, nodes, links, flow_name, name)
