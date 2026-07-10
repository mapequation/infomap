from __future__ import annotations

import importlib
import subprocess
import sys
from types import SimpleNamespace

import pytest

import infomap
import infomap._facade as facade
from infomap import _optional
from infomap.io.igraph import _import_igraph, add_igraph_graph


pytestmark = pytest.mark.fast


def test_import_infomap_does_not_import_igraph():
    result = subprocess.run(
        [
            sys.executable,
            "-c",
            "import sys; sys.modules.pop('igraph', None); import infomap; print('igraph' in sys.modules)",
        ],
        check=True,
        capture_output=True,
        text=True,
    )

    assert result.stdout.strip() == "False"


def test_missing_igraph_error_mentions_extra(monkeypatch):
    # The guard imports via importlib.import_module (infomap._optional), so
    # block it there rather than through builtins.__import__.
    original_import_module = importlib.import_module

    def fake_import(name, *args, **kwargs):
        if name == "igraph":
            raise ImportError("missing igraph")
        return original_import_module(name, *args, **kwargs)

    monkeypatch.setattr(_optional.importlib, "import_module", fake_import)

    with pytest.raises(ImportError, match=r"infomap\[igraph\]"):
        _import_igraph()


def test_find_igraph_communities_returns_vertex_clustering_with_codelength():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (0, 2), (3, 4), (3, 5)], directed=False)
    graph.vs["name"] = ["a", "b", "c", "d", "e", "f"]

    clustering = infomap.find_igraph_communities(graph, trials=1, seed=123)

    assert isinstance(clustering, ig.VertexClustering)
    assert len(clustering.membership) == graph.vcount()
    assert set(clustering.membership) == set(range(max(clustering.membership) + 1))
    assert isinstance(clustering.codelength, float)


def test_find_igraph_communities_handles_empty_graph():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph()

    clustering = infomap.find_igraph_communities(
        graph,
        module_attribute="community",
        flow_attribute="flow",
    )

    assert isinstance(clustering, ig.VertexClustering)
    assert clustering.membership == []
    assert clustering.codelength == 0.0
    assert graph.vs["community"] == []
    assert graph.vs["flow"] == []


def test_add_igraph_graph_preserves_named_vertices(make_infomap):
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (0, 2)], directed=False)
    graph.vs["name"] = ["alpha", "beta", "gamma"]

    im = make_infomap(no_infomap=True)
    mapping = im.add_igraph_graph(graph)

    assert mapping == {0: "alpha", 1: "beta", 2: "gamma"}
    assert dict(im.names) == {0: "alpha", 1: "beta", 2: "gamma"}


def test_find_igraph_communities_can_annotate_vertices():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (1, 2), (3, 4), (4, 5)], directed=False)

    clustering = infomap.find_igraph_communities(
        graph,
        trials=1,
        module_attribute="community",
        flow_attribute="flow",
        seed=123,
    )

    assert graph.vs["community"] == clustering.membership
    assert len(graph.vs["flow"]) == graph.vcount()
    assert all(isinstance(flow, float) for flow in graph.vs["flow"])


def test_find_igraph_communities_does_not_mutate_graph_by_default():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (1, 2)], directed=False)

    infomap.find_igraph_communities(graph, trials=1, seed=123)

    assert "community" not in graph.vs.attributes()
    assert "flow" not in graph.vs.attributes()


def test_find_igraph_communities_sets_directed_for_directed_graph(monkeypatch):
    ig = pytest.importorskip("igraph")
    instances = []

    class FakeInfomap:
        flowModelIsSet = False

        @property
        def _core(self):
            return self

        def __init__(self, **options):
            self.options = options
            self.directed = False
            self.codelength = 1.0
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

        def run(self):
            pass

    monkeypatch.setattr(facade, "Infomap", FakeInfomap)
    graph = ig.Graph(edges=[(0, 1)], directed=True)

    clustering = infomap.find_igraph_communities(graph, trials=3)

    assert clustering.membership == [0, 0]
    assert instances[0].directed is True
    assert instances[0].options["silent"] is True
    assert instances[0].options["no_file_output"] is True
    assert instances[0].options["num_trials"] == 3


