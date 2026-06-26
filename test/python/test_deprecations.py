"""Deprecation coverage for Phase 7 (non-breaking).

Two additive deprecations:

1. ``Infomap.from_*`` graph constructors emit ``DeprecationWarning`` while
   still returning a working instance identical to the non-deprecated path.
2. The ``depth_level`` parameter on the results accessors is deprecated in
   favour of the canonical ``depth`` (``level`` for ``to_dataframe``); the
   old name keeps working and emits ``DeprecationWarning``.
"""

import warnings

import numpy as np
import pytest

from infomap import Infomap


# ---------------------------------------------------------------------------
# 1. Infomap.from_* graph constructors are deprecated
# ---------------------------------------------------------------------------


@pytest.mark.fast
def test_from_networkx_deprecated_but_equivalent():
    nx = pytest.importorskip("networkx")
    G = nx.Graph([("a", "b"), ("a", "c"), ("b", "c")])

    baseline = Infomap(silent=True, no_file_output=True, seed=42)
    baseline.add_networkx_graph(G)
    baseline.run()

    with pytest.warns(DeprecationWarning, match="from_networkx is deprecated"):
        im = Infomap.from_networkx(G, silent=True, no_file_output=True, seed=42)
    im.run()

    assert im.codelength == pytest.approx(baseline.codelength)
    assert im.node_id_to_label == baseline.node_id_to_label == {0: "a", 1: "b", 2: "c"}


@pytest.mark.fast
def test_from_igraph_deprecated_but_equivalent():
    ig = pytest.importorskip("igraph")
    g = ig.Graph.Formula("a-b, a-c, b-c")

    baseline = Infomap(silent=True, no_file_output=True, seed=42)
    baseline.add_igraph_graph(g)
    baseline.run()

    with pytest.warns(DeprecationWarning, match="from_igraph is deprecated"):
        im = Infomap.from_igraph(g, silent=True, no_file_output=True, seed=42)
    im.run()

    assert im.codelength == pytest.approx(baseline.codelength)
    assert im.node_id_to_label == baseline.node_id_to_label


@pytest.mark.fast
def test_from_scipy_sparse_matrix_deprecated_but_equivalent():
    sparse = pytest.importorskip("scipy.sparse")
    A = sparse.csr_matrix(np.array([[0, 1, 0], [1, 0, 1], [0, 1, 0]]))

    baseline = Infomap(silent=True, no_file_output=True, seed=42)
    baseline.add_scipy_sparse_matrix(A)
    baseline.run()

    with pytest.warns(
        DeprecationWarning, match="from_scipy_sparse_matrix is deprecated"
    ):
        im = Infomap.from_scipy_sparse_matrix(A, silent=True, no_file_output=True, seed=42)
    im.run()

    assert im.codelength == pytest.approx(baseline.codelength)
    assert im.node_id_to_label == baseline.node_id_to_label


@pytest.mark.fast
def test_from_edge_index_deprecated_but_equivalent():
    edge_index = [[0, 1, 2], [1, 2, 0]]

    baseline = Infomap(silent=True, no_file_output=True, seed=42)
    baseline.add_edge_index(edge_index)
    baseline.run()

    with pytest.warns(DeprecationWarning, match="from_edge_index is deprecated"):
        im = Infomap.from_edge_index(
            edge_index, silent=True, no_file_output=True, seed=42
        )
    im.run()

    assert im.codelength == pytest.approx(baseline.codelength)
    assert im.node_id_to_label == baseline.node_id_to_label


# ---------------------------------------------------------------------------
# 2. depth_level -> depth on the results accessors
# ---------------------------------------------------------------------------


@pytest.fixture
def ran_infomap(make_infomap, example_network_path):
    im = make_infomap(num_trials=5)
    im.read_file(str(example_network_path("ninetriangles.net")))
    im.run()
    return im


@pytest.mark.fast
def test_get_modules_depth_level_deprecated_matches_depth(ran_infomap):
    with pytest.warns(DeprecationWarning, match="depth_level is deprecated; use depth"):
        legacy = ran_infomap.get_modules(depth_level=2)
    assert legacy == ran_infomap.get_modules(depth=2)


@pytest.mark.fast
def test_get_modules_default_does_not_warn(ran_infomap):
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        default = ran_infomap.get_modules()
        explicit = ran_infomap.get_modules(depth=1)
    assert default == explicit


@pytest.mark.fast
def test_get_modules_positional_compatibility(ran_infomap):
    # get_modules(1, True) must still mean depth=1, states=True.
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        positional = ran_infomap.get_modules(1, True)
    assert positional == ran_infomap.get_modules(depth=1, states=True)


@pytest.mark.fast
def test_get_tree_depth_level_deprecated_matches_depth(ran_infomap):
    with pytest.warns(DeprecationWarning, match="depth_level is deprecated; use depth"):
        legacy = [node.module_id for node in ran_infomap.get_tree(depth_level=2)]
    assert legacy == [node.module_id for node in ran_infomap.get_tree(depth=2)]


@pytest.mark.fast
def test_get_nodes_depth_level_deprecated_matches_depth(ran_infomap):
    with pytest.warns(DeprecationWarning, match="depth_level is deprecated; use depth"):
        legacy = [node.node_id for node in ran_infomap.get_nodes(depth_level=2)]
    assert legacy == [node.node_id for node in ran_infomap.get_nodes(depth=2)]


@pytest.mark.fast
def test_get_effective_num_modules_depth_level_deprecated_matches_depth(ran_infomap):
    with pytest.warns(DeprecationWarning, match="depth_level is deprecated; use depth"):
        legacy = ran_infomap.get_effective_num_modules(depth_level=2)
    assert legacy == ran_infomap.get_effective_num_modules(depth=2)


@pytest.mark.fast
def test_get_dataframe_depth_level_deprecated_matches_depth(ran_infomap):
    pytest.importorskip("pandas")
    columns = ["node_id", "module_id"]
    with pytest.warns(DeprecationWarning, match="depth_level is deprecated; use depth"):
        legacy = ran_infomap.get_dataframe(
            columns=columns, states=False, depth_level=2
        )
    with pytest.warns(DeprecationWarning, match="get_dataframe.*to_dataframe"):
        new = ran_infomap.get_dataframe(columns=columns, states=False, depth=2)
    assert legacy.equals(new)


@pytest.mark.fast
def test_to_dataframe_depth_level_deprecated_matches_level(ran_infomap):
    pytest.importorskip("pandas")
    columns = ["node_id", "module_id"]
    with pytest.warns(DeprecationWarning, match="depth_level is deprecated; use level"):
        legacy = ran_infomap.to_dataframe(
            columns=columns, states=False, depth_level=2, sort=True
        )
    new = ran_infomap.to_dataframe(columns=columns, states=False, level=2, sort=True)
    assert legacy.equals(new)
