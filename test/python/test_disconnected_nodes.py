from __future__ import annotations

import networkx as nx
import pytest


pytestmark = pytest.mark.fast


_ISOLATED_IDS = (100, 101, 102, 103, 104)
_SBM_SIZES = (20, 20, 20)
_SBM_PROBS = [
    [0.85, 0.02, 0.02],
    [0.02, 0.85, 0.02],
    [0.02, 0.02, 0.85],
]
_SBM_NODE_COUNT = sum(_SBM_SIZES)
_SBM_ISOLATED_IDS = tuple(range(1000, 1005))


def _community_with_isolates() -> nx.Graph:
    g = nx.Graph()
    g.add_edges_from([(1, 2), (1, 3), (2, 3), (3, 4), (4, 5), (4, 6), (5, 6)])
    for n in _ISOLATED_IDS:
        g.add_node(n)
    return g


def _sbm() -> nx.Graph:
    return nx.stochastic_block_model(list(_SBM_SIZES), _SBM_PROBS, seed=123)


def _expected_sbm_partition(g: nx.Graph) -> list[list[int]]:
    return sorted(sorted(block) for block in g.graph["partition"])


def test_isolated_nodes_remain_singletons_without_regularized(make_infomap):
    im = make_infomap()
    im.add_networkx_graph(_community_with_isolates())
    im.run()

    modules = im.get_modules()
    isolated_module_ids = {modules[n] for n in _ISOLATED_IDS}

    assert len(isolated_module_ids) == len(_ISOLATED_IDS)
    assert im.num_top_modules >= 1 + len(_ISOLATED_IDS)


def test_isolated_nodes_merge_with_regularized(make_infomap):
    im = make_infomap(regularized=True)
    im.add_networkx_graph(_community_with_isolates())
    im.run()

    modules = im.get_modules()
    isolated_module_ids = {modules[n] for n in _ISOLATED_IDS}

    assert len(isolated_module_ids) < len(_ISOLATED_IDS)
    assert len(isolated_module_ids) <= 2
    assert im.num_top_modules < 1 + len(_ISOLATED_IDS)


@pytest.mark.parametrize("seed", [1, 7, 42, 123, 2026])
def test_isolated_nodes_merge_robust_across_seeds(make_infomap, seed):
    im = make_infomap(seed=seed, regularized=True)
    im.add_networkx_graph(_community_with_isolates())
    im.run()

    modules = im.get_modules()
    isolated_module_ids = {modules[n] for n in _ISOLATED_IDS}

    assert len(isolated_module_ids) < len(_ISOLATED_IDS)


def test_regularized_recovers_planted_partition(make_infomap, canonical_modules):
    g = _sbm()
    expected = _expected_sbm_partition(g)

    im = make_infomap(regularized=True)
    im.add_networkx_graph(g)
    im.run()

    assert im.num_top_modules == len(_SBM_SIZES)
    assert canonical_modules(im.get_modules()) == expected


@pytest.mark.parametrize("regularized", [False, True])
def test_regularized_matches_baseline_on_easy_partition(make_infomap, canonical_modules, regularized):
    g = _sbm()
    expected = _expected_sbm_partition(g)

    im = make_infomap(regularized=regularized)
    im.add_networkx_graph(g)
    im.run()

    assert im.num_top_modules == len(_SBM_SIZES)
    assert canonical_modules(im.get_modules()) == expected


def test_regularized_with_isolated_nodes_in_sbm(make_infomap):
    g = _sbm()
    for n in _SBM_ISOLATED_IDS:
        g.add_node(n)

    im = make_infomap(regularized=True)
    im.add_networkx_graph(g)
    im.run()

    modules = im.get_modules()
    sbm_module_ids = {modules[n] for n in range(_SBM_NODE_COUNT)}
    isolated_module_ids = {modules[n] for n in _SBM_ISOLATED_IDS}

    assert len(sbm_module_ids) == len(_SBM_SIZES)
    assert len(isolated_module_ids) < len(_SBM_ISOLATED_IDS)
