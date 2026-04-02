from infomap import Infomap
from operator import itemgetter
import networkx as nx
import pytest


def test_iter_physical_on_physical():
    im = Infomap(num_trials=10, silent=True)
    im.read_file("examples/networks/twotriangles.net")
    im.run()

    modules = sorted([(node.node_id, node.module_id) for node in im.physical_nodes], key=itemgetter(0))
    assert modules == [(1, 1), (2, 1), (3, 1), (4, 2), (5, 2), (6, 2)]


def test_iter_physical_on_states():
    im = Infomap(num_trials=10, silent=True)
    im.read_file("examples/networks/states.net")
    im.run()

    modules = sorted([(node.state_id, node.node_id, node.module_id) for node in im.physical_nodes], key=itemgetter(0))
    assert modules == [(1, 1, 1), (2, 2, 1), (3, 3, 1), (4, 1, 2), (5, 4, 2), (6, 5, 2)]


def test_iter_physical_reliability():

    for _ in range(100):
        im = Infomap(num_trials=10, silent=True)
        im.read_file("examples/networks/states.net")
        im.run()

        modules = [(node.node_id, node.module_id) for node in im.physical_nodes]
        assert modules == [(1, 1), (2, 1), (3, 1), (1, 2), (4, 2), (5, 2)]


def test_multilevel_modules_on_states():
    im = Infomap(silent=True)
    im.read_file("examples/networks/states.net")
    im.run()
    modules = [(node, modules) for node, modules in im.get_multilevel_modules(states=True).items()]
    assert modules == [(1, (1,)), (2, (1,)), (3, (1,)), (4, (2,)), (5, (2,)), (6, (2,))]


def test_multilevel_modules_on_physical():
    im = Infomap(silent=True)
    im.read_file("examples/networks/states.net")
    im.run()
    # RuntimeError: Cannot get multilevel modules on higher-order network without states.
    with pytest.raises(RuntimeError):
        im.get_multilevel_modules(states=False)


def test_iter_nodes_with_no_infomap_on_networkx_graph():
    graph = nx.Graph()
    graph.add_edges_from([(1, 2), (1, 3), (2, 3), (3, 4), (4, 5)])

    im = Infomap(silent=True, no_infomap=True)
    im.add_networkx_graph(graph)
    im.run()

    nodes = [(node.node_id, node.data.flow) for node in im.nodes]
    assert nodes == [
        (3, 0.30000000000000004),
        (1, 0.2),
        (2, 0.2),
        (4, 0.2),
        (5, 0.1),
    ]


if __name__ == "__main__":
    test_iter_physical_on_physical()
    test_iter_physical_on_states()
    test_iter_physical_reliability()
    test_multilevel_modules_on_states()
