from operator import itemgetter

import networkx as nx
import pytest


pytestmark = pytest.mark.fast


def test_iter_physical_on_physical(make_infomap, example_network_path):
    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("twotriangles.net")))
    im.run()

    modules = sorted([(node.node_id, node.module_id) for node in im.physical_nodes], key=itemgetter(0))
    assert modules == [(1, 1), (2, 1), (3, 1), (4, 2), (5, 2), (6, 2)]


def test_iter_physical_on_states(make_infomap, example_network_path):
    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("states.net")))
    im.run()

    modules = sorted([(node.state_id, node.node_id, node.module_id) for node in im.physical_nodes], key=itemgetter(0))
    assert modules == [(1, 1, 1), (2, 2, 1), (3, 3, 1), (4, 1, 2), (5, 4, 2), (6, 5, 2)]


def test_iter_physical_reliability(make_infomap, example_network_path):
    for _ in range(100):
        im = make_infomap(num_trials=10)
        im.read_file(str(example_network_path("states.net")))
        im.run()

        modules = [(node.node_id, node.module_id) for node in im.physical_nodes]
        assert modules == [(1, 1), (2, 1), (3, 1), (1, 2), (4, 2), (5, 2)]


def test_multilevel_modules_on_states(make_infomap, example_network_path):
    im = make_infomap()
    im.read_file(str(example_network_path("states.net")))
    im.run()
    modules = [(node, modules) for node, modules in im.get_multilevel_modules(states=True).items()]
    assert modules == [(1, (1,)), (2, (1,)), (3, (1,)), (4, (2,)), (5, (2,)), (6, (2,))]


def test_multilevel_modules_on_physical(make_infomap, example_network_path):
    im = make_infomap()
    im.read_file(str(example_network_path("states.net")))
    im.run()
    with pytest.raises(RuntimeError):
        im.get_multilevel_modules(states=False)


def test_iter_nodes_with_no_infomap_on_networkx_graph(make_infomap):
    graph = nx.Graph()
    graph.add_edges_from([(1, 2), (1, 3), (2, 3), (3, 4), (4, 5)])

    im = make_infomap(no_infomap=True)
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
