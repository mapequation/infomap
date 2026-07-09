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

        @property
        def _core(self):
            return self

        def __init__(self, **options):
            self.options = options
            self.directed = False
            instances.append(self)

        def get_node_data(self, level=1, states=False):
            return SimpleNamespace(
                state_id=[0, 1],
                module_id=[1, 1],
                flow=[0.5, 0.5],
            )

        def setDirected(self, value):
            self.directed = value

        def add_node(self, node_id, name=None):
            pass

        def add_link(self, source_id, target_id, weight=1.0):
            pass

        def run(self, initial_partition=None):
            self.initial_partition = initial_partition

    monkeypatch.setattr(facade, "Infomap", FakeInfomap)

    communities = infomap.find_communities(nx.DiGraph([("source", "target")]))

    assert communities == [{"source", "target"}]
    assert instances[0].directed is True
    assert instances[0].options["silent"] is True
    assert instances[0].options["no_file_output"] is True
    assert instances[0].initial_partition is None


def test_find_communities_accepts_initial_partition_with_original_labels(monkeypatch):
    instances = []

    class FakeInfomap:
        flowModelIsSet = True

        @property
        def _core(self):
            return self

        def __init__(self, **options):
            self.options = options
            instances.append(self)

        def get_node_data(self, level=1, states=False):
            return SimpleNamespace(
                state_id=[0, 1],
                module_id=[1, 2],
                flow=[0.5, 0.5],
            )

        def add_node(self, node_id, name=None):
            pass

        def add_link(self, source_id, target_id, weight=1.0):
            pass

        def run(self, initial_partition=None):
            self.initial_partition = initial_partition

    monkeypatch.setattr(facade, "Infomap", FakeInfomap)

    communities = infomap.find_communities(
        nx.Graph([("source", "target")]),
        initial_partition={"source": 7, "target": 8},
    )

    assert communities == [{"source"}, {"target"}]
    assert instances[0].initial_partition == {0: 7, 1: 8}


def test_find_communities_partitions_state_network_nodes():
    graph = nx.Graph()
    graph.add_node("state-b", node_id="beta")
    graph.add_node("state-a", node_id="alpha")
    graph.add_node("state-b-2", node_id="beta")
    graph.add_edge("state-b", "state-a")
    graph.add_edge("state-a", "state-b-2")

    communities = infomap.find_communities(graph, seed=123, num_trials=1)

    assert _flatten(communities) == set(graph.nodes)


def test_add_networkx_multigraph_with_parallel_edges_and_self_loop():
    # A MultiGraph forwards each parallel edge to add_link (weights accumulate in
    # the engine) and passes self-loops through. Exercise both add_networkx_graph
    # + run and the functional infomap.run(g) entry point and assert a sensible
    # clustering rather than a crash.
    graph = nx.MultiGraph()
    graph.add_edge("a", "b")
    graph.add_edge("a", "b")  # parallel edge
    graph.add_edge("b", "c")
    graph.add_edge("c", "d")
    graph.add_edge("d", "a")
    graph.add_edge("a", "a")  # self-loop

    im = infomap.Infomap(silent=True, no_file_output=True, seed=123, num_trials=1)
    mapping = im.add_networkx_graph(graph)
    result = im.run()

    assert set(mapping.values()) == set(graph.nodes)
    assert result.num_top_modules >= 1
    assert isinstance(result.codelength, float)

    # The functional entry point accepts the same MultiGraph directly.
    functional = infomap.run(
        graph, silent=True, no_file_output=True, seed=123, num_trials=1
    )
    assert sum(1 for _ in functional.nodes()) == graph.number_of_nodes()


