from __future__ import annotations

from types import SimpleNamespace

import networkx as nx
import pytest

import infomap
import infomap._facade as facade


pytestmark = pytest.mark.fast


def _flatten(communities):
    return set().union(*communities) if communities else set()


def test_find_communities_returns_original_string_labels():
    graph = nx.Graph()
    graph.add_edges_from([("a", "b"), ("c", "d")])

    communities = infomap.find_communities(graph, seed=123, num_trials=1)

    assert isinstance(communities, list)
    assert all(isinstance(community, set) for community in communities)
    assert _flatten(communities) == {"a", "b", "c", "d"}
    assert all(isinstance(node, str) for community in communities for node in community)


def test_find_communities_can_annotate_networkx_nodes():
    graph = nx.Graph()
    graph.add_edges_from([(1, 2), (2, 3), (10, 11), (11, 12)])

    communities = infomap.find_communities(
        graph,
        module_attribute="community",
        flow_attribute="flow",
        seed=123,
        num_trials=1,
    )

    assert _flatten(communities) == set(graph.nodes)
    assert set(nx.get_node_attributes(graph, "community")) == set(graph.nodes)
    assert set(nx.get_node_attributes(graph, "flow")) == set(graph.nodes)
    assert all(isinstance(graph.nodes[node]["community"], int) for node in graph.nodes)
    assert all(isinstance(graph.nodes[node]["flow"], float) for node in graph.nodes)


def test_find_communities_does_not_mutate_graph_by_default():
    graph = nx.Graph([(1, 2), (2, 3)])

    infomap.find_communities(graph, seed=123, num_trials=1)

    assert all("community" not in data for _, data in graph.nodes(data=True))
    assert all("flow" not in data for _, data in graph.nodes(data=True))


def test_find_communities_sets_directed_for_digraph(monkeypatch):
    instances = []

    class FakeInfomap:
        flowModelIsSet = False

        def __init__(self, **options):
            self.options = options
            self.directed = False
            self.nodes = [
                SimpleNamespace(state_id=0, module_id=1, flow=0.5),
                SimpleNamespace(state_id=1, module_id=1, flow=0.5),
            ]
            instances.append(self)

        def setDirected(self, value):
            self.directed = value

        def add_node(self, node_id, name=None):
            pass

        def add_link(self, source_id, target_id, weight=1.0):
            pass

        def run(self):
            pass

    monkeypatch.setattr(facade, "Infomap", FakeInfomap)

    communities = infomap.find_communities(nx.DiGraph([("source", "target")]))

    assert communities == [{"source", "target"}]
    assert instances[0].directed is True
    assert instances[0].options["silent"] is True
    assert instances[0].options["no_file_output"] is True


def test_find_communities_partitions_state_network_nodes():
    graph = nx.Graph()
    graph.add_node("state-b", node_id="beta")
    graph.add_node("state-a", node_id="alpha")
    graph.add_node("state-b-2", node_id="beta")
    graph.add_edge("state-b", "state-a")
    graph.add_edge("state-a", "state-b-2")

    communities = infomap.find_communities(graph, seed=123, num_trials=1)

    assert _flatten(communities) == set(graph.nodes)


def test_find_communities_partitions_multilayer_network_nodes():
    graph = nx.Graph()
    graph.add_node("state-b-layer-1", node_id="beta", layer_id=1)
    graph.add_node("state-a-layer-1", node_id="alpha", layer_id=1)
    graph.add_node("state-a-layer-2", node_id="alpha", layer_id=2)
    graph.add_edge("state-b-layer-1", "state-a-layer-1", weight=2.0)
    graph.add_edge("state-a-layer-1", "state-a-layer-2")

    communities = infomap.find_communities(graph, seed=123, num_trials=1)

    assert _flatten(communities) == set(graph.nodes)
