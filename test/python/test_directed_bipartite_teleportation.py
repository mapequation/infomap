"""Directed bipartite flow with ``--bipartite-teleportation`` against a hand model.

``reference_flow`` leaves this model out, so the expectation is built here from
the documented walk (``flow-models/bipartite.md``): each move A -> B -> A counts
as one step between primary (type-A) nodes, and teleportation only lands on
primary nodes. With ``alpha`` the teleportation probability and
``beta = 1 - alpha``, the walker at a primary node ``a`` follows an out-link to
a feature ``f`` and then one of ``f``'s out-links back to a primary node ``b``,
each picked by weight, with probability ``beta``, and teleports with
probability ``alpha``. A dangling primary node teleports with probability 1, and so does a walker
that reaches a feature without links back.
A teleport lands on primary node ``b`` with weight ``v_b``: its node weight
with ``--to-nodes``, otherwise its out-weight (unrecorded) or its in-weight
from features (recorded), as for the unipartite directed model.

``p`` is the stationary distribution of that walk. Recorded teleportation codes
every step, so the node flow is ``p``. Unrecorded teleportation codes the link
steps, so the flow of ``b`` is the two-step flow into it,
``sum_a p_a P(a -> b)``, normalised to sum 1. Features get flow 0.

Each half of a two-step move carries the flow of the move: ``a -> f`` gets
``beta p_a w_af / out_a`` (recorded) and ``f -> b`` gets that flow arriving at
``f`` split by ``w_fb / out_f``. Unrecorded teleportation leaves out ``beta``
and normalises by ``1 - (dangling flow)``. Both are doubled, Markov time 2 on
the full network being Markov time 1 between primary nodes (#1224, item 2).
"""

from __future__ import annotations

import numpy as np
import pytest
from infomap import run

pytestmark = pytest.mark.fast

REL = 1e-11
ABS = 1e-12
ALPHA = 0.15

VARIANTS = {
    "unrecorded-to-links": {},
    "unrecorded-to-nodes": {"to_nodes": True},
    "recorded-to-links": {"recorded_teleportation": True},
    "recorded-to-nodes": {"recorded_teleportation": True, "to_nodes": True},
}
RECORDED = [v for v in VARIANTS if v.startswith("recorded")]
UNRECORDED = [v for v in VARIANTS if v.startswith("unrecorded")]

# Primary nodes 1-3, features 4-5, every feature links back.
TWO_WAY = (
    4,
    [1, 2, 3, 4, 5],
    [
        (1, 4, 1.0),
        (2, 4, 2.0),
        (2, 5, 1.0),
        (3, 5, 1.0),
        (4, 1, 1.0),
        (4, 2, 1.0),
        (5, 3, 2.0),
        (5, 2, 1.0),
    ],
)
# Primary node 4 has an in-link from feature 6 and no out-links.
DANGLING_PRIMARY = (
    5,
    [1, 2, 3, 4, 5, 6],
    [
        (1, 5, 1.0),
        (2, 5, 2.0),
        (2, 6, 1.0),
        (3, 6, 1.0),
        (5, 1, 1.0),
        (5, 2, 1.0),
        (6, 3, 2.0),
        (6, 2, 1.0),
        (6, 4, 1.0),
    ],
)
# Feature 6 has an in-link from primary node 3 and no links back.
DANGLING_FEATURE = (
    4,
    [1, 2, 3, 4, 5, 6],
    [
        (1, 4, 1.0),
        (2, 4, 2.0),
        (2, 5, 1.0),
        (3, 5, 1.0),
        (3, 6, 1.0),
        (4, 1, 1.0),
        (4, 2, 1.0),
        (5, 3, 2.0),
        (5, 2, 1.0),
    ],
)
NODE_WEIGHTS = {1: 1.0, 2: 2.0, 3: 3.0, 4: 1.5}


def _write(path, network):
    start, nodes, links = network
    lines = [f"*Vertices {len(nodes)}"]
    lines += [
        f'{n} "{n}" {NODE_WEIGHTS.get(n, 1.0) if n < start else 1.0}' for n in nodes
    ]
    lines += [f"*Bipartite {start}"] + [f"{s} {t} {w!r}" for s, t, w in links]
    path.write_text("\n".join(lines) + "\n")
    return path


def _expected(network, *, recorded=False, to_nodes=False):
    """Node and link flow of the walk in the module docstring."""
    start, nodes, links = network
    primary = [n for n in nodes if n < start]
    features = [n for n in nodes if n >= start]
    index = {n: i for i, n in enumerate(nodes)}
    w = np.zeros((len(nodes), len(nodes)))
    for s, t, weight in links:
        w[index[s], index[t]] += weight
    out = w.sum(axis=1)
    pi = [index[n] for n in primary]
    fi = [index[n] for n in features]

    if to_nodes:
        v = np.array([NODE_WEIGHTS.get(n, 1.0) for n in primary])
    elif recorded:
        v = w[np.ix_(fi, pi)].sum(axis=0)
    else:
        v = w[np.ix_(pi, fi)].sum(axis=1)
    v = v / v.sum()

    # step[a, b] = P(a -> f -> b) between primary nodes
    to_feature = w[np.ix_(pi, fi)] / np.where(out[pi] > 0, out[pi], 1)[:, None]
    to_primary = w[np.ix_(fi, pi)] / np.where(out[fi] > 0, out[fi], 1)[:, None]
    step = to_feature @ to_primary
    dangling = out[pi] == 0
    # A walker that reaches a feature without links back is at a dangling node
    # and teleports, as from a dangling primary node.
    stuck = np.where(dangling, 0.0, 1.0 - step.sum(axis=1))

    transition = (1 - ALPHA) * (step + stuck[:, None] * v[None, :])
    transition += ALPHA * v[None, :]
    transition[dangling] = v
    k = len(primary)
    system = transition.T - np.eye(k)
    system[-1, :] = 1.0
    rhs = np.zeros(k)
    rhs[-1] = 1.0
    p = np.linalg.solve(system, rhs)

    if recorded:
        scale = 1 - ALPHA
        node = p
    else:
        scale = 1 / (1 - p[dangling].sum())
        node = p @ step
        node = node / node.sum()
    feature_flow = scale * (p @ to_feature)

    link = {}
    for s, t, _ in links:
        if s < start:
            i = primary.index(s)
            value = scale * p[i] * to_feature[i, features.index(t)]
        else:
            j = features.index(s)
            value = feature_flow[j] * to_primary[j, primary.index(t)]
        link[(s, t)] = 2 * value
    node_flow = {n: float(node[i]) for i, n in enumerate(primary)}
    node_flow.update(dict.fromkeys(features, 0.0))
    return node_flow, link