def test_find_communities_partitions_multilayer_network_nodes():
    graph = nx.Graph()
    graph.add_node("state-b-layer-1", node_id="beta", layer_id=1)
    graph.add_node("state-a-layer-1", node_id="alpha", layer_id=1)
    graph.add_node("state-a-layer-2", node_id="alpha", layer_id=2)
    graph.add_edge("state-b-layer-1", "state-a-layer-1", weight=2.0)
    graph.add_edge("state-a-layer-1", "state-a-layer-2")

    communities = infomap.find_communities(graph, seed=123, num_trials=1)

    assert _flatten(communities) == set(graph.nodes)


# -- error / edge paths (parity with test_scipy.py / test_igraph.py) -----------


def test_add_networkx_graph_empty_returns_empty_mapping():
    im = infomap.Infomap(silent=True, no_file_output=True)
    assert im.add_networkx_graph(nx.Graph()) == {}


def test_find_communities_empty_graph_returns_empty_list():
    assert infomap.find_communities(nx.Graph()) == []


def test_add_networkx_multilayer_diagonal_link_raises():
    # In the default intra/inter format a link that changes BOTH layer and
    # physical node is a "diagonal" link the format cannot express.
    graph = nx.Graph()
    graph.add_node("a-layer-1", node_id=1, layer_id=1)
    graph.add_node("b-layer-2", node_id=2, layer_id=2)
    graph.add_edge("a-layer-1", "b-layer-2")

    im = infomap.Infomap(silent=True, no_file_output=True)
    with pytest.raises(ValueError, match="diagonal"):
        im.add_networkx_graph(graph)

    # The documented workaround (full multilayer format) accepts the same graph.
    im_full = infomap.Infomap(silent=True, no_file_output=True)
    mapping = im_full.add_networkx_graph(graph, multilayer_inter_intra_format=False)
    assert set(mapping.values()) == set(graph.nodes)


def test_add_networkx_layer_id_without_node_id_raises_clear_error():
    # layer_id present but node_id missing: a multilayer network needs node_id to
    # know each state node's physical node. Must be a clear ValueError, not the
    # bare KeyError(None) the implicit phys_map lookup would otherwise raise.
    graph = nx.Graph()
    graph.add_node("a", layer_id=1)
    graph.add_node("b", layer_id=2)
    graph.add_edge("a", "b")

    im = infomap.Infomap(silent=True, no_file_output=True)
    with pytest.raises(ValueError, match="node_id"):
        im.add_networkx_graph(graph)


# -- meta data via a node attribute -------------------------------------------

_META_LINKS = [(0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3), (2, 3)]


def _nx_with_meta(values):
    graph = nx.Graph()
    graph.add_edges_from(_META_LINKS)
    for node, category in values.items():
        graph.nodes[node]["ct"] = category
    return graph


def test_from_networkx_meta_attribute_engages_and_encodes_strings():
    # String categories that cross the two natural triangles incur a meta cost;
    # categories aligned with the modules do not. Proves meta_attribute wires
    # metadata and auto-encodes non-integer (string) labels.
    crossing = infomap.run(
        infomap.Network.from_networkx(
            _nx_with_meta({0: "a", 1: "b", 2: "a", 3: "a", 4: "b", 5: "b"}),
            meta_attribute="ct",
        ),
        silent=True, num_trials=5, seed=1, meta_data_rate=1.0,
    )
    aligned = infomap.run(
        infomap.Network.from_networkx(
            _nx_with_meta({0: "x", 1: "x", 2: "x", 3: "y", 4: "y", 5: "y"}),
            meta_attribute="ct",
        ),
        silent=True, num_trials=5, seed=1, meta_data_rate=1.0,
    )
    assert crossing.meta_codelength > 0
    assert aligned.meta_codelength == 0.0


def test_from_networkx_meta_attribute_typo_raises():
    graph = nx.Graph()
    graph.add_edges_from([(0, 1), (1, 2)])
    with pytest.raises(ValueError, match="not set on any node"):
        infomap.Network.from_networkx(graph, meta_attribute="missing")


