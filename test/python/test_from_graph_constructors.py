"""Tests for the from_networkx/from_igraph constructors and node_id_to_label (SP2a)."""

import networkx as nx
import pytest

from infomap import Infomap


@pytest.mark.fast
def test_node_id_to_label_initialized_empty():
    im = Infomap(silent=True, no_file_output=True)
    assert im.node_id_to_label == {}


@pytest.mark.fast
def test_add_networkx_graph_sets_node_id_to_label():
    im = Infomap(silent=True, no_file_output=True)
    G = nx.Graph([("a", "b"), ("a", "c")])
    mapping = im.add_networkx_graph(G)
    assert mapping == {0: "a", 1: "b", 2: "c"}  # still returned (unchanged)
    assert im.node_id_to_label == mapping  # and now stored


@pytest.mark.fast
def test_from_networkx_matches_manual_build():
    G = nx.Graph([("a", "b"), ("a", "c"), ("b", "c")])
    a = Infomap(silent=True, no_file_output=True, seed=42)
    a.add_networkx_graph(G)
    a.run()
    b = Infomap.from_networkx(G, silent=True, no_file_output=True, seed=42)
    b.run()
    assert b.codelength == pytest.approx(a.codelength)
    assert b.node_id_to_label == a.node_id_to_label == {0: "a", 1: "b", 2: "c"}


@pytest.mark.fast
def test_from_networkx_passes_options():
    G = nx.Graph([("a", "b"), ("b", "c")])
    im = Infomap.from_networkx(G, num_trials=5, silent=True, no_file_output=True)
    im.run()
    assert len(im.codelengths) == 5  # num_trials forwarded through the constructor


@pytest.mark.fast
def test_from_igraph_matches_manual_build():
    ig = pytest.importorskip("igraph")
    g = ig.Graph.Formula("a-b, a-c, b-c")
    a = Infomap(silent=True, no_file_output=True, seed=42)
    a.add_igraph_graph(g)
    a.run()
    b = Infomap.from_igraph(g, silent=True, no_file_output=True, seed=42)
    b.run()
    assert b.codelength == pytest.approx(a.codelength)
    assert b.node_id_to_label == a.node_id_to_label
