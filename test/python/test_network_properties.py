"""Properties every run must have, on generated networks of every kind (#1252).

``network_strategies`` draws small plain, bipartite, multilayer, state and
meta-data networks. On each, a run with default settings, a drawn seed, one to
three trials and two-level or multilevel search must

- not crash,
- return a finite codelength,
- code no worse than the one-level codelength, the trivial partition every
  search starts from,
- assign every node of the network to a module, and
- write a tree that, read back with ``--cluster-data`` and ``--no-infomap``,
  gives the same modules and the same codelength.

The tree of a state or multilayer network is written at the state level. A
physical-level tree cannot say which state of a physical node went to which
module when they are split (#908), so it does not round-trip in general.

The default run draws a few dozen examples per kind, derandomized so a PR
cannot turn red on a draw that has nothing to do with it. The slow variant
draws more, from a random seed each run.
"""

from __future__ import annotations

import math
import os
import tempfile
from pathlib import Path

import pytest
from hypothesis import HealthCheck, given, settings
from hypothesis import strategies as st
from infomap import run
from network_strategies import KINDS, GeneratedNetwork

THREADS = {"num_threads": int(os.environ.get("INFOMAP_NUM_THREADS", "1"))}

TOLERANCE = 1e-10

FAST = settings(
    max_examples=100,
    derandomize=True,
    database=None,
    deadline=None,
    suppress_health_check=[HealthCheck.too_slow],
)
SLOW = settings(
    FAST,
    max_examples=1000,
    derandomize=False,
)

searches = st.fixed_dictionaries(
    {
        "seed": st.integers(1, 10_000),
        "num_trials": st.integers(1, 3),
        "two_level": st.booleans(),
    }
)


def _modules(result, states: bool) -> set[frozenset[int]]:
    """Every module of the tree, at every level, as the set of its leaf ids."""
    members: dict[tuple[int, ...], set[int]] = {}
    for node in result.tree(states=states):
        if node.is_leaf:
            leaf = node.state_id if states else node.node_id
            path = tuple(node.path)
            for depth in range(1, len(path)):
                members.setdefault(path[:depth], set()).add(leaf)
    return {frozenset(m) for m in members.values()}


def check_properties(net: GeneratedNetwork, search: dict) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        directory = Path(tmp)
        path, options = net.write(directory)
        result = run(str(path), silent=True, **search, **options, **THREADS)

        assert math.isfinite(result.codelength)
        assert math.isfinite(result.one_level_codelength)
        # With meta data, a search that ends in singleton modules can stay
        # above the one-level codelength (#1320).
        if net.meta is None or result.num_non_trivial_top_modules > 0:
            assert result.codelength <= result.one_level_codelength + TOLERANCE

        leaves = [node for node in result.tree() if node.is_leaf]
        assert {node.node_id for node in leaves} == net.nodes
        assert all(node.path for node in leaves)
        if net.kind == "state":
            states = {n.state_id for n in result.tree(states=True) if n.is_leaf}
            assert states == set(range(1, net.state_nodes + 1))

        tree = directory / "result.tree"
        result.write_tree(tree, states=net.has_states)
        scored = run(
            str(path),
            silent=True,
            cluster_data=str(tree),
            no_infomap=True,
            **options,
            **THREADS,
        )
        assert scored.codelength == pytest.approx(
            result.codelength, rel=TOLERANCE, abs=TOLERANCE
        )
        # A lone leaf is written at the root and read back into a module (#1316).
        written = [n for n in result.tree(states=net.has_states) if n.is_leaf]
        if len(written) > 1:
            assert _modules(scored, net.has_states) == _modules(result, net.has_states)


@pytest.mark.fast
@pytest.mark.parametrize("kind", KINDS)
def test_properties(kind):
    @FAST
    @given(net=KINDS[kind](), search=searches)
    def check(net, search):
        check_properties(net, search)

    check()


@pytest.mark.slow
@pytest.mark.parametrize("kind", KINDS)
def test_properties_sweep(kind):
    @SLOW
    @given(net=KINDS[kind](), search=searches)
    def check(net, search):
        check_properties(net, search)

    check()


# Examples the properties fail on, pinned until the fix lands. Each is strict,
# so the fix turns it red and the carve-out above goes with it.

ONE_NODE = GeneratedNetwork("plain", '*Vertices 1\n1 "n1"\n*Edges\n', frozenset({1}))

META_SINGLETONS = GeneratedNetwork(
    "plain",
    '*Vertices 4\n1 "n1"\n2 "n2"\n3 "n3"\n4 "n4"\n*Links\n'
    "1 1 1\n1 2 1\n1 3 1\n3 3 1\n3 4 1\n1 4 1\n"
    "2 1 1\n2 2 1\n2 3 1\n4 2 1\n4 3 1\n2 4 1\n",
    frozenset({1, 2, 3, 4}),
    directed=True,
    meta={1: 2, 2: 1, 3: 1, 4: 2},
)


@pytest.mark.fast
@pytest.mark.xfail(reason="#1316", strict=True)
def test_one_node_tree_reads_back_with_the_same_modules(tmp_path):
    path, _ = ONE_NODE.write(tmp_path)
    result = run(str(path), silent=True, **THREADS)
    tree = tmp_path / "result.tree"
    result.write_tree(tree)
    scored = run(
        str(path), silent=True, cluster_data=str(tree), no_infomap=True, **THREADS
    )
    assert _modules(scored, False) == _modules(result, False)


@pytest.mark.fast
@pytest.mark.xfail(reason="#1320", strict=True)
def test_meta_singletons_code_no_worse_than_one_level(tmp_path):
    path, options = META_SINGLETONS.write(tmp_path)
    result = run(str(path), silent=True, seed=1, **options, **THREADS)
    assert result.codelength <= result.one_level_codelength + TOLERANCE
