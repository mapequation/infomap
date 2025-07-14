import numpy as np
import networkx as nx
from infomap import Infomap


def create_G(int_type):
    G = nx.Graph()
    G.add_edge(int_type(1), int_type(2))
    return G


def check_node_map(node_map):
    assert 0 not in node_map
    assert len(node_map) == 2
    assert node_map[1] == 1
    assert node_map[2] == 2


def test_support_different_integer_types():
    G = create_G(np.int64)
    im = Infomap(silent=True)
    node_map = im.add_networkx_graph(G)
    check_node_map(node_map)
