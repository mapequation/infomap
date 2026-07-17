"""Tests for node_id_to_label and the add_*graph adapters (SP2a)."""

import networkx as nx
import pytest
from infomap import Infomap, Network


@pytest.mark.fast
def test_node_id_to_label_initialized_empty():
    im = Infomap(silent=True)
    assert im.node_id_to_label == {}


@pytest.mark.fast
def test_network_node_id_to_label_initialized_empty():
    # A manually built Network exposes an empty mapping instead of raising
    # AttributeError, mirroring Infomap; the graph-library constructors
    # replace it with the id->label mapping they build.
    net = Network()
    assert net.node_id_to_label == {}
    net.add_link(1, 2)
    assert net.node_id_to_label == {}


@pytest.mark.fast
def test_add_networkx_graph_sets_node_id_to_label():
    im = Infomap(silent=True)
    G = nx.Graph([("a", "b"), ("a", "c")])
    mapping = im.add_networkx_graph(G)
    assert mapping == {0: "a", 1: "b", 2: "c"}  # still returned (unchanged)
    assert im.node_id_to_label == mapping  # and now stored


@pytest.mark.fast
def test_add_scipy_sparse_matrix_sets_node_id_to_label():
    import numpy as np

    sparse = pytest.importorskip("scipy.sparse")
    A = sparse.csr_matrix(np.array([[0, 1, 0], [1, 0, 1], [0, 1, 0]]))
    im = Infomap(silent=True)
    mapping = im.add_scipy_sparse_matrix(A)
    assert mapping == {0: 0, 1: 1, 2: 2}  # {internal_id: external_id}
    assert im.node_id_to_label == mapping


@pytest.mark.fast
def test_add_edge_index_sets_node_id_to_label():
    im = Infomap(silent=True)
    mapping = im.add_edge_index([[0, 1, 2], [1, 2, 0]])
    assert mapping == {0: 0, 1: 1, 2: 2}
    assert im.node_id_to_label == mapping
