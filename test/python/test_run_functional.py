"""Functional layer: ``infomap.run`` dispatch, ``Network.from_*``, conversions.

Verifies that the one-call ``infomap.run`` produces results that match the
object-oriented ``Infomap.run`` path for each supported input type, that
``Network`` builders/runs match, and that ``infomap.to_networkx`` round-trips a
``Result`` back into an annotated graph. The ``Network``-backed ``Result``
honours the same run-generation guard as the ``Infomap``-backed one.
"""

from __future__ import annotations

import networkx as nx
import pytest

import infomap
from infomap import Infomap, Network
from infomap.result import Result, _StaleResultError


pytestmark = pytest.mark.fast


_LINKS = [(1, 2), (2, 3), (1, 3), (3, 4), (4, 5), (4, 6), (5, 6)]
_SETTINGS = {"silent": True, "num_trials": 5, "seed": 123}


def _oo_codelength(loader, **settings):
    im = Infomap(**settings)
    loader(im)
    im.run()
    return im.codelength


def test_run_edgelist_matches_oo():
    result = infomap.run(_LINKS, **_SETTINGS)
    assert isinstance(result, Result)

    expected = _oo_codelength(lambda im: im.add_links(_LINKS), **_SETTINGS)
    assert result.codelength == pytest.approx(expected)


def test_run_networkx_matches_oo():
    graph = nx.karate_club_graph()
    result = infomap.run(graph, **_SETTINGS)
    assert isinstance(result, Result)

    expected = _oo_codelength(lambda im: im.add_networkx_graph(graph), **_SETTINGS)
    assert result.codelength == pytest.approx(expected)


def test_run_file_matches_oo(example_network_path):
    path = example_network_path("ninetriangles.net")
    settings = {"silent": True, "num_trials": 10, "seed": 123}

    result = infomap.run(path, **settings)
    assert isinstance(result, Result)

    expected = _oo_codelength(lambda im: im.read_file(str(path)), **settings)
    assert result.codelength == pytest.approx(expected)
    assert result.codelength == pytest.approx(3.4622731375264144, abs=1e-4)


def test_run_network_matches_oo():
    net = Network()
    net.add_links(_LINKS)
    result = infomap.run(net, **_SETTINGS)
    assert isinstance(result, Result)

    expected = _oo_codelength(lambda im: im.add_links(_LINKS), **_SETTINGS)
    assert result.codelength == pytest.approx(expected)


def test_run_options_object_and_overrides():
    options = infomap.Options(silent=True, num_trials=5, seed=999)
    # Override seed back to the canonical value; overrides must win.
    result = infomap.run(_LINKS, options=options, seed=123)
    expected = _oo_codelength(lambda im: im.add_links(_LINKS), **_SETTINGS)
    assert result.codelength == pytest.approx(expected)


def test_network_run_matches_infomap_run():
    net = Network()
    net.add_links(_LINKS)
    from_method = net.run(settings=_SETTINGS)

    expected = _oo_codelength(lambda im: im.add_links(_LINKS), **_SETTINGS)
    assert from_method.codelength == pytest.approx(expected)


def test_network_from_networkx_parity():
    graph = nx.karate_club_graph()

    net = Network.from_networkx(graph)
    net_result = net.run(settings=_SETTINGS)

    expected = _oo_codelength(lambda im: im.add_networkx_graph(graph), **_SETTINGS)
    assert net_result.codelength == pytest.approx(expected)
    assert net.node_id_to_label  # mapping populated by the adapter


def test_network_from_file_parity(example_network_path):
    path = example_network_path("ninetriangles.net")
    settings = {"silent": True, "num_trials": 10, "seed": 123}

    net = Network.from_file(path)
    result = net.run(settings=settings)

    expected = _oo_codelength(lambda im: im.read_file(str(path)), **settings)
    assert result.codelength == pytest.approx(expected)