def test_add_igraph_graph_supports_edge_weight_inputs():
    ig = pytest.importorskip("igraph")

    class Recorder:
        flowModelIsSet = True

        @property
        def _core(self):
            return self

        def __init__(self):
            self.links = []

        def add_node(self, node_id, name=None):
            pass

        def add_link(self, source_id, target_id, weight=1.0):
            self.links.append((source_id, target_id, weight))

    graph = ig.Graph(edges=[(0, 1), (1, 2), (2, 0)], directed=False)
    graph.es["strength"] = [2, 3, 4]

    attr_recorder = Recorder()
    add_igraph_graph(attr_recorder, graph, edge_weights="strength")
    assert attr_recorder.links == [(0, 1, 2.0), (1, 2, 3.0), (0, 2, 4.0)]

    sequence_recorder = Recorder()
    add_igraph_graph(sequence_recorder, graph, edge_weights=[5, 6, 7])
    assert sequence_recorder.links == [(0, 1, 5.0), (1, 2, 6.0), (0, 2, 7.0)]

    unweighted_recorder = Recorder()
    add_igraph_graph(unweighted_recorder, graph, edge_weights=None)
    assert unweighted_recorder.links == [(0, 1, 1.0), (1, 2, 1.0), (0, 2, 1.0)]


def test_add_igraph_graph_rejects_invalid_edge_weight_inputs():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (1, 2)], directed=False)

    with pytest.raises(ValueError, match="does not exist"):
        add_igraph_graph(_Recorder(), graph, edge_weights="missing")

    graph.es["weight"] = ["a", "b"]
    with pytest.raises(ValueError, match="numeric"):
        add_igraph_graph(_Recorder(), graph, edge_weights="weight")

    graph.es["weight"] = [1.0, None]
    with pytest.raises(ValueError, match="missing|NaN"):
        add_igraph_graph(_Recorder(), graph, edge_weights="weight")

    with pytest.raises(ValueError, match="one value per edge"):
        add_igraph_graph(_Recorder(), graph, edge_weights=[1.0])


def test_find_igraph_communities_partitions_state_vertices():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (1, 2)], directed=False)
    graph.vs["node_id"] = ["beta", "alpha", "beta"]

    clustering = infomap.find_igraph_communities(graph, trials=1, seed=123)

    assert len(clustering.membership) == graph.vcount()


def test_add_igraph_graph_rejects_float_node_id_collision():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (1, 2)], directed=False)
    # Distinct labels 1 (int) and 1.0 (float) would both coerce to physical id 1,
    # silently merging two vertices into one physical node. Must raise instead.
    graph.vs["node_id"] = [1, 1.0, 2]

    with pytest.raises(ValueError, match=r"both map to physical id 1"):
        add_igraph_graph(_Recorder(), graph)


def test_add_igraph_graph_rejects_float_node_id_collision_in_label_branch():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (1, 2)], directed=False)
    # A non-numeric label forces the string/label branch; 1 and 1.0 are ==-equal
    # with the same hash, so they would silently merge there too. Must raise.
    graph.vs["node_id"] = [1, 1.0, "x"]

    with pytest.raises(ValueError, match=r"distinct but collide"):
        add_igraph_graph(_Recorder(), graph)


def test_add_igraph_graph_allows_repeated_integer_node_ids(make_infomap):
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (1, 2)], directed=False)
    # Two vertices legitimately sharing physical id 5 (a state network) must be
    # accepted -- only textually distinct labels colliding is an error.
    graph.vs["node_id"] = [5, 5, 3]

    im = make_infomap(no_infomap=True)
    im.add_igraph_graph(graph)

    assert dict(im.names) == {5: "5", 3: "3"}


def test_add_igraph_graph_names_non_numeric_physical_ids(make_infomap):
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (1, 2)], directed=False)
    graph.vs["node_id"] = ["beta", "alpha", "beta"]

    im = make_infomap(no_infomap=True)
    im.add_igraph_graph(graph)

    assert dict(im.names) == {0: "beta", 1: "alpha"}


def test_find_igraph_communities_partitions_multilayer_vertices():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (0, 2)], directed=False)
    graph.vs["node_id"] = ["alpha", "beta", "alpha"]
    graph.vs["layer_id"] = [1, 1, 2]

    clustering = infomap.find_igraph_communities(graph, trials=1, seed=123)

    assert len(clustering.membership) == graph.vcount()


