"""Metamorphic checks: changes to the input with a known effect on the result.

Each relation transforms a network and compares the run on the original with
the run on the transformed network (#1171). Most compare a fixed partition,
scored with ``--cluster-data`` and ``--no-infomap``, so the search is not
involved. Where the search is compared, it is by the best codelength of a seed
sweep, and each side's best tree is also rescored on the other side. Seeds
reach different trees with different standard libraries (#342, #884), so no
test pins the tree of one seed.

Relations, and what the theory says
-----------------------------------
Permuting node ids and the link order
    The map equation depends on the flow and the partition, not on labels or
    input order. A fixed partition has the same codelength, and so does each
    side's best tree on the other side.

Scaling every weight by ``c``
    Flow is a ratio of weights, so node flow, link flow and the codelength are
    unchanged. The regularized prior is built from ``s / k`` over the average
    weight and scales away too. The entropy correction does not: it charges
    ``P / (2 D ln 2)`` for ``P`` free parameters, with ``D`` the number of
    observed walk steps, which scales with the weights (or is the link count
    where that is larger). ``D`` times the correction is unchanged.

Splitting a link into two with the same summed weight
    Input links with the same source and target are summed, so the network is
    the same for every objective, the ones that count links included. In an
    undirected network ``a b`` and ``b a`` are the same link, so splitting it
    into one of each must give the same as splitting it into two ``a b``.

Adding an isolated node
    With no teleportation onto it, the node gets no flow. Its codeword is never
    used and it never enters or exits a module, so the codelength is unchanged
    wherever it is placed, except for the terms that count modules or
    codewords. The module preference moves by the change in ``|K - K_pref|``.
    The entropy correction charges one free parameter per added codeword at
    the same ``D``: one in an existing module, and two as a module of its own
    (a codeword in the parent's codebook, and the node's own codeword next to
    the new module's exit codeword) when there is more than one top module. This holds for undirected flow, for
    unrecorded teleportation (to nodes the walker lands on the node but steps
    on at once, so no coded link step ends there), for recorded teleportation
    to links (the node receives no links), for undirdir, outdirdir and rawdir,
    and at any Markov time. With recorded teleportation to nodes, and with
    ``--regularized``, whose prior links the node to every other node, it gets
    flow and the codelength changes; ``test_reference_flow`` covers that flow.

Undirected versus directed with both directions
    Give each undirected link ``a - b`` of weight ``w`` the arcs ``a -> b`` and
    ``b -> a`` of weight ``w``. With teleportation to links, the directed
    stationary distribution is the undirected ``p = s / W``: a teleport lands
    in proportion to out-weight, like a link step from the stationary walk
    (Lambiotte & Rosvall 2012). Undirdir, outdirdir and rawdir give link flow
    ``w / W`` on the symmetric network. So node flow and the codelength are the
    undirected ones under all four. The directed regularized prior on a
    symmetric network satisfies detailed balance for both its link steps and
    its ``u_a u_b`` teleportation, so it has the undirected regularized flow.
    Not promised: recorded teleportation (the teleport steps are coded),
    teleportation to nodes, and the entropy correction's ``D``, which counts
    every listed link and so doubles. ``D`` times the correction is the same.

Thread count
    With a fixed seed the result does not depend on the thread count: each task
    of the recursive search owns its sub-Infomap, and ``--parallel-trials``
    gives trial ``i`` the seed it has in a serial run. Nor may an Infomap
    object that runs twice give two results. ``--inner-parallelization`` is
    documented to change the partition and is not covered.
"""

from __future__ import annotations

import functools
import logging
import math
import os
import subprocess
import sys
import warnings
from pathlib import Path

import numpy as np
import pytest
from infomap import Infomap, run
from reference_codelength import total_degree

pytestmark = pytest.mark.fast

REPO = Path(__file__).resolve().parents[2]
EXAMPLES = REPO / "examples" / "networks"
NETWORKS = REPO / "test" / "fixtures" / "networks"

REL = 1e-10
ABS = 1e-12
# How far apart the best codelengths of two seed sweeps may be.
SEARCH_NOISE = 0.01
# The networks are tiny, so starting threads costs more than the run; the
# thread count has its own tests at the end.
SERIAL = {"num_threads": 1}

