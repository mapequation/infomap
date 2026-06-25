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
