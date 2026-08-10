from __future__ import annotations

from pathlib import Path

import networkx as nx
import pytest
from infomap import Infomap
from infomap.export import (
    annotate_igraph_graph,
    annotate_networkx_graph,
    write_gexf,
    write_graphml,
)

pytestmark = pytest.mark.fast


def _networkx_result(graph):
    im = Infomap(silent=True, num_trials=1, seed=123)
    mapping = im.add_networkx_graph(graph)
    im.run()
    return im, mapping


def test_write_networkx_graphml_can_be_read_back(tmp_path):
    graph = nx.karate_club_graph()
    im, mapping = _networkx_result(graph)
    path = tmp_path / "karate.graphml"

    write_graphml(graph, im, path, node_mapping=mapping)

    exported = nx.read_graphml(path)
    assert exported.number_of_nodes() == graph.number_of_nodes()
    assert all("infomap_module" in data for _, data in exported.nodes(data=True))


def test_write_networkx_gexf_can_be_read_back(tmp_path):
    graph = nx.Graph([(1, 2), (2, 3), (10, 11), (11, 12)])
    im, mapping = _networkx_result(graph)
    path = tmp_path / "communities.gexf"

    write_gexf(graph, im, path, node_mapping=mapping)

    exported = nx.read_gexf(path)
    assert exported.number_of_nodes() == graph.number_of_nodes()
    assert all("infomap_module" in data for _, data in exported.nodes(data=True))


def test_networkx_export_supports_custom_module_attribute():
    graph = nx.Graph([(1, 2)])
    im, mapping = _networkx_result(graph)

    annotated = annotate_networkx_graph(
        graph,
        im,
        node_mapping=mapping,
        module_attribute="community",
        flow_attribute="flow",
    )

    # The annotate family keeps its documented all-strings contract.
    assert set(nx.get_node_attributes(annotated, "community")) == set(graph.nodes)
    assert all(
        isinstance(value, str)
        for value in nx.get_node_attributes(annotated, "community").values()
    )
    assert all(
        isinstance(value, str)
        for value in nx.get_node_attributes(annotated, "flow").values()
    )


def test_networkx_export_can_disable_hierarchy_attributes():
    graph = nx.Graph([(1, 2)])
    im, mapping = _networkx_result(graph)

    annotated = annotate_networkx_graph(
        graph,
        im,
        node_mapping=mapping,
        include_hierarchy=False,
    )

    assert all("infomap_module" in data for _, data in annotated.nodes(data=True))
    assert all("infomap_path" not in data for _, data in annotated.nodes(data=True))
    assert all("infomap_level_1" not in data for _, data in annotated.nodes(data=True))


def test_networkx_export_copy_true_leaves_original_graph_unchanged():
    graph = nx.Graph([(1, 2)])
    im, mapping = _networkx_result(graph)

    annotated = annotate_networkx_graph(graph, im, node_mapping=mapping)

    assert annotated is not graph
    assert all("infomap_module" not in data for _, data in graph.nodes(data=True))
    assert all("infomap_module" in data for _, data in annotated.nodes(data=True))


def test_networkx_export_copy_false_mutates_original_graph():
    graph = nx.Graph([(1, 2)])
    im, mapping = _networkx_result(graph)

    annotated = annotate_networkx_graph(graph, im, node_mapping=mapping, copy=False)

    assert annotated is graph
    assert all("infomap_module" in data for _, data in graph.nodes(data=True))


def test_networkx_export_rejects_unrun_infomap():
    graph = nx.Graph([(1, 2)])
    im = Infomap(silent=True)
    im.add_networkx_graph(graph)

    with pytest.raises(ValueError, match="Run Infomap"):
        annotate_networkx_graph(graph, im)


def test_networkx_export_rejects_node_mismatch_when_strict():
    graph = nx.Graph([(1, 2)])
    im, mapping = _networkx_result(graph)
    graph.add_node(3)

    with pytest.raises(ValueError, match="without Infomap assignments"):
        annotate_networkx_graph(graph, im, node_mapping=mapping)


def test_networkx_export_warns_and_partially_annotates_when_not_strict():
    graph = nx.Graph([(1, 2)])
    im, mapping = _networkx_result(graph)
    graph.add_node(3)

    with pytest.warns(UserWarning, match="annotated only matching"):
        annotated = annotate_networkx_graph(
            graph, im, node_mapping=mapping, strict=False
        )

    assert "infomap_module" in annotated.nodes[1]
    assert "infomap_module" not in annotated.nodes[3]


