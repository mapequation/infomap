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
    assert result.codelength == pytest.approx(3.385830820341408, abs=1e-4)


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
    from_method = net.run(options=_SETTINGS)

    expected = _oo_codelength(lambda im: im.add_links(_LINKS), **_SETTINGS)
    assert from_method.codelength == pytest.approx(expected)


def test_network_from_networkx_parity():
    graph = nx.karate_club_graph()

    net = Network.from_networkx(graph)
    net_result = net.run(options=_SETTINGS)

    expected = _oo_codelength(lambda im: im.add_networkx_graph(graph), **_SETTINGS)
    assert net_result.codelength == pytest.approx(expected)
    assert net.node_id_to_label  # mapping populated by the adapter


def test_network_from_file_parity(example_network_path):
    path = example_network_path("ninetriangles.net")
    settings = {"silent": True, "num_trials": 10, "seed": 123}

    net = Network.from_file(path)
    result = net.run(options=settings)

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
    exported_modules = {
        node: data["infomap_module"] for node, data in exported.nodes(data=True)
    }
    assert exported_modules == result.modules()


def test_to_networkx_attributes_have_native_types():
    graph = nx.karate_club_graph()
    result = infomap.run(graph, **_SETTINGS)

    exported = infomap.to_networkx(result)
    for _, data in exported.nodes(data=True):
        assert isinstance(data["infomap_module"], int)
        assert isinstance(data["infomap_path"], str)
        assert isinstance(data["infomap_level_1"], int)
        assert isinstance(data["flow"], float)
    for _, _, data in exported.edges(data=True):
        assert isinstance(data["weight"], float)


def test_to_networkx_graph_survives_graphml_and_gexf(tmp_path):
    graph = nx.karate_club_graph()
    result = infomap.run(graph, **_SETTINGS)
    exported = infomap.to_networkx(result)

    graphml_path = tmp_path / "karate.graphml"
    nx.write_graphml(exported, graphml_path)
    read_back = nx.read_graphml(graphml_path)
    node = next(iter(read_back.nodes))
    assert isinstance(read_back.nodes[node]["infomap_module"], int)
    assert isinstance(read_back.nodes[node]["flow"], float)

    nx.write_gexf(exported, tmp_path / "karate.gexf")


def test_to_networkx_directed_flow_model_exports_digraph():
    # Result directedness derives from the effective flow model, so an
    # explicit flow_model="directed" exports a DiGraph exactly like
    # directed=True does.
    net = Network().add_links(_LINKS)
    for options in ({"flow_model": "directed"}, {"directed": True}):
        result = infomap.run(net, **options, **_SETTINGS)
        assert isinstance(infomap.to_networkx(result), nx.DiGraph)

    undirected = infomap.run(net, **_SETTINGS)
    assert not isinstance(infomap.to_networkx(undirected), nx.DiGraph)


def test_network_result_generation_guard_after_rerun():
    net = Network()
    net.add_links(_LINKS)
    result = net.run(options=_SETTINGS)

    # Eager scalars stay valid; node-level access is fine before the re-run.
    assert result.codelength > 0
    assert len(list(result.nodes())) > 0

    codelength_before = result.codelength
    net.run(options={"silent": True, "seed": 1})

    # Eager scalars captured at run() time remain valid on the stale Result.
    assert result.codelength == codelength_before
    # Node-level access on the stale Result now raises.
    with pytest.raises(_StaleResultError):
        list(result.nodes())


