"""Compare the reported node and link flow with an independent reference.

``reference_flow`` computes the flow of each plain flow model from the network
alone, without any of Infomap's flow code. ``test_reference_codelength`` takes
the flow a run reports as given, so this closes the gap below it: a wrong flow
with a right map equation over it is caught here (#1171).

Flow does not depend on the seed, so it is compared directly, to 1e-10.
"""

from __future__ import annotations

import ast
from pathlib import Path

import numpy as np
import pytest
from infomap import Network, run
from reference_codelength import Leaf, Partition, codelength
from reference_flow import (
    _Network,
    bipartite_projection,
    flow,
    leaf_enter_exit,
    stationary,
    teleport_targets,
)

pytestmark = pytest.mark.fast

REPO = Path(__file__).resolve().parents[2]
EXAMPLES = REPO / "examples" / "networks"
NETWORKS = REPO / "test" / "fixtures" / "networks"

REL = 1e-10
ABS = 1e-12

NETWORK_FILES = [
    EXAMPLES / "twotriangles.net",
    EXAMPLES / "ninetriangles.net",
    EXAMPLES / "modular_w.net",
    EXAMPLES / "modular_wd.net",
    NETWORKS / "unbalanced_hierarchy.net",
    NETWORKS / "lossy_benchmark.net",
    NETWORKS / "lossy_benchmark_reciprocal.net",
    NETWORKS / "accumulate_a.net",
    NETWORKS / "teleport_directed.net",
    NETWORKS / "twotriangles_flow.net",
    # A state network: the flow models apply unchanged to the state nodes.
    NETWORKS / "states.net",
]

DIRECTED_VARIANTS = {
    "unrecorded-to-links": {},
    "unrecorded-to-nodes": {"to_nodes": True},
    "recorded-to-links": {"recorded_teleportation": True},
    "recorded-to-nodes": {"recorded_teleportation": True, "to_nodes": True},
}


def _id(path: Path) -> str:
    return path.name if "examples" in path.parts else f"fixture-{path.name}"


def _run(network, **options):
    options.setdefault("num_trials", 1)
    options.setdefault("seed", 7)
    return run(str(network) if isinstance(network, Path) else network, **options)


def _vertex_weights(path: Path) -> dict[int, float] | None:
    """Node weights from a Pajek ``*Vertices`` section, or None for a state network."""
    weights = {}
    section = None
    for line in path.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if line.startswith("*"):
            section = line.split()[0].lower()
            if section == "*states":
                return None
            continue
        if section == "*vertices" and line:
            parts = line.split('"')
            rest = parts[-1].split() if len(parts) > 1 else line.split()[1:]
            weights[int(line.split()[0])] = float(rest[0]) if rest else 1.0
    return weights


def _leaves(result):
    return {node.state_id: node for node in result.tree(states=True) if node.is_leaf}


def _reference(result, model, options, node_weights=None):
    return flow(
        _leaves(result),
        result.links("weight"),
        model,
        teleportation_probability=options.get("teleportation_probability", 0.15),
        recorded=options.get("recorded_teleportation", False),
        to_nodes=options.get("to_nodes", False),
        node_weights=node_weights,
    )


def _assert_flow(result, expected):
    leaves = _leaves(result)
    assert set(leaves) == set(expected.node)
    for node_id, value in expected.node.items():
        assert leaves[node_id].flow == pytest.approx(value, rel=REL, abs=ABS), node_id
    links = {(source, target): value for source, target, value in result.links("flow")}
    assert set(links) == set(expected.link)
    for link, value in expected.link.items():
        assert links[link] == pytest.approx(value, rel=REL, abs=ABS), link
    if expected.teleport_flow is not None:
        for node_id, value in expected.teleport_flow.items():
            data = leaves[node_id].data
            assert data.teleportFlow == pytest.approx(value, rel=REL, abs=ABS), node_id
            assert data.teleportWeight == pytest.approx(
                expected.teleport_weight[node_id], rel=REL, abs=ABS
            ), node_id


def _assert_leaf_enter_exit(result, expected, *, undirected, markov_time=1.0):
    enter, exit_ = leaf_enter_exit(
        expected, undirected=undirected, markov_time=markov_time
    )
    for node_id, node in _leaves(result).items():
        assert node.data.enterFlow == pytest.approx(enter[node_id], rel=REL, abs=ABS), (
            node_id
        )
        assert node.data.exitFlow == pytest.approx(exit_[node_id], rel=REL, abs=ABS), (
            node_id
        )


