"""Tests for node_id_to_label and the add_*graph adapters (SP2a)."""

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
def test_add_scipy_sparse_matrix_sets_node_id_to_label():
    import numpy as np

    sparse = pytest.importorskip("scipy.sparse")
    A = sparse.csr_matrix(np.array([[0, 1, 0], [1, 0, 1], [0, 1, 0]]))
    im = Infomap(silent=True, no_file_output=True)
    mapping = im.add_scipy_sparse_matrix(A)
    assert mapping  # non-empty {internal_id: external_id}
    assert im.node_id_to_label == mapping


@pytest.mark.fast
def test_add_edge_index_sets_node_id_to_label():
    im = Infomap(silent=True, no_file_output=True)
    mapping = im.add_edge_index([[0, 1, 2], [1, 2, 0]])
    assert mapping
    assert im.node_id_to_label == mapping
