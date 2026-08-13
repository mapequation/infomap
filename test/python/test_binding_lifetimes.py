"""Objects handed across the SWIG boundary must not outlive what they point at.

Two kinds did: iterator positions collected from a physical tree pointed into per-iterator
storage the source dropped a step later, and the network accessors handed out a non-owning
proxy with no tie to its owner. Both read freed heap from ordinary code -- silently when
the page still held the old values, which is how a partition full of zeros reached a user
about to publish it (#900).
"""

from __future__ import annotations

import gc
import weakref

import infomap
import pytest

pytestmark = pytest.mark.fast

STATE_NETWORK = "test/fixtures/networks/states.net"


def _state_run():
    im = infomap.Infomap(silent=True, seed=7)
    im.read_file(STATE_NETWORK)
    im.run()
    return im


def test_collected_physical_tree_positions_match_eager_reads():
    """list(...) is the natural idiom and used to yield zeros on a higher-order network."""
    im = _state_run()

    collected = [
        (node.node_id, node.flow)
        for node in list(im.get_tree(states=False))
        if node.is_leaf
    ]
    eager = [
        (node.node_id, node.flow) for node in im.get_tree(states=False) if node.is_leaf
    ]

    assert collected == eager
    assert [node_id for node_id, _ in collected] == [1, 2, 3, 1, 4, 5]
    assert all(flow > 0 for _, flow in collected)


def test_collected_physical_nodes_match_eager_reads():
    """The deprecated accessor filtered the tree and handed out the live cursor itself."""
    im = _state_run()

    collected = [(node.node_id, node.flow) for node in list(im.physical_nodes)]
    eager = [(node.node_id, node.flow) for node in im.physical_nodes]

    assert collected == eager
    assert [node_id for node_id, _ in collected] == [1, 2, 3, 1, 4, 5]


def test_sorting_collected_positions_sees_real_flows():
    """Sorting on a collected position's flow sorted on garbage keys."""
    im = _state_run()

    by_flow = sorted(
        (node for node in list(im.get_tree(states=False)) if node.is_leaf),
        key=lambda node: -node.flow,
    )

    assert all(node.flow > 0 for node in by_flow)
    assert {node.node_id for node in by_flow} == {1, 2, 3, 4, 5}


def test_network_proxy_keeps_its_owner_alive():
    """A helper that returns im.network and drops the instance is ordinary code."""
    core_ref = None

    def helper():
        nonlocal core_ref
        im = infomap.Infomap(silent=True)
        im.add_links(((1, 2), (2, 3), (3, 1)))
        im.run()
        core_ref = weakref.ref(im._core)
        return im.network

    network = helper()
    gc.collect()

    assert core_ref() is not None, (
        "the owner was collected while its proxy was still held"
    )
    assert network.numNodes() == 3
    assert network.numLinks() == 3


def test_network_proxy_from_the_network_surface_keeps_its_owner():
    core_ref = None

    def helper():
        nonlocal core_ref
        net = infomap.Network()
        net.add_links(((1, 2), (2, 3)))
        core_ref = weakref.ref(net._core)
        return net.network

    network = helper()
    gc.collect()

    assert core_ref() is not None
    assert network.numNodes() == 3