FILES = [
    EXAMPLES / "twotriangles.net",
    EXAMPLES / "ninetriangles.net",
    EXAMPLES / "modular_w.net",
    EXAMPLES / "modular_wd.net",
    NETWORKS / "unbalanced_hierarchy.net",
    NETWORKS / "lossy_benchmark.net",
    NETWORKS / "teleport_directed.net",
    NETWORKS / "twotriangles_flow.net",
]

VARIANTS = {
    "undirected": {},
    "directed": {"directed": True},
    "directed-to-nodes": {"directed": True, "to_nodes": True},
    "recorded": {"directed": True, "recorded_teleportation": True},
    "recorded-to-nodes": {
        "directed": True,
        "recorded_teleportation": True,
        "to_nodes": True,
    },
    "undirdir": {"flow_model": "undirdir"},
    "outdirdir": {"flow_model": "outdirdir"},
    "rawdir": {"flow_model": "rawdir"},
    "markov-time": {"markov_time": 2.0},
    "markov-time-directed": {"directed": True, "markov_time": 0.5},
    "preferred": {"preferred_number_of_modules": 3},
    "entropy": {"entropy_corrected": True},
    "entropy-directed": {"entropy_corrected": True, "directed": True},
    "regularized": {"regularized": True},
    "regularized-directed": {"regularized": True, "directed": True},
}

# For the seed sweeps: one variant per kind of objective.
SEARCH_VARIANTS = {
    "undirected": VARIANTS["undirected"],
    "directed": VARIANTS["directed"],
    "recorded": VARIANTS["recorded"],
    "markov-time": VARIANTS["markov-time"],
    "preferred": VARIANTS["preferred"],
    "entropy": VARIANTS["entropy"],
}


# --- networks ----------------------------------------------------------------


class Net:
    """A network as plain data: node ids with weights, and weighted links."""

    def __init__(self, nodes, links, weights=None):
        self.nodes = list(nodes)
        self.links = list(links)
        self.weights = weights or dict.fromkeys(self.nodes, 1.0)

    def write(self, path: Path) -> Path:
        lines = [f"*Vertices {len(self.nodes)}"]
        lines += [f'{n} "{n}" {self.weights[n]!r}' for n in self.nodes]
        lines.append("*Links")
        lines += [f"{s} {t} {w!r}" for s, t, w in self.links]
        path.write_text("\n".join(lines) + "\n")
        return path


def _vertex_weights(path: Path) -> dict[int, float]:
    weights = {}
    section = None
    for line in path.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if line.startswith("*"):
            section = line.split()[0].lower()
            continue
        if section == "*vertices" and line:
            parts = line.split('"')
            rest = parts[-1].split() if len(parts) > 1 else line.split()[1:]
            weights[int(line.split()[0])] = float(rest[0]) if rest else 1.0
    return weights


def _read(path: Path) -> Net:
    # Infomap parses the file; the links come back as given (duplicates summed).
    result = run(str(path), silent=True, no_infomap=True, directed=True, **SERIAL)
    nodes = sorted(node.node_id for node in result.tree() if node.is_leaf)
    weights = _vertex_weights(path)
    return Net(nodes, result.links("weight"), {n: weights.get(n, 1.0) for n in nodes})


def _generated(seed: int) -> Net:
    """Three groups of eight with directed weighted links, a self-link and random node weights."""
    rng = np.random.default_rng(seed)
    group = np.repeat(np.arange(3), 8)
    links = [
        (u + 1, v + 1, float(rng.uniform(0.5, 2.0)))
        for u in range(24)
        for v in range(24)
        if u != v and rng.random() < (0.35 if group[u] == group[v] else 0.04)
    ]
    links.append((5, 5, 1.5))
    nodes = list(range(1, 25))
    return Net(nodes, links, {n: float(rng.uniform(0.5, 3.0)) for n in nodes})


NETWORK_IDS = [path.name for path in FILES] + ["generated-1", "generated-2"]


def network(name: str) -> Net:
    for path in FILES:
        if path.name == name:
            return _read(path)
    return _generated(int(name.rsplit("-", 1)[1]))


# --- running -------------------------------------------------------------------


def _write_tree(path: Path, paths: dict[int, tuple[int, ...]]) -> Path:
    lines = ["# path flow name node_id"]
    lines += [f'{":".join(map(str, p))} 0 "{n}" {n}' for n, p in paths.items()]
    path.write_text("\n".join(lines) + "\n")
    return path