def test_find_igraph_communities_partitions_nontrivial_multilayer_graph():
    # A non-trivial multilayer igraph graph: two layers, three physical nodes
    # each, intra-layer triangles plus same-physical inter-layer links. Routed
    # through find_igraph_communities end to end -- the kind of graph whose
    # generated state ids the old dense-[0, vcount) indexing assumed away.
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(directed=False)
    graph.add_vertices(6)
    graph.vs["node_id"] = [1, 2, 3, 1, 2, 3]
    graph.vs["layer_id"] = [1, 1, 1, 2, 2, 2]
    graph.add_edges([(0, 1), (1, 2), (0, 2)])  # layer 1 triangle
    graph.add_edges([(3, 4), (4, 5), (3, 5)])  # layer 2 triangle
    graph.add_edges([(0, 3), (1, 4), (2, 5)])  # same-physical inter-layer links

    clustering = infomap.find_igraph_communities(graph, trials=1, seed=123)

    assert isinstance(clustering, ig.VertexClustering)
    assert len(clustering.membership) == graph.vcount()
    assert all(module_id is not None for module_id in clustering.membership)
    assert set(clustering.membership) == set(range(max(clustering.membership) + 1))


def test_find_igraph_communities_skips_extra_minted_state_nodes(monkeypatch):
    # Regression for the multilayer alignment crash. A multilayer run can emit
    # *additional* leaf state nodes (inter-layer relaxation, matchable ids) whose
    # state ids fall outside [0, vcount). find_igraph_communities must map results
    # back through the adapter's node_mapping -- the set of state ids that belong
    # to a real vertex -- and ignore the extra ids, rather than letting them
    # collide with vertex indices or break alignment.
    #
    # The fake engine reports the two real vertices (state ids 0 and 1) plus an
    # extra minted node with state id 99 that is not in node_mapping.
    ig = pytest.importorskip("igraph")

    class FakeInfomap:
        flowModelIsSet = False

        @property
        def _core(self):
            return self

        def __init__(self, **options):
            self.options = options
            self.directed = False
            self.codelength = 1.0

        def get_node_data(self, level=1, states=False):
            return SimpleNamespace(
                state_id=[0, 1, 99],
                module_id=[1, 1, 2],
                flow=[0.4, 0.4, 0.2],
            )

        def setDirected(self, value):
            self.directed = value

        def set_name(self, node_id, name):
            pass

        def add_node(self, node_id, name=None):
            pass

        def add_link(self, source_id, target_id, weight=1.0):
            pass

        def run(self):
            pass

    monkeypatch.setattr(facade, "Infomap", FakeInfomap)
    graph = ig.Graph(edges=[(0, 1)], directed=False)

    clustering = infomap.find_igraph_communities(graph, trials=1)

    assert isinstance(clustering, ig.VertexClustering)
    assert len(clustering.membership) == graph.vcount()
    assert all(module_id is not None for module_id in clustering.membership)
    assert clustering.membership == [0, 0]


def test_from_igraph_meta_attribute_engages():
    ig = pytest.importorskip("igraph")
    links = [(0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3), (2, 3)]
    g = ig.Graph(edges=links, directed=False)
    g.vs["ct"] = ["a", "b", "a", "a", "b", "b"]  # crossing the two triangles
    result = infomap.run(
        infomap.Network.from_igraph(g, meta_attribute="ct"),
        silent=True, num_trials=5, seed=1, meta_data_rate=1.0,
    )
    assert result.meta_codelength > 0


def test_from_igraph_meta_attribute_typo_raises():
    ig = pytest.importorskip("igraph")
    g = ig.Graph(edges=[(0, 1), (1, 2)], directed=False)
    with pytest.raises(ValueError, match="does not exist"):
        infomap.Network.from_igraph(g, meta_attribute="missing")


def test_find_igraph_communities_rejects_trial_and_vertex_weight_conflicts():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1)], directed=False)

    with pytest.raises(ValueError, match="trials.*num_trials"):
        infomap.find_igraph_communities(graph, trials=1, num_trials=2)

    with pytest.raises(ValueError, match="vertex_weights"):
        infomap.find_igraph_communities(graph, vertex_weights=[1, 1])


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_find_igraph_communities_accepts_num_trials_alone(monkeypatch):
    # `num_trials` (the engine option name) is accepted on its own, matching
    # the networkx helper; only passing it *together* with `trials` conflicts.
    ig = pytest.importorskip("igraph")
    captured = {}
    real_infomap = facade.Infomap

    class RecordingInfomap(real_infomap):
        def __init__(self, *args, **options):
            captured.update(options)
            super().__init__(*args, **options)

    monkeypatch.setattr(facade, "Infomap", RecordingInfomap)
    infomap.find_igraph_communities(ig.Graph(edges=[(0, 1)]), num_trials=4, seed=1)
    assert captured["num_trials"] == 4


class _Recorder:
    flowModelIsSet = True

    @property
    def _core(self):
        return self

    def set_name(self, node_id, name):
        pass

    def add_node(self, node_id, name=None):
        pass

    def add_link(self, source_id, target_id, weight=1.0):
        pass
