from __future__ import annotations

import pytest

from infomap import Infomap, Result


def _two_triangles() -> Infomap:
    im = Infomap(silent=True)
    im.add_link(0, 1)
    im.add_link(1, 2)
    im.add_link(2, 0)
    im.add_link(2, 3)
    im.add_link(3, 4)
    im.add_link(4, 5)
    im.add_link(5, 3)
    return im


@pytest.mark.fast
def test_get_node_data_single_traversal_matches_get_modules():
    im = _two_triangles()
    im.run()
    nd = im._core.get_node_data(1, False)
    by_node = dict(zip(list(nd.node_id), list(nd.module_id)))
    assert by_node == im.get_modules(1, False)


@pytest.mark.fast
def test_get_node_data_camelcase_swig_accessor_still_works():
    im = _two_triangles()
    im.run()
    nd = im._core.getNodeData(1, False)
    assert dict(zip(list(nd.node_id), list(nd.module_id))) == im.get_modules(1, False)


@pytest.mark.fast
def test_run_returns_result_with_eager_scalars():
    im = _two_triangles()
    result = im.run()

    assert isinstance(result, Result)
    assert result is im._result
    assert result.codelength == im.codelength
    assert result.num_top_modules == im.num_top_modules
    assert result.num_levels == im.max_depth
    assert result.num_non_trivial_top_modules == im.num_non_trivial_top_modules
    assert result.index_codelength == im.index_codelength
    assert result.module_codelength == im.module_codelength
    assert result.one_level_codelength == im.one_level_codelength
    assert result.relative_codelength_savings == im.relative_codelength_savings
    assert result.entropy_rate == im.entropy_rate
    assert result.meta_codelength == im.meta_codelength
    assert result.num_nodes == im.num_nodes
    assert result.num_links == im.num_links


@pytest.mark.fast
def test_effective_num_modules_is_computed_lazily(monkeypatch):
    """``effective_num_*`` must not traverse the result tree until read.

    Computing them eagerly in ``Result.__init__`` walked the whole tree (twice)
    on every ``run()``, roughly doubling ``run()`` wall-clock for mid-size
    networks even when the caller never reads them.
    """
    import infomap.result as result_mod

    calls: list[int] = []
    original = result_mod.Result._compute_effective_num_modules.__func__

    def counting(cls, core, depth):
        calls.append(depth)
        return original(cls, core, depth)

    monkeypatch.setattr(
        result_mod.Result,
        "_compute_effective_num_modules",
        classmethod(counting),
    )

    im = _two_triangles()
    result = im.run()

    # Nothing computed just by running.
    assert calls == []

    # First read computes; depth is the only thing requested.
    _ = result.effective_num_top_modules
    assert calls == [1]
    _ = result.effective_num_leaf_modules
    assert calls == [1, -1]

    # Cached: a second read does not recompute.
    _ = result.effective_num_top_modules
    _ = result.effective_num_leaf_modules
    assert calls == [1, -1]


@pytest.mark.fast
def test_effective_num_modules_cached_value_survives_rerun():
    """Once read, the snapshot value is cached and stays valid after a re-run."""
    im = _two_triangles()
    result = im.run()

    top = result.effective_num_top_modules
    leaf = result.effective_num_leaf_modules

    im.run()  # rebuilds the C++ tree

    assert result.effective_num_top_modules == top
    assert result.effective_num_leaf_modules == leaf


@pytest.mark.fast
def test_result_modules_matches_get_modules():
    im = _two_triangles()
    result = im.run()
    assert result.modules() == im.get_modules()
    assert result.modules(1, states=False) == im.get_modules(1, False)


@pytest.mark.fast
def test_result_is_frozen():
    im = _two_triangles()
    result = im.run()
    with pytest.raises(AttributeError):
        result.codelength = 0.0
    with pytest.raises(AttributeError):
        result._codelength = 0.0
    with pytest.raises(AttributeError):
        del result._codelength


@pytest.mark.fast
def test_result_nodes_yield_immutable_treenodes():
    im = _two_triangles()
    result = im.run()
    nodes = list(result.nodes())
    assert len(nodes) == 6
    node = nodes[0]
    assert node.node_id in {1, 2, 3, 4, 5, 6}
    assert isinstance(node.path, tuple)
    with pytest.raises(AttributeError):
        node.node_id = 99