def _tree(result) -> dict[int, tuple[int, ...]]:
    return {node.node_id: tuple(node.path) for node in result.tree() if node.is_leaf}


def found_tree(tmp_path, net: Net, options) -> dict[int, tuple[int, ...]]:
    """A tree to score: the one a short search finds on ``net``."""
    path = net.write(tmp_path / "search.net")
    return _tree(run(str(path), silent=True, num_trials=2, seed=3, **options, **SERIAL))


def score(tmp_path, net: Net, paths, options, tag="scored"):
    """The run that scores the fixed tree ``paths`` on ``net``."""
    net_path = net.write(tmp_path / f"{tag}.net")
    tree_path = _write_tree(tmp_path / f"{tag}.tree", paths)
    return run(
        str(net_path),
        silent=True,
        cluster_data=str(tree_path),
        no_infomap=True,
        **options,
        **SERIAL,
    )


def best_of_seeds(tmp_path, net: Net, options, seeds=range(1, 7), tag="sweep"):
    path = net.write(tmp_path / f"{tag}.net")
    results = [
        run(str(path), silent=True, num_trials=1, seed=seed, **options, **SERIAL)
        for seed in seeds
    ]
    return min(results, key=lambda r: r.codelength)


def _leaf_flow(result) -> dict[int, float]:
    return {node.node_id: node.flow for node in result.tree() if node.is_leaf}


def _same(a: float, b: float):
    assert a == pytest.approx(b, rel=REL, abs=ABS)


def _entropy_bits_per_parameter(net: Net, strength: float = 1.0) -> float:
    return strength / (2 * total_degree(net.links) * math.log(2))


# --- permuting node ids and link order -----------------------------------------


def _permuted(net: Net, rng) -> tuple[Net, dict[int, int]]:
    # New ids far from the old ones, nodes and links listed in a new order.
    new_ids = rng.permutation([1000 + 7 * i for i in range(len(net.nodes))])
    relabel = dict(zip(net.nodes, map(int, new_ids), strict=True))
    links = [(relabel[s], relabel[t], w) for s, t, w in net.links]
    order = rng.permutation(len(links))
    nodes = [relabel[n] for n in net.nodes]
    rng.shuffle(nodes)
    weights = {relabel[n]: w for n, w in net.weights.items()}
    return Net(nodes, [links[i] for i in order], weights), relabel


@pytest.mark.parametrize("variant", VARIANTS)
@pytest.mark.parametrize("name", NETWORK_IDS)
def test_permuting_ids_keeps_the_codelength_of_a_partition(tmp_path, name, variant):
    options = VARIANTS[variant]
    net = network(name)
    permuted, relabel = _permuted(net, np.random.default_rng(len(name)))
    paths = found_tree(tmp_path, net, options)
    original = score(tmp_path, net, paths, options)
    moved = score(
        tmp_path, permuted, {relabel[n]: p for n, p in paths.items()}, options
    )
    _same(moved.codelength, original.codelength)
    flow = _leaf_flow(moved)
    for node, value in _leaf_flow(original).items():
        _same(flow[relabel[node]], value)


@pytest.mark.parametrize("variant", SEARCH_VARIANTS)
@pytest.mark.parametrize(
    "name",
    ["twotriangles.net", "ninetriangles.net", "modular_wd.net", "generated-1"],
)
def test_permuting_ids_keeps_the_best_codelength(tmp_path, name, variant):
    options = SEARCH_VARIANTS[variant]
    net = network(name)
    permuted, relabel = _permuted(net, np.random.default_rng(len(name)))
    back = {new: old for old, new in relabel.items()}
    original = best_of_seeds(tmp_path, net, options, tag="a")
    moved = best_of_seeds(tmp_path, permuted, options, tag="b")
    # Each best tree has the same codelength on the other network ...
    rescored = score(
        tmp_path, net, {back[n]: p for n, p in _tree(moved).items()}, options
    )
    _same(rescored.codelength, moved.codelength)
    rescored = score(
        tmp_path, permuted, {relabel[n]: p for n, p in _tree(original).items()}, options
    )
    _same(rescored.codelength, original.codelength)
    # ... and the two sweeps end up equally good, up to search noise.
    assert moved.codelength == pytest.approx(original.codelength, rel=SEARCH_NOISE)


