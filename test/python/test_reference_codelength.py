"""Compare the reported codelength with an independent reference.

``reference_codelength`` recomputes the map equation from the flow and the tree
a run reports, without any of Infomap's codelength code. A formula that is
wrong in the core is then caught even when the core's tracked and recomputed
values agree with each other (#1171).

Past bugs these checks cover: the entropy correction charged in nats (#1033),
the module-count preference missing from a scored partition (#1052), the
one-level reference (#1054) and ``num_levels`` (#1040).
"""

from __future__ import annotations

import ast
from collections import defaultdict
from pathlib import Path

import numpy as np
import pytest
from infomap import run
from reference_codelength import (
    Leaf,
    Partition,
    codelength,
    one_module,
    total_degree,
    tree_depth,
)

pytestmark = pytest.mark.fast

REPO = Path(__file__).resolve().parents[2]
EXAMPLES = REPO / "examples" / "networks"
NETWORKS = REPO / "test" / "fixtures" / "networks"
META = REPO / "test" / "fixtures" / "meta"
CLUSTERS = REPO / "test" / "fixtures" / "clusters"

REL = 1e-10
ABS = 1e-12

UNDIRECTED = [
    EXAMPLES / "twotriangles.net",
    EXAMPLES / "ninetriangles.net",
    EXAMPLES / "modular_w.net",
    NETWORKS / "unbalanced_hierarchy.net",
    NETWORKS / "lossy_benchmark.net",
    NETWORKS / "accumulate_a.net",
    EXAMPLES / "bipartite.net",
    NETWORKS / "bipartite_uneven.net",
]

DIRECTED = [
    EXAMPLES / "modular_wd.net",
    NETWORKS / "teleport_directed.net",
    NETWORKS / "twotriangles_flow.net",
    NETWORKS / "lossy_benchmark_reciprocal.net",
]

HIGHER_ORDER = [
    EXAMPLES / "states.net",
    NETWORKS / "states.net",
    NETWORKS / "states_flow.net",
    NETWORKS / "intra_identity_states.net",
    EXAMPLES / "multilayer.net",
    NETWORKS / "multilayer.net",
    EXAMPLES / "multilayer_intra_inter.net",
    NETWORKS / "intra_inter.net",
]


def _run(network, **options):
    options.setdefault("silent", True)
    options.setdefault("num_trials", 3)
    options.setdefault("seed", 7)
    return run(str(network) if isinstance(network, Path) else network, **options)


def _undirected_links(options) -> bool:
    return (
        not options.get("directed")
        and options.get("flow_model", "undirected") == "undirected"
    )


def _partition(result, options, **objective) -> Partition:
    leaves = {
        node.state_id: Leaf(
            path=tuple(node.path), flow=node.flow, physical_id=node.node_id
        )
        for node in result.tree(states=True)
        if node.is_leaf
    }
    return Partition(
        leaves=leaves,
        links=list(result.links("flow")),
        undirected_links=_undirected_links(options),
        total_degree=total_degree(result.links("weight")),
        **objective,
    )


def _assert_matches(result, partition, *, check_one_level=True):
    reference = codelength(partition)
    assert result.codelength == pytest.approx(reference.total, rel=REL, abs=ABS)
    assert result.num_levels == tree_depth(partition)
    if result.num_levels == 2:
        assert result.index_codelength == pytest.approx(
            reference.index, rel=REL, abs=ABS
        )
        assert result.module_codelength == pytest.approx(
            reference.module, rel=REL, abs=ABS
        )
    if check_one_level:
        one_level = codelength(one_module(partition))
        assert result.one_level_codelength == pytest.approx(
            one_level.total, rel=REL, abs=ABS
        )
    return reference


def _id(path: Path) -> str:
    return (
        path.name
        if path.parent.name == "networks" and "examples" in path.parts
        else f"fixture-{path.name}"
    )


# --- the reference itself ----------------------------------------------------