@pytest.mark.fast
def test_result_modules_raises_on_higher_order_without_states(network_fixture_path):
    im = Infomap(silent=True)
    im.read_file(str(network_fixture_path("states.net")))
    result = im.run()
    assert result.have_memory
    with pytest.raises(RuntimeError, match="higher-order network without states"):
        result.modules(states=False)
    # states=True is fine and matches the legacy accessor.
    assert result.modules(states=True) == im.get_modules(1, True)


_STATES_NET_STATE_NAMES = {
    1: "α~_i",
    2: "β~_j",
    3: "γ~_k",
    4: "δ~_i",
    5: "ε~_l",
    6: "ζ~_m",
}
_STATES_NET_PHYSICAL_NAMES = {1: "i", 2: "j", 3: "k", 4: "l", 5: "m"}


@pytest.mark.fast
def test_state_names_accessor_exposes_state_node_names(network_fixture_path):
    im = Infomap(silent=True)
    im.read_file(str(network_fixture_path("states.net")))
    result = im.run()

    # New accessor: state_id -> state-node name (parsed from the *States section).
    assert result.state_names == _STATES_NET_STATE_NAMES
    # Physical names stay keyed by node_id, unchanged.
    assert result.names == _STATES_NET_PHYSICAL_NAMES


@pytest.mark.fast
def test_to_dataframe_state_name_column_distinguishes_state_nodes(
    network_fixture_path,
):
    pytest.importorskip("pandas")
    im = Infomap(silent=True)
    im.read_file(str(network_fixture_path("states.net")))
    result = im.run()

    df = result.to_dataframe(
        states=True, columns=["state_id", "node_id", "name", "state_name"]
    )

    # The "state_name" column carries the per-state-node name.
    assert dict(zip(df["state_id"], df["state_name"])) == _STATES_NET_STATE_NAMES
    # The "name" column is unchanged: the physical name keyed by node_id.
    assert all(
        name == _STATES_NET_PHYSICAL_NAMES[node_id]
        for node_id, name in zip(df["node_id"], df["name"])
    )
    # State nodes 1 and 4 are both physical node 1: the physical name collapses
    # them to "i", while state_name keeps them distinct.
    node1 = df[df["node_id"] == 1]
    assert sorted(node1["state_name"]) == ["α~_i", "δ~_i"]
    assert list(node1["name"]) == ["i", "i"]

    # nodes() exposes the same state_name on each TreeNode.
    assert {n.state_id: n.state_name for n in result.nodes(states=True)} == (
        _STATES_NET_STATE_NAMES
    )


@pytest.mark.fast
def test_state_name_falls_back_to_physical_name_for_first_order(example_network_path):
    pytest.importorskip("pandas")
    im = Infomap(silent=True)
    im.read_file(str(example_network_path("twotriangles.net")))
    result = im.run()

    # A first-order network has no per-state names.
    assert result.state_names == {}

    df = result.to_dataframe(columns=["node_id", "name", "state_name"])
    # state_name falls back to the physical name.
    assert list(df["state_name"]) == list(df["name"])


@pytest.mark.fast
def test_stale_result_raises_on_node_access_but_keeps_scalars():
    im = _two_triangles()
    first = im.run()
    first_codelength = first.codelength

    # Re-run: the C++ tree is rebuilt, so node-level access on the first Result
    # must raise instead of reading freed memory.
    second = im.run()
    assert second is not first

    with pytest.raises(RuntimeError, match="stale Result"):
        first.modules()
    with pytest.raises(RuntimeError, match="stale Result"):
        list(first.nodes())
    with pytest.raises(RuntimeError, match="stale Result"):
        first.to_dataframe(columns=["node_id"])
    with pytest.raises(RuntimeError, match="stale Result"):
        list(first.tree())
    with pytest.raises(RuntimeError, match="stale Result"):
        list(first.leaf_modules())

    # Eager scalars captured at run() time stay valid on the stale Result.
    assert first.codelength == first_codelength

    # The current Result still works.
    assert second.modules() == im.get_modules()


@pytest.mark.fast
def test_stale_result_raises_when_iterator_acquired_before_rerun():
    # tree()/leaf_modules() hand back live engine iterators. Acquiring one and
    # *then* re-running must raise on consumption -- the guard fires before each
    # step, not only at call time -- rather than walking a rebuilt C++ tree.
    im = _two_triangles()
    first = im.run()
    tree_it = first.tree()
    leaf_it = first.leaf_modules()

    im.run()  # rebuilds the C++ tree

    with pytest.raises(RuntimeError, match="stale Result"):
        list(tree_it)
    with pytest.raises(RuntimeError, match="stale Result"):
        list(leaf_it)