# --- scaling weights -------------------------------------------------------------


@pytest.mark.parametrize("factor", [1e-3, 3.0, 1e3])
@pytest.mark.parametrize("variant", VARIANTS)
@pytest.mark.parametrize("name", NETWORK_IDS)
def test_scaling_weights(tmp_path, name, variant, factor):
    options = VARIANTS[variant]
    net = network(name)
    scaled = Net(net.nodes, [(s, t, w * factor) for s, t, w in net.links], net.weights)
    paths = found_tree(tmp_path, net, options)
    original = score(tmp_path, net, paths, options)
    result = score(tmp_path, scaled, paths, options)
    flow = _leaf_flow(result)
    for node, value in _leaf_flow(original).items():
        _same(flow[node], value)
    if not options.get("entropy_corrected"):
        _same(result.codelength, original.codelength)
        return
    # The correction is P / (2 D ln 2): D times it does not change.
    plain_options = {k: v for k, v in options.items() if k != "entropy_corrected"}
    plain = score(tmp_path, net, paths, plain_options).codelength
    scaled_plain = score(tmp_path, scaled, paths, plain_options).codelength
    _same(scaled_plain, plain)
    _same(
        (result.codelength - scaled_plain) / _entropy_bits_per_parameter(scaled),
        (original.codelength - plain) / _entropy_bits_per_parameter(net),
    )


# --- splitting links -------------------------------------------------------------


def _split(net: Net, rng, *, reverse: bool, every: bool = False) -> Net:
    links = []
    for s, t, w in net.links:
        if s != t and (every or rng.random() < 0.5):
            share = float(rng.uniform(0.1, 0.9))
            rest = (t, s) if reverse else (s, t)
            links += [(s, t, w * share), (*rest, w * (1 - share))]
        else:
            links.append((s, t, w))
    return Net(net.nodes, links, net.weights)


@pytest.mark.parametrize("variant", VARIANTS)
@pytest.mark.parametrize("name", NETWORK_IDS)
def test_splitting_a_link_into_duplicates(tmp_path, name, variant):
    options = VARIANTS[variant]
    net = network(name)
    split = _split(net, np.random.default_rng(len(name)), reverse=False)
    paths = found_tree(tmp_path, net, options)
    _same(
        score(tmp_path, split, paths, options).codelength,
        score(tmp_path, net, paths, options).codelength,
    )


_REVERSE_SPLIT_COUNTS_LINKS = pytest.mark.xfail(
    reason="undirected input sums duplicate links only when they have the same direction, "
    "so a b plus b a counts as two links and a b twice as one, and the objectives that count "
    "links (the regularized prior's degrees, the entropy correction's sample size) differ",
    strict=True,
    raises=AssertionError,
)


@pytest.mark.parametrize(
    "variant",
    [
        "undirected",
        "markov-time",
        "preferred",
        pytest.param("entropy", marks=_REVERSE_SPLIT_COUNTS_LINKS),
        pytest.param("regularized", marks=_REVERSE_SPLIT_COUNTS_LINKS),
    ],
)
@pytest.mark.parametrize(
    "name", ["twotriangles.net", "ninetriangles.net", "lossy_benchmark.net"]
)
def test_undirected_split_does_not_depend_on_direction(tmp_path, name, variant):
    options = VARIANTS[variant]
    net = network(name)
    same = _split(net, np.random.default_rng(1), reverse=False, every=True)
    reverse = _split(net, np.random.default_rng(1), reverse=True, every=True)
    paths = found_tree(tmp_path, net, options)
    original = score(tmp_path, net, paths, options)
    _same(score(tmp_path, same, paths, options).codelength, original.codelength)
    _same(score(tmp_path, reverse, paths, options).codelength, original.codelength)


# --- adding an isolated node -------------------------------------------------------

# Variants under which the isolated node gets no flow.
ZERO_FLOW_VARIANTS = [
    name
    for name in VARIANTS
    if name not in ("recorded-to-nodes", "regularized", "regularized-directed")
]


def _with_isolated(net: Net) -> tuple[Net, int]:
    node = max(net.nodes) + 1
    return Net([*net.nodes, node], net.links, {**net.weights, node: 1.0}), node