def test_reference_does_not_import_infomap():
    source = (Path(__file__).parent / "reference_codelength.py").read_text()
    imported = set()
    for node in ast.walk(ast.parse(source)):
        if isinstance(node, ast.Import):
            imported.update(alias.name.split(".")[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module:
            imported.add(node.module.split(".")[0])
    assert "infomap" not in imported


def test_reference_two_triangles_by_hand():
    # Two triangles joined by one link, each triangle a module, undirected and
    # unweighted: 14 link ends, each link flow 1/7, node flow degree / 14.
    links = [
        (1, 2, 1),
        (1, 3, 1),
        (2, 3, 1),
        (3, 4, 1),
        (4, 5, 1),
        (4, 6, 1),
        (5, 6, 1),
    ]
    degree = {1: 2, 2: 2, 3: 3, 4: 3, 5: 2, 6: 2}
    leaves = {
        node: Leaf(
            path=(1 if node <= 3 else 2, node if node <= 3 else node - 3),
            flow=degree[node] / 14,
        )
        for node in degree
    }
    partition = Partition(
        leaves=leaves,
        links=[(s, t, 1 / 7) for s, t, _ in links],
        undirected_links=True,
    )
    reference = codelength(partition)
    q = 1 / 14  # each module enters and exits along half of the bridge
    index = -2 * q * np.log2(1 / 2)
    module = 2 * -(
        q * np.log2(q / (q + 0.5))
        + 2 * (2 / 14) * np.log2((2 / 14) / (q + 0.5))
        + (3 / 14) * np.log2((3 / 14) / (q + 0.5))
    )
    assert reference.index == pytest.approx(index, rel=1e-14)
    assert reference.module == pytest.approx(module, rel=1e-14)
    assert reference.total == pytest.approx(2.3207303568338, rel=1e-12)


# --- plain map equation ------------------------------------------------------


@pytest.mark.parametrize("two_level", [False, True], ids=["multilevel", "two-level"])
@pytest.mark.parametrize("network", UNDIRECTED, ids=_id)
def test_undirected(network, two_level):
    options = {"two_level": two_level}
    result = _run(network, **options)
    _assert_matches(result, _partition(result, options))


@pytest.mark.parametrize("flow_model", ["directed", "undirdir", "outdirdir", "rawdir"])
@pytest.mark.parametrize("two_level", [False, True], ids=["multilevel", "two-level"])
@pytest.mark.parametrize("network", DIRECTED, ids=_id)
def test_directed_flow_models(network, two_level, flow_model):
    options = {"two_level": two_level, "flow_model": flow_model}
    result = _run(network, **options)
    _assert_matches(result, _partition(result, options))


def _expected_teleportation(result, options, network):
    """Teleportation flow and target weight per node, derived from the input.

    A node teleports ``alpha * p`` (all of ``p`` if it is dangling) to a node
    chosen by its weight with ``to_nodes`` (1 when the file gives none), or by
    its share of the total link weight it receives otherwise.
    """
    alpha = options.get("teleportation_probability", 0.15)
    out_degree = defaultdict(int)
    in_weight = defaultdict(float)
    total_weight = 0.0
    for source, target, weight in result.links("weight"):
        out_degree[source] += 1
        in_weight[target] += weight
        total_weight += weight

    node_weight = {}
    section = None
    for line in Path(network).read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if line.startswith("*"):
            section = line.split()[0].lower()
            continue
        if section == "*vertices" and line:
            parts = line.split('"')
            rest = parts[-1].split() if len(parts) > 1 else line.split()[1:]
            node_weight[int(line.split()[0])] = float(rest[0]) if rest else 1.0
    leaves = [node for node in result.tree(states=True) if node.is_leaf]
    total_node_weight = sum(node_weight.get(node.state_id, 1.0) for node in leaves)

    expected = {}
    for node in leaves:
        tele = node.flow * (alpha if out_degree[node.state_id] else 1.0)
        if options.get("to_nodes"):
            weight = node_weight.get(node.state_id, 1.0) / total_node_weight
        else:
            weight = in_weight[node.state_id] / total_weight
        expected[node.state_id] = (tele, weight)
    return expected


@pytest.mark.parametrize("to_nodes", [False, True], ids=["to-links", "to-nodes"])
@pytest.mark.parametrize("two_level", [False, True], ids=["multilevel", "two-level"])
@pytest.mark.parametrize("network", DIRECTED, ids=_id)
def test_recorded_teleportation(network, two_level, to_nodes):
    options = {
        "two_level": two_level,
        "directed": True,
        "recorded_teleportation": True,
        "to_nodes": to_nodes,
    }
    result = _run(network, **options)
    partition = _partition(result, options, recorded_teleportation=True)
    teleportation = _expected_teleportation(result, options, network)
    for node_id, leaf in partition.leaves.items():
        leaf.teleport_flow, leaf.teleport_weight = teleportation[node_id]
    _assert_matches(result, partition)


@pytest.mark.parametrize("markov_time", [0.5, 2.0, 3.0])
@pytest.mark.parametrize("two_level", [False, True], ids=["multilevel", "two-level"])
@pytest.mark.parametrize(
    "network", [EXAMPLES / "ninetriangles.net", EXAMPLES / "modular_wd.net"], ids=_id
)
def test_markov_time(network, two_level, markov_time):
    options = {
        "two_level": two_level,
        "markov_time": markov_time,
        "directed": network.name == "modular_wd.net",
    }
    result = _run(network, **options)
    _assert_matches(result, _partition(result, options, markov_time=markov_time))


# --- generated networks ------------------------------------------------------


def _planted_partition(seed, *, directed):
    """Three groups of eight, weighted, with a self-link and, if directed, a dangling node."""
    rng = np.random.default_rng(seed)
    groups = np.repeat(np.arange(3), 8)
    links = []
    for u in range(24):
        for v in range(24) if directed else range(u + 1, 24):
            if u == v:
                continue
            p = 0.45 if groups[u] == groups[v] else 0.04
            if rng.random() < p:
                links.append((u + 1, v + 1, float(rng.uniform(0.5, 2.0))))
    links.append((5, 5, 1.5))
    if directed:
        links = [link for link in links if link[0] != 24]
    return links


@pytest.mark.parametrize("seed", [1, 2, 3, 4])
@pytest.mark.parametrize("directed", [False, True], ids=["undirected", "directed"])
@pytest.mark.parametrize("two_level", [False, True], ids=["multilevel", "two-level"])
def test_generated(seed, directed, two_level):
    options = {"two_level": two_level, "directed": directed}
    result = _run(_planted_partition(seed, directed=directed), **options)
    _assert_matches(result, _partition(result, options))


# --- memory and state networks -----------------------------------------------


@pytest.mark.parametrize("directed", [False, True], ids=["undirected", "directed"])
@pytest.mark.parametrize("two_level", [False, True], ids=["multilevel", "two-level"])
@pytest.mark.parametrize("network", HIGHER_ORDER, ids=_id)
def test_higher_order(network, two_level, directed):
    options = {"two_level": two_level, "directed": directed}
    result = _run(network, **options)
    _assert_matches(result, _partition(result, options, memory=True))


# --- metadata ----------------------------------------------------------------


def _read_meta(path: Path) -> dict[int, int]:
    meta = {}
    for line in path.read_text().splitlines():
        parts = line.split("#", 1)[0].split()
        if len(parts) >= 2:
            meta[int(parts[0])] = int(parts[1])
    return meta


@pytest.mark.parametrize(
    ("network", "meta_file"),
    [
        (EXAMPLES / "twotriangles.net", META / "twotriangles.meta"),
        (EXAMPLES / "ninetriangles.net", META / "ninetriangles_crossing.meta"),
    ],
    ids=["twotriangles", "ninetriangles"],
)
@pytest.mark.parametrize("rate", [1.0, 0.3, 2.5])
@pytest.mark.parametrize(
    "unweighted", [False, True], ids=["flow-weighted", "unweighted"]
)
@pytest.mark.parametrize("two_level", [False, True], ids=["multilevel", "two-level"])
def test_meta_data(network, meta_file, rate, unweighted, two_level):
    options = {
        "two_level": two_level,
        "meta_data": str(meta_file),
        "meta_data_rate": rate,
        "meta_data_unweighted": unweighted,
    }
    result = _run(network, **options)
    partition = _partition(result, options, meta_rate=rate, meta_unweighted=unweighted)
    meta = _read_meta(meta_file)
    for node_id, leaf in partition.leaves.items():
        leaf.meta = meta.get(node_id, -1)
    _assert_matches(result, partition)


# --- module-count preference and entropy correction --------------------------

OBJECTIVE_NETWORKS = [
    EXAMPLES / "twotriangles.net",
    EXAMPLES / "ninetriangles.net",
    NETWORKS / "unbalanced_hierarchy.net",
    NETWORKS / "twotriangles_flow.net",
]


@pytest.mark.parametrize("preferred", [1, 2, 3, 5])
@pytest.mark.parametrize("two_level", [False, True], ids=["multilevel", "two-level"])
@pytest.mark.parametrize("network", OBJECTIVE_NETWORKS, ids=_id)
def test_preferred_number_of_modules(network, two_level, preferred):
    options = {"two_level": two_level, "preferred_number_of_modules": preferred}
    result = _run(network, **options)
    _assert_matches(
        result, _partition(result, options, preferred_num_modules=preferred)
    )


@pytest.mark.parametrize("strength", [1.0, 3.0])
@pytest.mark.parametrize("two_level", [False, True], ids=["multilevel", "two-level"])
@pytest.mark.parametrize(
    "network", [*OBJECTIVE_NETWORKS, EXAMPLES / "modular_wd.net"], ids=_id
)
def test_entropy_correction(network, two_level, strength):
    options = {
        "two_level": two_level,
        "entropy_corrected": True,
        "entropy_correction_strength": strength,
        "directed": network.name == "modular_wd.net",
    }
    result = _run(network, **options)
    _assert_matches(
        result, _partition(result, options, entropy_correction_strength=strength)
    )


@pytest.mark.parametrize(
    "network",
    [
        EXAMPLES / "twotriangles.net",
        EXAMPLES / "ninetriangles.net",
        NETWORKS / "twotriangles_flow.net",
    ],
    ids=_id,
)
def test_entropy_correction_and_preference_together(network):
    options = {"entropy_corrected": True, "preferred_number_of_modules": 2}
    result = _run(network, **options)
    partition = _partition(
        result, options, entropy_correction_strength=1.0, preferred_num_modules=2
    )
    _assert_matches(result, partition)


# One trial per run: the recursive search reports its own aggregate codelength, while
# a run with several trials can end on a recomputed one, and which trial does depends on
# the platform's rounding. Many seeds, so the check does not hang on one search path.
DEEP_TREE_SEEDS = range(1, 21)


def test_entropy_correction_on_deep_tree():
    options = {"entropy_corrected": True, "preferred_number_of_modules": 2}
    checked = 0
    for seed in DEEP_TREE_SEEDS:
        result = _run(
            NETWORKS / "unbalanced_hierarchy.net", seed=seed, num_trials=1, **options
        )
        # Two top modules, so the preference costs nothing and only the correction is tested.
        if result.num_levels < 3 or result.num_top_modules != 2:
            continue
        checked += 1
        partition = _partition(
            result, options, entropy_correction_strength=1.0, preferred_num_modules=2
        )
        _assert_matches(result, partition)
    assert checked > 0


@pytest.mark.parametrize(
    "network",
    [EXAMPLES / "ninetriangles.net", NETWORKS / "unbalanced_hierarchy.net"],
    ids=_id,
)
def test_preferred_number_of_modules_on_deep_tree(network):
    options = {"preferred_number_of_modules": 2}
    for seed in DEEP_TREE_SEEDS:
        result = _run(network, seed=seed, num_trials=1, **options)
        _assert_matches(result, _partition(result, options, preferred_num_modules=2))


# --- scored partitions (--cluster-data with --no-infomap) --------------------

CLUSTER_FILES = [
    "twotriangles_all_bare.tree",
    "twotriangles_ragged_branches.tree",
    "twotriangles_single_module.clu",
    "twotriangles_three_level.tree",
    "twotriangles_top_level_leaf.tree",
    "twotriangles_top_level_leaf_first.tree",
    "twotriangles_top_level_leaf_module.tree",
    "twotriangles_two_modules.clu",
]

OBJECTIVES = {
    "plain": ({}, {}),
    "preferred": ({"preferred_number_of_modules": 3}, {"preferred_num_modules": 3}),
    "entropy": ({"entropy_corrected": True}, {"entropy_correction_strength": 1.0}),
}


@pytest.mark.parametrize("objective", OBJECTIVES)
@pytest.mark.parametrize("cluster_file", CLUSTER_FILES)
def test_scored_partition(cluster_file, objective):
    engine, reference = OBJECTIVES[objective]
    options = {
        "cluster_data": str(CLUSTERS / cluster_file),
        "no_infomap": True,
        **engine,
    }
    result = _run(EXAMPLES / "twotriangles.net", **options)
    _assert_matches(result, _partition(result, options, **reference))


# --- regularized flow --------------------------------------------------------


def _with_reported_teleportation(result, partition):
    # The regularized prior's teleportation flow and target weights are not
    # derived here; they are taken from the run and only the map equation
    # over them is checked.
    for node in result.tree(states=True):
        if node.is_leaf:
            leaf = partition.leaves[node.state_id]
            leaf.teleport_flow = node.data.teleportFlow
            leaf.teleport_weight = node.data.teleportWeight
    return partition


@pytest.mark.parametrize(
    "network",
    [
        EXAMPLES / "twotriangles.net",
        EXAMPLES / "modular_w.net",
        NETWORKS / "unbalanced_hierarchy.net",
        EXAMPLES / "modular_wd.net",
        NETWORKS / "teleport_directed.net",
    ],
    ids=_id,
)
def test_regularized(network):
    options = {
        "regularized": True,
        "directed": network.name in ("modular_wd.net", "teleport_directed.net"),
    }
    result = _run(network, **options)
    partition = _partition(result, options, recorded_teleportation=True)
    _assert_matches(result, _with_reported_teleportation(result, partition))


# A node flow that is not the regularized walk's own: Markov time scales the link
# flow and not the teleportation, and node weights replace the node flow. The leaves'
# enter and exit flow came from the node flow, so the search priced modules from
# something else than the links (#1189, #1191, #1231).
@pytest.mark.parametrize(
    "objective",
    [
        {"markov_time": 0.5},
        {"markov_time": 2.0},
        {"use_node_weights_as_flow": True},
    ],
    ids=["markov-time-0.5", "markov-time-2", "node-weights"],
)
@pytest.mark.parametrize(
    "network",
    [
        EXAMPLES / "twotriangles.net",
        EXAMPLES / "modular_w.net",
        NETWORKS / "lossy_benchmark.net",
        EXAMPLES / "modular_wd.net",
        NETWORKS / "teleport_directed.net",
    ],
    ids=_id,
)
def test_regularized_with_foreign_node_flow(network, objective):
    options = {
        "regularized": True,
        "directed": network.name in ("modular_wd.net", "teleport_directed.net"),
        **objective,
    }
    result = _run(network, **options)
    partition = _partition(
        result,
        options,
        recorded_teleportation=True,
        markov_time=objective.get("markov_time", 1.0),
    )
    _assert_matches(result, _with_reported_teleportation(result, partition))


@pytest.mark.parametrize(
    "options",
    [
        {"regularized": True},
        {"directed": True, "recorded_teleportation": True, "to_nodes": True},
    ],
    ids=["regularized", "recorded-to-nodes"],
)
@pytest.mark.parametrize(
    "network", [EXAMPLES / "bipartite.net", NETWORKS / "bipartite_uneven.net"], ids=_id
)
def test_bipartite_recorded_teleportation(network, options):
    result = _run(network, **options)
    partition = _partition(result, options, recorded_teleportation=True)
    partition = _with_reported_teleportation(result, partition)
    assert sum(
        leaf.teleport_weight for leaf in partition.leaves.values()
    ) == pytest.approx(1.0)
    _assert_matches(result, partition)