def test_networkx_writer_warning_points_to_user_call(tmp_path):
    graph = nx.Graph([(1, 2)])
    im, mapping = _networkx_result(graph)
    graph.add_node(3)

    with pytest.warns(UserWarning, match="annotated only matching") as warnings:
        write_gexf(
            graph,
            im,
            tmp_path / "partial.gexf",
            node_mapping=mapping,
            strict=False,
        )

    assert Path(warnings[0].filename) == Path(__file__)


def test_networkx_export_requires_mapping_for_string_labels():
    graph = nx.Graph([("a", "b")])
    im, mapping = _networkx_result(graph)

    with pytest.raises(ValueError, match="without Infomap assignments"):
        annotate_networkx_graph(graph, im)

    annotated = annotate_networkx_graph(graph, im, node_mapping=mapping)
    assert set(nx.get_node_attributes(annotated, "infomap_module")) == {"a", "b"}


def _igraph_result(graph):
    im = Infomap(silent=True, num_trials=1, seed=123)
    im.add_igraph_graph(graph)
    im.run()
    return im


def test_igraph_graphml_export_writes_vertex_attributes(tmp_path):
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (2, 3)], directed=False)
    im = _igraph_result(graph)
    path = tmp_path / "communities.graphml"

    write_graphml(graph, im, path)

    exported = ig.Graph.Read_GraphML(str(path))
    assert "infomap_module" in exported.vs.attributes()
    assert all(value is not None for value in exported.vs["infomap_module"])


def test_igraph_graphml_export_accepts_generic_pathlike(tmp_path, fspath_only):
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (2, 3)], directed=False)
    im = _igraph_result(graph)
    path = tmp_path / "generic.graphml"

    write_graphml(graph, im, fspath_only(path))

    assert path.stat().st_size > 0


def test_networkx_graphml_export_accepts_generic_pathlike(tmp_path, fspath_only):
    graph = nx.Graph([(1, 2), (2, 3), (10, 11), (11, 12)])
    im, mapping = _networkx_result(graph)
    path = tmp_path / "generic.graphml"

    write_graphml(graph, im, fspath_only(path), node_mapping=mapping)

    assert path.stat().st_size > 0


def test_networkx_gexf_export_accepts_generic_pathlike(tmp_path, fspath_only):
    graph = nx.Graph([(1, 2), (2, 3), (10, 11), (11, 12)])
    im, mapping = _networkx_result(graph)
    path = tmp_path / "generic.gexf"

    write_gexf(graph, im, fspath_only(path), node_mapping=mapping)

    assert path.stat().st_size > 0


def test_igraph_export_copy_true_leaves_original_graph_unchanged():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1)], directed=False)
    im = _igraph_result(graph)

    annotated = annotate_igraph_graph(graph, im)

    assert annotated is not graph
    assert "infomap_module" not in graph.vs.attributes()
    assert "infomap_module" in annotated.vs.attributes()


def test_igraph_export_copy_false_mutates_original_graph():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1)], directed=False)
    im = _igraph_result(graph)

    annotated = annotate_igraph_graph(graph, im, copy=False)

    assert annotated is graph
    assert "infomap_module" in graph.vs.attributes()


def test_igraph_export_supports_custom_attribute_and_hierarchy_toggle():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1)], directed=False)
    im = _igraph_result(graph)

    annotated = annotate_igraph_graph(
        graph,
        im,
        module_attribute="community",
        include_hierarchy=False,
    )

    assert "community" in annotated.vs.attributes()
    assert "infomap_path" not in annotated.vs.attributes()
    assert "infomap_level_1" not in annotated.vs.attributes()


def test_write_gexf_rejects_igraph_graph(tmp_path):
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1)], directed=False)
    im = _igraph_result(graph)

    with pytest.raises(NotImplementedError, match="python-igraph does not provide"):
        write_gexf(graph, im, tmp_path / "communities.gexf")


def test_annotate_accepts_result_and_matches_infomap_input():
    graph = nx.Graph([(0, 1), (1, 2), (2, 0)])
    im = Infomap(silent=True, num_trials=1, seed=123)
    im.add_networkx_graph(graph)
    result = im.run()

    via_im = annotate_networkx_graph(graph, im)
    via_result = annotate_networkx_graph(graph, result)

    assert dict(via_result.nodes(data=True)) == dict(via_im.nodes(data=True))


def test_annotate_rejects_stale_result():
    graph = nx.Graph([(0, 1), (1, 2), (2, 0)])
    im = Infomap(silent=True, num_trials=1, seed=123)
    im.add_networkx_graph(graph)
    stale = im.run()
    im.run()  # rebuilds the C++ result tree; `stale` is now generation-stale

    with pytest.raises(RuntimeError, match="re-run"):
        annotate_networkx_graph(graph, stale)