def _isolated_paths(paths, node, *, own_module: bool):
    if own_module:
        top = max(p[0] for p in paths.values())
        return {**paths, node: (top + 1, 1)}
    module = paths[min(paths)][:-1]
    siblings = [p[-1] for p in paths.values() if p[:-1] == module]
    return {**paths, node: (*module, max(siblings) + 1)}


@pytest.mark.parametrize("own_module", [False, True], ids=["in-module", "own-module"])
@pytest.mark.parametrize("variant", ZERO_FLOW_VARIANTS)
@pytest.mark.parametrize("name", NETWORK_IDS)
def test_adding_an_isolated_node(tmp_path, name, variant, own_module):
    options = VARIANTS[variant]
    net = network(name)
    extended, node = _with_isolated(net)
    paths = found_tree(tmp_path, net, options)
    original = score(tmp_path, net, paths, options)
    result = score(
        tmp_path, extended, _isolated_paths(paths, node, own_module=own_module), options
    )
    assert _leaf_flow(result)[node] == 0.0
    expected = original.codelength
    one_level = original.one_level_codelength
    if options.get("preferred_number_of_modules"):
        preferred = options["preferred_number_of_modules"]
        top = original.num_top_modules
        if own_module:
            expected += abs(top + 1 - preferred) - abs(top - preferred)
    if options.get("entropy_corrected"):
        expected += (2 if own_module else 1) * _entropy_bits_per_parameter(net)
        one_level += _entropy_bits_per_parameter(net)
    _same(result.codelength, expected)
    _same(result.one_level_codelength, one_level)


@pytest.mark.parametrize("variant", ["undirected", "directed", "markov-time"])
@pytest.mark.parametrize(
    "name", ["twotriangles.net", "ninetriangles.net", "modular_wd.net"]
)
def test_adding_an_isolated_node_keeps_the_best_codelength(tmp_path, name, variant):
    options = SEARCH_VARIANTS[variant]
    net = network(name)
    extended, _ = _with_isolated(net)
    original = best_of_seeds(tmp_path, net, options, tag="a")
    result = best_of_seeds(tmp_path, extended, options, tag="b")
    assert result.codelength == pytest.approx(original.codelength, rel=SEARCH_NOISE)


# --- undirected versus both directions --------------------------------------------


def _undirected(net: Net) -> Net:
    weights = {}
    for s, t, w in net.links:
        key = (min(s, t), max(s, t))
        weights[key] = weights.get(key, 0.0) + w
    return Net(net.nodes, [(s, t, w) for (s, t), w in weights.items()], net.weights)


def _both_directions(net: Net) -> Net:
    arcs = [*net.links, *((t, s, w) for s, t, w in net.links if s != t)]
    return Net(net.nodes, arcs, net.weights)


BOTH_DIRECTION_MODELS = {
    "directed": {"directed": True},
    "undirdir": {"flow_model": "undirdir"},
    "outdirdir": {"flow_model": "outdirdir"},
    "rawdir": {"flow_model": "rawdir"},
}


_DIRECTED_DEGREE_SUM_COUNTS_SELF_LINKS_ONCE = pytest.mark.xfail(
    reason="StateNetwork::sumWeightedDegree and sumDegree count a self-link once, as an "
    "undirected link end; the directed regularized prior takes its average weight from them "
    "but counts a self-link as both an out-link and an in-link, so with a self-link the "
    "directed prior on a symmetric network is not the undirected one",
    strict=True,
    raises=AssertionError,
)


def _both_direction_cases():
    for name in NETWORK_IDS:
        for objective in (
            "undirected",
            "markov-time",
            "preferred",
            "entropy",
            "regularized",
        ):
            for model in BOTH_DIRECTION_MODELS:
                marks = ()
                if objective == "regularized":
                    # --regularized is defined for undirected and directed flow only (#1189).
                    if model != "directed":
                        continue
                    # The generated networks have a self-link.
                    if name.startswith("generated"):
                        marks = _DIRECTED_DEGREE_SUM_COUNTS_SELF_LINKS_ONCE
                yield pytest.param(
                    name,
                    objective,
                    model,
                    marks=marks,
                    id=f"{name}-{objective}-{model}",
                )