# --- the reference itself ----------------------------------------------------


def test_reference_does_not_import_infomap():
    source = (Path(__file__).parent / "reference_flow.py").read_text()
    imported = set()
    for node in ast.walk(ast.parse(source)):
        if isinstance(node, ast.Import):
            imported.update(alias.name.split(".")[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module:
            imported.add(node.module.split(".")[0])
    assert "infomap" not in imported


def test_reference_directed_by_hand():
    # 1 -> 2, node 2 dangling, uniform teleportation to nodes with alpha = 0.15.
    # p1 = 0.075 p1 + 0.5 p2 and p1 + p2 = 1, so p1 = 0.5 / 1.425.
    p1 = 0.5 / 1.425
    recorded = flow([1, 2], [(1, 2, 1.0)], "directed", recorded=True, to_nodes=True)
    assert recorded.node == pytest.approx({1: p1, 2: 1 - p1}, rel=1e-14)
    assert recorded.link == pytest.approx({(1, 2): 0.85 * p1}, rel=1e-14)
    assert recorded.teleport_flow == pytest.approx({1: 0.15 * p1, 2: 1 - p1}, rel=1e-14)
    assert recorded.teleport_weight == pytest.approx({1: 0.5, 2: 0.5})
    # Unrecorded, the only coded step is 1 -> 2, and node 1 has no in-links.
    unrecorded = flow([1, 2], [(1, 2, 1.0)], "directed", to_nodes=True)
    assert unrecorded.node == pytest.approx({1: 0.0, 2: 1.0}, abs=1e-15)
    assert unrecorded.link == pytest.approx({(1, 2): 1.0}, rel=1e-14)


def test_reference_unrecorded_flow_at_teleportation_probability_one():
    # alpha = 1 is accepted. The walker never takes a link step, so the
    # unrecorded flow is the limit alpha -> 1: one step from the teleport
    # distribution. A two-node cycle stays at 1/2 everywhere.
    cycle = flow(
        [1, 2],
        [(1, 2, 1.0), (2, 1, 1.0)],
        "directed",
        teleportation_probability=1.0,
    )
    assert cycle.node == pytest.approx({1: 0.5, 2: 0.5}, rel=1e-14)
    assert cycle.link == pytest.approx({(1, 2): 0.5, (2, 1): 0.5}, rel=1e-14)


def test_reference_undirected_flow_is_stationary():
    # The closed form p_a = s_a / sum(s) must be the stationary distribution of
    # the walk that leaves a node along each link end by weight.
    links = _generated_links(1, directed=False)
    nodes = sorted({n for link in links for n in link[:2]})
    index = {node: i for i, node in enumerate(nodes)}
    adjacency = np.zeros((len(nodes), len(nodes)))
    for source, target, weight in links:
        adjacency[index[source], index[target]] += weight
        if source != target:
            adjacency[index[target], index[source]] += weight
    walk = adjacency / adjacency.sum(axis=1, keepdims=True)
    p = np.array([flow(nodes, links).node[node] for node in nodes])
    assert p @ walk == pytest.approx(p, rel=1e-12)


def test_reference_unrecorded_teleportation_to_links_is_a_link_step():
    # Unrecorded teleportation to links lands at a link's source and is not
    # coded. Equivalently, the teleport picks a link by weight and the walker
    # traverses it, so every step is a link step (Lambiotte & Rosvall 2012).
    # Build that walk independently and compare its link-step rates.
    links, _ = _directed_network(2)
    nodes = sorted({n for link in links for n in link[:2]})
    index = {node: i for i, node in enumerate(nodes)}
    w = np.zeros((len(nodes), len(nodes)))
    for source, target, weight in links:
        w[index[source], index[target]] += weight
    out = w.sum(axis=1)
    dangling = out == 0
    step = np.divide(w, out[:, None], out=np.zeros_like(w), where=~dangling[:, None])
    alpha = 0.15
    in_weight = w.sum(axis=0) / w.sum()
    transition = (1 - alpha) * step + alpha * in_weight[None, :]
    transition[dangling] = in_weight
    q = stationary(transition)
    teleporting = alpha * q[~dangling].sum() + q[dangling].sum()
    rates = (1 - alpha) * q[:, None] * step + teleporting * w / w.sum()

    expected = flow(nodes, links, "directed")
    assert expected.node == pytest.approx(
        dict(zip(nodes, q, strict=True)), rel=1e-12, abs=1e-15
    )
    for (source, target), value in expected.link.items():
        assert value == pytest.approx(rates[index[source], index[target]], rel=1e-12)


def test_reference_teleport_targets():
    links = [(1, 2, 3.0), (2, 1, 1.0), (2, 3, 1.0)]
    net = _Network([1, 2, 3], links)
    assert teleport_targets(net, recorded=False, to_nodes=False) == pytest.approx(
        [0.6, 0.4, 0.0]
    )
    assert teleport_targets(net, recorded=True, to_nodes=False) == pytest.approx(
        [0.2, 0.6, 0.2]
    )
    assert teleport_targets(
        net, recorded=True, to_nodes=True, node_weights={3: 2.0}
    ) == pytest.approx([0.25, 0.25, 0.5])


# --- fixtures ----------------------------------------------------------------


@pytest.mark.parametrize("model", ["undirected", "undirdir", "outdirdir", "rawdir"])
@pytest.mark.parametrize("network", NETWORK_FILES, ids=_id)
def test_link_weight_models(network, model):
    options = {"flow_model": model}
    result = _run(network, **options)
    expected = _reference(result, model, options)
    _assert_flow(result, expected)
    _assert_leaf_enter_exit(result, expected, undirected=model == "undirected")


@pytest.mark.parametrize("alpha", [0.15, 0.4])
@pytest.mark.parametrize("variant", DIRECTED_VARIANTS)
@pytest.mark.parametrize("network", NETWORK_FILES, ids=_id)
def test_directed(network, variant, alpha):
    options = {
        "flow_model": "directed",
        "teleportation_probability": alpha,
        **DIRECTED_VARIANTS[variant],
    }
    node_weights = _vertex_weights(network)
    if options.get("to_nodes") and node_weights is None:
        pytest.skip("teleportation to state nodes takes no weights from the file")
    result = _run(network, **options)
    expected = _reference(result, "directed", options, node_weights)
    _assert_flow(result, expected)
    _assert_leaf_enter_exit(result, expected, undirected=False)


@pytest.mark.parametrize("markov_time", [0.5, 2.0, 3.0])
@pytest.mark.parametrize(
    ("model", "variant"),
    [
        ("undirected", None),
        ("rawdir", None),
        ("directed", "unrecorded-to-links"),
        ("directed", "recorded-to-nodes"),
    ],
    ids=["undirected", "rawdir", "directed", "directed-recorded"],
)
@pytest.mark.parametrize(
    "network", [EXAMPLES / "ninetriangles.net", EXAMPLES / "modular_wd.net"], ids=_id
)
def test_markov_time(network, model, variant, markov_time):
    # Markov time leaves the flow alone and scales only the link part of the
    # enter and exit flow.
    options = {
        "flow_model": model,
        "markov_time": markov_time,
        **DIRECTED_VARIANTS.get(variant, {}),
    }
    result = _run(network, **options)
    expected = _reference(result, model, options, _vertex_weights(network))
    _assert_flow(result, expected)
    _assert_leaf_enter_exit(
        result, expected, undirected=model == "undirected", markov_time=markov_time
    )


# --- generated networks ------------------------------------------------------


def _generated_links(seed, *, directed):
    """Three weighted groups of six with a self-link, and a separate triangle."""
    rng = np.random.default_rng(seed)
    groups = np.repeat(np.arange(3), 6)
    links = []
    for u in range(18):
        for v in range(18) if directed else range(u + 1, 18):
            if u != v and rng.random() < (0.5 if groups[u] == groups[v] else 0.05):
                links.append((u + 1, v + 1, float(rng.uniform(0.5, 2.0))))
    links.append((5, 5, 1.5))
    links += [(20, 21, 1.0), (21, 22, 2.0), (22, 20, 1.0)]
    return links


def _directed_network(seed, *, self_link_only=False):
    """A directed network with every kind of node the directed model treats specially.

    Nodes 6 and 12 are dangling sinks, 13 has out-links only, 23 is a dangling
    node in a separate component, and 24 and 25 are isolated. With
    ``self_link_only`` node 3's only out-link is a self-link. Node weights are
    random, for teleportation to nodes.
    """
    rng = np.random.default_rng(seed + 100)
    links = [
        link
        for link in _generated_links(seed, directed=True)
        if link[0] not in (6, 12) and link[1] != 13
    ]
    links.append((22, 23, 0.5))
    if self_link_only:
        links = [link for link in links if link[0] != 3] + [(3, 3, 1.5)]
    node_weights = {node: float(rng.uniform(0.5, 3.0)) for node in range(1, 26)}
    return links, node_weights


def _write_pajek(path, links, node_weights):
    lines = [f"*Vertices {len(node_weights)}"]
    lines += [f'{node} "{node}" {weight!r}' for node, weight in node_weights.items()]
    lines.append("*Arcs")
    lines += [f"{source} {target} {weight!r}" for source, target, weight in links]
    path.write_text("\n".join(lines) + "\n")
    return path


@pytest.mark.parametrize("seed", [1, 2, 3, 4])
@pytest.mark.parametrize("model", ["undirected", "undirdir", "outdirdir", "rawdir"])
def test_generated_link_weight_models(tmp_path, seed, model):
    links, node_weights = _directed_network(seed, self_link_only=True)
    path = _write_pajek(tmp_path / "net.net", links, node_weights)
    options = {"flow_model": model}
    result = _run(path, **options)
    expected = _reference(result, model, options)
    _assert_flow(result, expected)
    _assert_leaf_enter_exit(result, expected, undirected=model == "undirected")


@pytest.mark.parametrize("seed", [1, 2, 3, 4])
@pytest.mark.parametrize("alpha", [0.15, 0.4])
@pytest.mark.parametrize("variant", DIRECTED_VARIANTS)
def test_generated_directed(tmp_path, variant, alpha, seed):
    links, node_weights = _directed_network(seed)
    path = _write_pajek(tmp_path / "net.net", links, node_weights)
    options = {
        "flow_model": "directed",
        "teleportation_probability": alpha,
        **DIRECTED_VARIANTS[variant],
    }
    result = _run(path, **options)
    expected = _reference(result, "directed", options, node_weights)
    assert sorted(result.links("weight")) == sorted(links)
    _assert_flow(result, expected)
    _assert_leaf_enter_exit(result, expected, undirected=False)


@pytest.mark.parametrize("seed", [1, 2])
@pytest.mark.parametrize("variant", ["unrecorded-to-links", "unrecorded-to-nodes"])
def test_generated_unrecorded_at_teleportation_probability_one(tmp_path, variant, seed):
    links, node_weights = _directed_network(seed)
    path = _write_pajek(tmp_path / "net.net", links, node_weights)
    options = {
        "flow_model": "directed",
        "teleportation_probability": 1.0,
        **DIRECTED_VARIANTS[variant],
    }
    result = _run(path, **options)
    expected = _reference(result, "directed", options, node_weights)
    _assert_flow(result, expected)


@pytest.mark.parametrize("seed", [1, 2])
@pytest.mark.parametrize("variant", DIRECTED_VARIANTS)
def test_generated_directed_flow_with_self_link_only_node(tmp_path, variant, seed):
    # The flow itself treats a self-link as an out-link; see the xfail below
    # for what the optimizer does with such a node.
    links, node_weights = _directed_network(seed, self_link_only=True)
    path = _write_pajek(tmp_path / "net.net", links, node_weights)
    options = {"flow_model": "directed", **DIRECTED_VARIANTS[variant]}
    result = _run(path, **options)
    expected = _reference(result, "directed", options, node_weights)
    links_reported = {(s, t): v for s, t, v in result.links("flow")}
    assert set(links_reported) == set(expected.link)
    for node_id, node in _leaves(result).items():
        assert node.flow == pytest.approx(expected.node[node_id], rel=REL, abs=ABS)
    for link, value in expected.link.items():
        assert links_reported[link] == pytest.approx(value, rel=REL, abs=ABS)


# --- bipartite networks ------------------------------------------------------

BIPARTITE_FILES = [EXAMPLES / "bipartite.net", NETWORKS / "bipartite_uneven.net"]


def _bipartite_start(path: Path) -> int:
    for line in path.read_text().splitlines():
        if line.lower().startswith("*bipartite"):
            return int(line.split()[1])
    raise ValueError(f"{path} has no *Bipartite section")


def _bipartite_network(tmp_path, seed, *, feature_links=False):
    """Ten primary nodes (1-10, node 10 isolated) and six features (11-16), weighted.

    With ``feature_links`` some features link back to primary nodes, so the
    directed model has features that are not dangling.
    """
    rng = np.random.default_rng(seed)
    links = [
        (u, f, float(rng.uniform(0.5, 2.0)))
        for u in range(1, 10)
        for f in range(11, 17)
        if rng.random() < (0.6 if (u - 1) // 3 == (f - 11) // 2 else 0.1)
    ]
    if feature_links:
        links += [(11, 4, 1.0), (14, 1, 0.5), (16, 2, 2.0)]
    lines = ["*Vertices 16"] + [f'{n} "{n}"' for n in range(1, 17)]
    lines += ["*Bipartite 11"] + [f"{s} {t} {w!r}" for s, t, w in links]
    path = tmp_path / "bipartite.net"
    path.write_text("\n".join(lines) + "\n")
    return path


def _bipartite_cases(tmp_path, network):
    if isinstance(network, Path):
        return network, _bipartite_start(network)
    return _bipartite_network(tmp_path, network), 11


@pytest.mark.parametrize(
    "network",
    [*BIPARTITE_FILES, 1, 2, 3],
    ids=lambda n: _id(n) if isinstance(n, Path) else f"generated-{n}",
)
def test_bipartite_undirected(tmp_path, network):
    path, start = _bipartite_cases(tmp_path, network)
    result = _run(path)
    leaves = _leaves(result)
    full = flow(leaves, result.links("weight"))
    expected = bipartite_projection(full, [n for n in leaves if n >= start])
    _assert_flow(result, expected)


@pytest.mark.parametrize(
    ("model", "variant"),
    [("undirected", None), *(("directed", v) for v in DIRECTED_VARIANTS)],
    ids=["undirected", *DIRECTED_VARIANTS],
)
@pytest.mark.parametrize("feature_links", [False, True], ids=["one-way", "two-way"])
@pytest.mark.parametrize("seed", [1, 2])
def test_bipartite_skip_adjust_is_the_plain_flow(
    tmp_path, seed, feature_links, model, variant
):
    # Declaring the network bipartite and opting out of the projection must
    # leave the flow exactly as without the declaration (#945).
    path = _bipartite_network(tmp_path, seed, feature_links=feature_links)
    options = {
        "flow_model": model,
        "skip_adjust_bipartite_flow": True,
        **DIRECTED_VARIANTS.get(variant, {}),
    }
    result = _run(path, **options)
    _assert_flow(result, _reference(result, model, options))


# --- findings ----------------------------------------------------------------


@pytest.mark.xfail(
    reason="recorded teleportation: a node whose only out-link is a self-link gets the flow "
    "of a non-dangling node, but the optimizer drops self-links from the tree, takes the "
    "node for dangling and gives it teleport flow p instead of alpha * p, so the reported "
    "codelength is too high (#1182)",
    strict=True,
)
@pytest.mark.parametrize("to_nodes", [False, True], ids=["to-links", "to-nodes"])
@pytest.mark.parametrize("seed", [1, 2])
def test_recorded_teleportation_with_self_link_only_node(tmp_path, seed, to_nodes):
    links, node_weights = _directed_network(seed, self_link_only=True)
    path = _write_pajek(tmp_path / "net.net", links, node_weights)
    options = {
        "flow_model": "directed",
        "recorded_teleportation": True,
        "to_nodes": to_nodes,
        "two_level": True,
    }
    result = _run(path, **options)
    expected = _reference(result, "directed", options, node_weights)
    leaves = {
        node_id: Leaf(
            path=tuple(node.path),
            flow=expected.node[node_id],
            teleport_flow=expected.teleport_flow[node_id],
            teleport_weight=expected.teleport_weight[node_id],
        )
        for node_id, node in _leaves(result).items()
    }
    partition = Partition(
        leaves=leaves,
        links=[(s, t, v) for (s, t), v in expected.link.items()],
        recorded_teleportation=True,
    )
    assert result.codelength == pytest.approx(
        codelength(partition).total, rel=REL, abs=ABS
    )
    _assert_flow(result, expected)


def test_api_node_weights_reach_teleportation():
    # Node weights set with add_node used to be reset to 1 before the flow (#1183).
    links, node_weights = _directed_network(1)
    net = Network()
    for node_id, weight in node_weights.items():
        net.add_node(node_id, teleportation_weight=weight)
    net.add_links(links)
    options = {
        "flow_model": "directed",
        "recorded_teleportation": True,
        "to_nodes": True,
    }
    result = _run(net, **options)
    _assert_flow(result, _reference(result, "directed", options, node_weights))