def _run(tmp_path, network, variant):
    path = _write(tmp_path / "bipartite.net", network)
    return run(
        str(path),
        flow_model="directed",
        bipartite_teleportation=True,
        num_trials=1,
        seed=7,
        **VARIANTS[variant],
    )


def _node_flow(result):
    return {node.node_id: node.flow for node in result.tree() if node.is_leaf}


def _assert_node_flow(result, expected):
    got = _node_flow(result)
    assert set(got) == set(expected)
    for node_id, value in expected.items():
        assert got[node_id] == pytest.approx(value, rel=REL, abs=ABS), node_id


def _assert_link_flow(result, expected, *, primary_side_only=False):
    got = {(s, t): value for s, t, value in result.links("flow")}
    assert set(got) == set(expected)
    start = min(t for (s, t) in expected if s < t)
    for link, value in expected.items():
        if primary_side_only and link[0] >= start:
            continue
        assert got[link] == pytest.approx(value, rel=REL, abs=ABS), link


@pytest.mark.parametrize("variant", VARIANTS)
def test_two_way_node_flow(tmp_path, variant):
    options = VARIANTS[variant]
    result = _run(tmp_path, TWO_WAY, variant)
    node, _ = _expected(
        TWO_WAY,
        recorded=options.get("recorded_teleportation", False),
        to_nodes=options.get("to_nodes", False),
    )
    _assert_node_flow(result, node)


@pytest.mark.parametrize("variant", UNRECORDED)
def test_two_way_link_flow_unrecorded(tmp_path, variant):
    result = _run(tmp_path, TWO_WAY, variant)
    _, link = _expected(TWO_WAY, to_nodes=VARIANTS[variant].get("to_nodes", False))
    _assert_link_flow(result, link)


@pytest.mark.parametrize("variant", RECORDED)
def test_two_way_link_flow_recorded_primary_side(tmp_path, variant):
    result = _run(tmp_path, TWO_WAY, variant)
    _, link = _expected(
        TWO_WAY, recorded=True, to_nodes=VARIANTS[variant].get("to_nodes", False)
    )
    _assert_link_flow(result, link, primary_side_only=True)


@pytest.mark.parametrize("variant", RECORDED)
def test_two_way_link_flow_recorded(tmp_path, variant):
    result = _run(tmp_path, TWO_WAY, variant)
    _, link = _expected(
        TWO_WAY, recorded=True, to_nodes=VARIANTS[variant].get("to_nodes", False)
    )
    _assert_link_flow(result, link)


@pytest.mark.parametrize("variant", RECORDED)
def test_dangling_primary_node_flow_recorded(tmp_path, variant):
    result = _run(tmp_path, DANGLING_PRIMARY, variant)
    node, _ = _expected(
        DANGLING_PRIMARY,
        recorded=True,
        to_nodes=VARIANTS[variant].get("to_nodes", False),
    )
    _assert_node_flow(result, node)


@pytest.mark.parametrize("variant", UNRECORDED)
def test_dangling_primary_node_flow_unrecorded(tmp_path, variant):
    result = _run(tmp_path, DANGLING_PRIMARY, variant)
    node, _ = _expected(
        DANGLING_PRIMARY, to_nodes=VARIANTS[variant].get("to_nodes", False)
    )
    assert sum(_node_flow(result).values()) == pytest.approx(1.0, rel=REL)
    _assert_node_flow(result, node)


@pytest.mark.parametrize("variant", UNRECORDED)
def test_dangling_primary_link_flow_unrecorded(tmp_path, variant):
    result = _run(tmp_path, DANGLING_PRIMARY, variant)
    _, link = _expected(
        DANGLING_PRIMARY, to_nodes=VARIANTS[variant].get("to_nodes", False)
    )
    _assert_link_flow(result, link)


@pytest.mark.xfail(
    strict=True,
    reason="#1311: flow into a feature without links back is "
    "dropped and the rest renormalised instead of teleported",
)
@pytest.mark.parametrize("variant", VARIANTS)
def test_dangling_feature_node_flow(tmp_path, variant):
    options = VARIANTS[variant]
    result = _run(tmp_path, DANGLING_FEATURE, variant)
    node, _ = _expected(
        DANGLING_FEATURE,
        recorded=options.get("recorded_teleportation", False),
        to_nodes=options.get("to_nodes", False),
    )
    _assert_node_flow(result, node)