@pytest.mark.parametrize(("name", "objective", "model"), _both_direction_cases())
def test_undirected_equals_both_directions(tmp_path, name, objective, model):
    undirected_options = VARIANTS[objective]
    directed_options = {**undirected_options, **BOTH_DIRECTION_MODELS[model]}
    net = _undirected(network(name))
    arcs = _both_directions(net)
    paths = found_tree(tmp_path, net, undirected_options)
    undirected = score(tmp_path, net, paths, undirected_options, tag="undirected")
    directed = score(tmp_path, arcs, paths, directed_options, tag="directed")
    flow = _leaf_flow(directed)
    for node, value in _leaf_flow(undirected).items():
        _same(flow[node], value)
    if objective != "entropy":
        _same(directed.codelength, undirected.codelength)
        return
    # The sample size D counts both arcs; D times the correction is the same.
    plain_directed = {
        k: v for k, v in directed_options.items() if k != "entropy_corrected"
    }
    plain = score(tmp_path, net, paths, {}, tag="undirected").codelength
    plain_arcs = score(tmp_path, arcs, paths, plain_directed, tag="directed").codelength
    _same(plain_arcs, plain)
    _same(
        (directed.codelength - plain_arcs) / _entropy_bits_per_parameter(arcs),
        (undirected.codelength - plain) / _entropy_bits_per_parameter(net),
    )


@pytest.mark.parametrize(
    "name", ["twotriangles.net", "ninetriangles.net", "modular_w.net"]
)
def test_undirected_equals_both_directions_best_codelength(tmp_path, name):
    net = _undirected(network(name))
    undirected = best_of_seeds(tmp_path, net, {}, tag="a")
    directed = best_of_seeds(
        tmp_path, _both_directions(net), {"directed": True}, tag="b"
    )
    assert directed.codelength == pytest.approx(undirected.codelength, rel=SEARCH_NOISE)


# --- thread count ----------------------------------------------------------------

THREAD_NETWORKS = [
    "ninetriangles.net",
    "modular_w.net",
    "unbalanced_hierarchy.net",
    "generated-1",
]
# The recorded variants are in test_parallel_trial_worker_runs_a_second_trial_alike.
THREAD_VARIANTS = [
    name for name in VARIANTS if not VARIANTS[name].get("recorded_teleportation")
]


def _modules(result) -> set[frozenset]:
    """Every module as its set of leaves, whatever its position in the tree."""
    members: dict[tuple[int, ...], set[int]] = {}
    for node, path in _tree(result).items():
        for depth in range(1, len(path)):
            members.setdefault(path[:depth], set()).add(node)
    return {frozenset(m) for m in members.values()}


def _run_threads(path, options, **extra):
    return run(str(path), silent=True, num_trials=3, seed=5, **options, **extra)


@pytest.mark.parametrize("variant", THREAD_VARIANTS)
@pytest.mark.parametrize("name", THREAD_NETWORKS)
def test_thread_count_does_not_change_the_result(tmp_path, name, variant):
    path = network(name).write(tmp_path / "net.net")
    options = VARIANTS[variant]
    serial = _run_threads(path, options, num_threads=1)
    threaded = _run_threads(path, options, num_threads=4)
    assert threaded.codelengths == serial.codelengths
    assert _tree(threaded) == _tree(serial)
    for threads in (1, 2, 4):
        parallel = _run_threads(
            path, options, num_threads=threads, parallel_trials=True
        )
        # A worker rebuilds the network for each trial, so a sum can come out in
        # the last bit.
        assert parallel.codelengths == pytest.approx(serial.codelengths, rel=1e-14)
        assert parallel.codelength == pytest.approx(serial.codelength, rel=1e-14)
        # The same tree, though not always in the same order (see below).
        assert _modules(parallel) == _modules(serial), threads


def test_omp_num_threads_does_not_change_the_result(tmp_path):
    path = network("generated-1").write(tmp_path / "net.net")
    script = (
        "import sys; from infomap import run\n"
        "for extra in ({}, {'parallel_trials': True}):\n"
        "    r = run(sys.argv[1], silent=True, num_trials=3, seed=5, **extra)\n"
        "    print(repr(r.codelengths), sorted((n.node_id, n.path) for n in r.tree() if n.is_leaf))\n"
    )
    outputs = []
    for threads in ("1", "4"):
        env = {**os.environ, "OMP_NUM_THREADS": threads}
        env.pop("INFOMAP_NUM_THREADS", None)
        outputs.append(
            subprocess.run(
                [sys.executable, "-c", script, str(path)],
                env=env,
                check=True,
                capture_output=True,
                text=True,
            ).stdout
        )
    assert outputs[0] == outputs[1]