def test_find_communities_accepts_meta_attribute():
    graph = _nx_with_meta({0: "a", 1: "b", 2: "a", 3: "a", 4: "b", 5: "b"})
    communities = infomap.find_communities(
        graph, meta_attribute="ct", seed=1, num_trials=3, meta_data_rate=1.0
    )
    assert set().union(*communities) == set(graph.nodes)


def test_infomap_add_networkx_graph_accepts_meta_attribute():
    graph = _nx_with_meta({0: "a", 1: "b", 2: "a", 3: "a", 4: "b", 5: "b"})
    im = infomap.Infomap(
        silent=True, no_file_output=True, seed=1, num_trials=5, meta_data_rate=1.0
    )
    im.add_networkx_graph(graph, meta_attribute="ct")
    result = im.run()
    assert result.meta_codelength > 0


# -- mixed labels + extra state ids (parity with the igraph helper) -----------


def test_label_to_internal_id_enumerates_mixed_int_and_string():
    from infomap.io.networkx import _label_to_internal_id

    # An int first label must NOT trigger identity mapping for the whole set:
    # "a" would otherwise map to itself and reach add_node.
    assert _label_to_internal_id([1, "a"]) == {1: 0, "a": 1}


def test_add_networkx_graph_handles_mixed_int_string_labels():
    graph = nx.Graph([(1, "a"), ("a", 2)])
    im = infomap.Infomap(silent=True, no_file_output=True, seed=1, num_trials=1)
    mapping = im.add_networkx_graph(graph)
    assert set(mapping.values()) == {1, "a", 2}
    assert im.run().num_top_modules >= 1


def test_find_communities_skips_unregistered_state_ids(monkeypatch):
    # community_node_data can surface extra leaf state ids (multilayer
    # relaxation) that are not registered vertices; the networkx helper must
    # skip them, not KeyError -- the same protection the igraph helper has.
    class FakeInfomap:
        flowModelIsSet = False

        @property
        def _core(self):
            return self

        def __init__(self, **options):
            pass

        def get_node_data(self, level=1, states=False):
            return SimpleNamespace(
                state_id=[0, 1, 99],  # 99 is not a registered vertex
                module_id=[1, 1, 2],
                flow=[0.4, 0.4, 0.2],
            )

        def setDirected(self, value):
            pass

        def add_node(self, node_id, name=None):
            pass

        def add_link(self, source_id, target_id, weight=1.0):
            pass

        def run(self, initial_partition=None):
            pass

    monkeypatch.setattr(facade, "Infomap", FakeInfomap)
    communities = infomap.find_communities(nx.Graph([("x", "y")]))
    assert _flatten(communities) == {"x", "y"}


# -- trials parity with the igraph helper -------------------------------------


def test_find_communities_trials_default_matches_igraph():
    import inspect

    nx_default = inspect.signature(infomap.find_communities).parameters["trials"].default
    ig_default = (
        inspect.signature(infomap.find_igraph_communities).parameters["trials"].default
    )
    # Both use the same sentinel; the effective default (10) is asserted below.
    assert nx_default is None and ig_default is None


def test_find_communities_rejects_trials_and_num_trials_conflict():
    with pytest.raises(ValueError, match="trials.*num_trials"):
        infomap.find_communities(nx.Graph([("a", "b")]), trials=1, num_trials=2)


@pytest.mark.parametrize(
    "kwargs, expected",
    [
        ({}, 10),                    # neither -> aligned default
        ({"trials": 7}, 7),          # the convenience alias
        ({"num_trials": 3}, 3),      # the engine option (back-compat)
    ],
)
def test_find_communities_resolves_num_trials(monkeypatch, kwargs, expected):
    captured = {}
    real_infomap = facade.Infomap

    class RecordingInfomap(real_infomap):
        def __init__(self, *args, **options):
            captured.update(options)
            super().__init__(*args, **options)

    monkeypatch.setattr(facade, "Infomap", RecordingInfomap)
    infomap.find_communities(nx.Graph([("a", "b")]), seed=1, **kwargs)
    assert captured["num_trials"] == expected