def test_to_networkx_round_trip():
    graph = nx.karate_club_graph()
    result = infomap.run(graph, **_SETTINGS)

    exported = infomap.to_networkx(result)
    assert isinstance(exported, nx.Graph)
    assert exported.number_of_nodes() == graph.number_of_nodes()
    assert exported.number_of_edges() == graph.number_of_edges()
    assert all("infomap_module" in data for _, data in exported.nodes(data=True))
    assert all("flow" in data for _, data in exported.nodes(data=True))

    # The exported partition matches the result's module assignment.
    result_modules = result.modules()
    exported_modules = {
        node: int(data["infomap_module"])
        for node, data in exported.nodes(data=True)
    }
    assert exported_modules == result_modules


def test_network_result_generation_guard_after_rerun():
    net = Network()
    net.add_links(_LINKS)
    result = net.run(settings=_SETTINGS)

    # Eager scalars stay valid; node-level access is fine before the re-run.
    assert result.codelength > 0
    assert len(list(result.nodes())) > 0

    codelength_before = result.codelength
    net.run(settings={"silent": True, "seed": 1})

    # Eager scalars captured at run() time remain valid on the stale Result.
    assert result.codelength == codelength_before
    # Node-level access on the stale Result now raises.
    with pytest.raises(_StaleResultError):
        list(result.nodes())


def test_run_edge_index_matches_oo():
    np = pytest.importorskip("numpy")
    edge_index = np.array([[0, 1, 0, 2, 3, 3, 4], [1, 2, 2, 3, 4, 5, 5]])
    settings = {"silent": True, "num_trials": 5, "seed": 123, "directed": False}

    result = infomap.run(edge_index, **settings)
    assert isinstance(result, Result)

    expected = _oo_codelength(
        lambda im: im.add_edge_index(edge_index, directed=False), **settings
    )
    assert result.codelength == pytest.approx(expected)


def test_run_scipy_sparse_matches_oo():
    sparse = pytest.importorskip("scipy.sparse")
    rows = [0, 1, 0, 2, 3, 3, 4]
    cols = [1, 2, 2, 3, 4, 5, 5]
    n = 6
    A = sparse.coo_matrix(([1.0] * len(rows), (rows, cols)), shape=(n, n)).tocsr()
    A = A + A.T  # symmetric / undirected
    settings = {"silent": True, "num_trials": 5, "seed": 123}

    result = infomap.run(A, **settings)
    assert isinstance(result, Result)

    expected = _oo_codelength(
        lambda im: im.add_scipy_sparse_matrix(A), **settings
    )
    assert result.codelength == pytest.approx(expected)


def test_run_igraph_matches_oo():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph.Famous("Zachary")
    result = infomap.run(graph, **_SETTINGS)
    assert isinstance(result, Result)

    expected = _oo_codelength(lambda im: im.add_igraph_graph(graph), **_SETTINGS)
    assert result.codelength == pytest.approx(expected)


def test_network_from_igraph_parity():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph.Famous("Zachary")

    net = Network.from_igraph(graph)
    net_result = net.run(settings=_SETTINGS)

    expected = _oo_codelength(lambda im: im.add_igraph_graph(graph), **_SETTINGS)
    assert net_result.codelength == pytest.approx(expected)


def test_to_igraph_round_trip():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph.Famous("Zachary")
    result = infomap.run(graph, **_SETTINGS)

    exported = infomap.to_igraph(result)
    assert isinstance(exported, ig.Graph)
    assert exported.vcount() == graph.vcount()
    assert exported.ecount() == graph.ecount()
    assert "infomap_module" in exported.vs.attributes()
    assert "flow" in exported.vs.attributes()


def test_run_rejects_unsupported_input():
    with pytest.raises(TypeError):
        infomap.run(42)


def test_run_network_rejects_initial_partition():
    net = Network()
    net.add_links(_LINKS)
    with pytest.raises(TypeError):
        infomap.run(net, initial_partition={1: 0, 2: 0})