# --- findings ----------------------------------------------------------------------

# Two directed cycles bridged by one arc, ending in a dangling path 6 -> 7 -> 8.
SINK = [(1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 6), (6, 4), (6, 7), (7, 8)]

_DANGLING_FLOW_LEAKS = pytest.mark.xfail(
    reason="InfomapBase::initEnterExitFlow adds to m_sumDanglingFlow without resetting it, "
    "so a second run on the same instance (an Infomap object run twice, or a --parallel-trials "
    "worker running its second trial) charges every dangling node's flow twice in the recorded "
    "teleportation enter flow",
    strict=True,
    raises=AssertionError,
)


@_DANGLING_FLOW_LEAKS
@pytest.mark.parametrize("to_nodes", [False, True], ids=["to-links", "to-nodes"])
def test_infomap_object_runs_twice_alike(to_nodes):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        im = Infomap(
            silent=True,
            directed=True,
            recorded_teleportation=True,
            to_nodes=to_nodes,
            two_level=True,
            num_trials=1,
            seed=1,
        )
    im.add_links(SINK)
    first = im.run()
    second = im.run()
    assert second.codelength == first.codelength
    assert _modules(second) == _modules(first)


@functools.cache
def _openmp() -> bool:
    """Whether the extension runs with OpenMP, read from the run's log."""
    lines = []
    handler = logging.Handler()
    handler.emit = lambda record: lines.append(record.getMessage())
    logger = logging.getLogger("infomap")
    level = logger.level
    logger.addHandler(handler)
    logger.setLevel(logging.DEBUG)
    try:
        run([(1, 2)], num_trials=1)
    finally:
        logger.removeHandler(handler)
        logger.setLevel(level)
    return any("Runtime" in line and "OpenMP" in line for line in lines)


@pytest.mark.skipif(
    not _openmp(), reason="--parallel-trials runs serially without OpenMP"
)
@_DANGLING_FLOW_LEAKS
@pytest.mark.parametrize("variant", ["recorded", "recorded-to-nodes"])
@pytest.mark.parametrize("name", ["sink", "ninetriangles.net"])
def test_parallel_trial_worker_runs_a_second_trial_alike(tmp_path, name, variant):
    net = (
        Net(range(1, 9), [(s, t, 1.0) for s, t in SINK])
        if name == "sink"
        else network(name)
    )
    path = net.write(tmp_path / "net.net")
    options = VARIANTS[variant]
    serial = _run_threads(path, options, num_threads=1)
    # One worker runs all three trials.
    parallel = _run_threads(path, options, num_threads=1, parallel_trials=True)
    assert parallel.codelengths == serial.codelengths


@pytest.mark.xfail(
    reason="restoreBestResult rebuilds a two-level best trial with initTree, which goes "
    "through initPartition and creates the modules in node id order, and the tree is not "
    "sorted on flow again; module ids then depend on whether the best trial was the last "
    "one, and --parallel-trials always restores",
    strict=True,
    raises=AssertionError,
)
@pytest.mark.parametrize(
    "extra",
    [{"num_trials": 2}, {"num_trials": 2, "parallel_trials": True}],
    ids=["serial", "parallel-trials"],
)
def test_restored_tree_is_sorted_on_flow(extra):
    # A triangle on the first ids, bridged to a clique of six: the clique has
    # more flow, so it is module 1 in a one-trial run.
    links = [(1, 2), (2, 3), (3, 1), (3, 4)]
    links += [(a, b) for a in range(4, 10) for b in range(a + 1, 10)]
    one_trial = run(links, silent=True, two_level=True, num_trials=1, seed=1, **SERIAL)
    result = run(links, silent=True, two_level=True, seed=1, **extra, **SERIAL)
    flows = [node.flow for node in result.tree() if node.depth == 1]
    assert flows == sorted(flows, reverse=True)
    assert dict(result.modules()) == dict(one_trial.modules())