def test_run_edge_index_matches_oo():
    np = pytest.importorskip("numpy")
    edge_index = np.array([[0, 1, 0, 2, 3, 3, 4], [1, 2, 2, 3, 4, 5, 5]])
    engine = {"silent": True, "num_trials": 5, "seed": 123}

    # `directed` is an edge_index adapter argument, so it goes on the builder,
    # not on run() (which would now reject it). This is the documented path.
    result = infomap.run(Network.from_edge_index(edge_index, directed=False), **engine)
    assert isinstance(result, Result)

    expected = _oo_codelength(
        lambda im: im.add_edge_index(edge_index, directed=False), **engine
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
    net_result = net.run(options=_SETTINGS)

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
    assert all(isinstance(value, int) for value in exported.vs["infomap_module"])
    assert all(isinstance(value, str) for value in exported.vs["infomap_path"])
    assert all(isinstance(value, float) for value in exported.vs["flow"])


def test_to_igraph_graph_survives_graphml(tmp_path):
    ig = pytest.importorskip("igraph")
    graph = ig.Graph.Famous("Zachary")
    result = infomap.run(graph, **_SETTINGS)
    exported = infomap.to_igraph(result)

    path = tmp_path / "karate.graphml"
    exported.write_graphml(str(path))
    read_back = ig.Graph.Read_GraphML(str(path))
    assert all(value is not None for value in read_back.vs["infomap_module"])


def test_run_rejects_unsupported_input():
    with pytest.raises(TypeError):
        infomap.run(42)


def test_run_network_accepts_initial_partition():
    net = Network()
    net.add_links(_LINKS)
    result = infomap.run(
        net, initial_partition={1: 0, 2: 0}, silent=True, num_trials=1, seed=1
    )
    assert isinstance(result, Result)


def test_network_run_initial_partition_matches_infomap():
    partition = {1: 0, 2: 0, 3: 0, 4: 1, 5: 1, 6: 1}

    net = Network()
    net.add_links(_LINKS)
    net_result = net.run(options=_SETTINGS, initial_partition=partition)

    im = Infomap(**_SETTINGS)
    im.add_links(_LINKS)
    im.run(initial_partition=partition)
    assert net_result.codelength == pytest.approx(im.codelength)


# -- adapter-argument guard (run() configures the engine, not input building) --


def test_run_rejects_networkx_adapter_kwarg():
    graph = nx.Graph()
    graph.add_edge("a", "b", capacity=5.0)
    with pytest.raises(TypeError, match="Network.from_networkx"):
        infomap.run(graph, weight="capacity", silent=True)


def test_run_rejects_scipy_directed_kwarg():
    sp = pytest.importorskip("scipy.sparse")
    A = sp.csr_matrix([[0, 2], [3, 0]])
    # `directed` is ambiguous (engine flow flag vs scipy adapter param); for a
    # scipy input run() must reject it rather than silently folding the matrix.
    with pytest.raises(TypeError, match="Network.from_scipy_sparse_matrix"):
        infomap.run(A, directed=True, silent=True)


def test_run_rejects_edge_index_adapter_kwarg():
    import numpy as np

    edge_index = np.array([[0, 1, 2], [1, 2, 0]])
    with pytest.raises(TypeError, match="Network.from_edge_index"):
        infomap.run(edge_index, edge_weight=[1.0, 1.0, 1.0], silent=True)


def test_run_allows_directed_on_edge_list():
    # `directed` is a legitimate engine flag for inputs with no `directed`
    # adapter param (a plain edge list); it must NOT be rejected there.
    result = infomap.run(_LINKS, directed=True, silent=True, num_trials=1, seed=1)
    assert isinstance(result, Result)


def test_run_options_instance_does_not_false_positive_guard():
    # An Options instance expands to every field (directed=None included) via
    # to_kwargs(); that must not trip the scipy `directed` guard.
    sp = pytest.importorskip("scipy.sparse")
    from infomap import Options

    A = sp.csr_matrix([[0, 1], [1, 0]])
    result = infomap.run(A, options=Options(silent=True, num_trials=1, seed=1))
    assert isinstance(result, Result)


def test_run_float_link_matrix_is_links_not_edge_index():
    np = pytest.importorskip("numpy")
    # A weighted link matrix (rows of (source, target, weight)) is (2, 3) and
    # float; it must be read as link rows, not misrouted to the edge_index path.
    links = np.array([[1, 2, 1.5], [2, 3, 2.0]])
    result = infomap.run(links, silent=True, num_trials=1, seed=1)
    expected = _oo_codelength(
        lambda im: im.add_links(links), silent=True, num_trials=1, seed=1
    )
    assert result.codelength == pytest.approx(expected)


def test_run_rejects_networkx_meta_attribute_kwarg():
    graph = nx.Graph()
    graph.add_edge("a", "b")
    graph.nodes["a"]["ct"] = 0
    graph.nodes["b"]["ct"] = 1
    with pytest.raises(TypeError, match="Network.from_networkx"):
        infomap.run(graph, meta_attribute="ct", silent=True)
