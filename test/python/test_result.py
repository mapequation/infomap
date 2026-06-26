from __future__ import annotations

import pytest

from infomap import Infomap, Result


def _two_triangles() -> Infomap:
    im = Infomap(silent=True, no_file_output=True)
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
    im = Infomap(silent=True, no_file_output=True)
    im.read_file(str(network_fixture_path("states.net")))
    result = im.run()
    assert result.have_memory
    with pytest.raises(RuntimeError, match="higher-order network without states"):
        result.modules(states=False)
    # states=True is fine and matches the legacy accessor.
    assert result.modules(states=True) == im.get_modules(1, True)


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