@pytest.mark.fast
def test_result_node_data_is_cached_per_level_states():
    im = _two_triangles()
    result = im.run()

    call_count = 0
    original = type(im._core).get_node_data

    def counting_get_node_data(self, level=1, states=False):
        nonlocal call_count
        call_count += 1
        return original(self, level, states)

    type(im._core).get_node_data = counting_get_node_data
    try:
        result.modules()
        result.modules()
        list(result.nodes())
        assert call_count == 1  # one (level=1, states=False) traversal, cached
    finally:
        type(im._core).get_node_data = original


@pytest.mark.fast
def test_stale_result_raises_on_links_acquired_before_rerun():
    im = _two_triangles()
    first = im.run()
    links = first.links(data="flow")  # acquire before rerun
    im.run()
    with pytest.raises(RuntimeError, match="stale Result"):
        list(links)


@pytest.mark.fast
def test_directed_is_an_eager_snapshot_and_export_guards():
    pytest.importorskip("networkx")
    import infomap

    im = _two_triangles()
    first = im.run()
    assert isinstance(first._directed, bool)  # captured eagerly in __init__
    assert infomap.to_networkx(first).number_of_nodes() > 0
    im.run()  # rerun -> export of the stale Result must raise, not read the engine
    with pytest.raises(RuntimeError, match="stale Result"):
        infomap.to_networkx(first)


@pytest.mark.fast
def test_treenode_is_immutable_to_delete():
    node = next(_two_triangles().run().nodes())
    with pytest.raises(AttributeError):
        del node.node_id


def test_to_dataframe_depth_and_deprecated_aliases_agree(
    make_infomap, example_network_path
):
    pytest.importorskip("pandas")

    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("ninetriangles.net")))
    result = im.run()

    by_depth = result.to_dataframe(columns=["node_id", "module_id"], depth=2)
    by_level = result.to_dataframe(columns=["node_id", "module_id"], level=2)
    by_depth_level = result.to_dataframe(
        columns=["node_id", "module_id"], depth_level=2
    )

    assert by_depth.equals(by_level)
    assert by_depth.equals(by_depth_level)

    # Equal values across the primary kwarg and an alias are accepted.
    both = result.to_dataframe(columns=["node_id"], depth=2, depth_level=2)
    assert both.equals(result.to_dataframe(columns=["node_id"], depth=2))


def test_to_dataframe_conflicting_depth_aliases_raise(
    make_infomap, example_network_path
):
    pytest.importorskip("pandas")

    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("ninetriangles.net")))
    result = im.run()

    with pytest.raises(ValueError, match="Conflicting values for the tree depth"):
        result.to_dataframe(depth=1, level=2)


def test_to_dataframe_index_true_is_rejected(make_infomap, example_network_path):
    pytest.importorskip("pandas")

    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("ninetriangles.net")))
    result = im.run()

    # index=True has no column to key on; it must raise clearly instead of a
    # bare pandas KeyError(True).
    with pytest.raises(TypeError, match="index=True is not a column"):
        result.to_dataframe(index=True)

    # A real column key still works, and False/None keep the RangeIndex.
    assert result.to_dataframe(index="node_id").index.name == "node_id"
    assert result.to_dataframe(index=False).index.name is None


def test_to_dataframe_deprecated_aliases_stay_silent(
    make_infomap, example_network_path
):
    # Docs-only deprecation policy: the aliases must not warn at runtime.
    pytest.importorskip("pandas")
    import warnings

    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("ninetriangles.net")))
    result = im.run()

    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        result.to_dataframe(level=2)
        result.to_dataframe(depth_level=2)


@pytest.mark.fast
def test_result_repr_is_concise_text():
    text = repr(_two_triangles().run())
    assert text.startswith("Result(")
    assert "codelength" in text


@pytest.mark.fast
def test_infomap_backed_result_renders_html_card():
    html = _two_triangles().run()._repr_html_()
    assert isinstance(html, str) and "infomap" in html.lower()


@pytest.mark.fast
def test_network_backed_result_html_declines_without_crashing():
    import infomap

    # datasets return a Network, so this Result is Network-backed; the rich card
    # is engine-specific, so _repr_html_ declines (None) and the notebook falls
    # back to the text repr instead of raising 'Network has no attribute summary'.
    result = infomap.run(infomap.datasets.two_triangles(), seed=1)
    assert result._repr_html_() is None
    assert repr(result).startswith("Result(")


@pytest.mark.fast
def test_stale_result_html_declines():
    im = _two_triangles()
    first = im.run()
    im.run()  # re-run makes `first` stale
    assert first._repr_html_() is None
