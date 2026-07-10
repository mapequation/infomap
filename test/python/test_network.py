"""Tests for the first-class :class:`infomap.Network` input builder.

Covers:
(a) parity -- a graph built via a standalone ``Network`` produces the same
    run result as the identical graph built via ``Infomap``'s delegated verbs;
(b) fluent mutators return ``self``;
(c) ``num_nodes`` / ``num_links`` report correct counts.

A standalone ``Network`` runs via :meth:`Network.run` or the functional
``infomap.run(net)`` entry point; the parity checks here compare its run result
against the identical graph built and run through ``Infomap``.
"""

import pytest

from infomap import Infomap, MultilayerNode, Network

pytestmark = pytest.mark.fast


_RUN_ARGS = "--silent --no-file-output --seed 123 --num-trials 5"
_LINKS = [(1, 2), (1, 3), (2, 3), (3, 4), (4, 5), (4, 6), (5, 6)]


def test_standalone_network_matches_infomap_delegated_build():
    # Standalone Network: build onto its own default Core, then run that Core.
    net = Network()
    net.add_links(_LINKS)
    net._core.run(_RUN_ARGS)
    net_codelength = net._core.codelength()
    net_modules = net._core.numTopModules()

    # Same graph via Infomap's delegated building verbs, same settings.
    im = Infomap(silent=True, seed=123, num_trials=5)
    im.add_links(_LINKS)
    im.run()

    assert net_codelength == pytest.approx(im.codelength)
    assert net_modules == im.num_top_modules
    assert net.num_nodes == im.num_nodes
    assert net.num_links == im.num_links


def test_multilayer_standalone_matches_infomap_delegated_build():
    intra = [(1, 1, 2), (1, 2, 1), (2, 2, 3), (2, 3, 2)]
    inter = [(1, 2, 2), (2, 2, 1)]

    net = Network()
    net.add_multilayer_intra_links(intra)
    net.add_multilayer_inter_links(inter)
    net._core.run(_RUN_ARGS)

    im = Infomap(silent=True, seed=123, num_trials=5)
    im.add_multilayer_intra_links(intra)
    im.add_multilayer_inter_links(inter)
    im.run()

    assert net._core.codelength() == pytest.approx(im.codelength)
    assert net._core.numTopModules() == im.num_top_modules


def test_mutators_return_self_for_chaining():
    net = Network()
    assert net.add_node(1, "a") is net
    assert net.add_nodes([2, 3]) is net
    assert net.set_name(1, "A") is net
    assert net.set_names({2: "B"}) is net
    assert net.add_link(1, 2) is net
    assert net.add_links([(2, 3)]) is net
    assert net.remove_link(2, 3) is net
    assert net.remove_links([(1, 2)]) is net
    assert net.set_meta_data(1, 0) is net

    ml = Network()
    assert ml.add_multilayer_link((0, 1), (1, 2)) is ml
    assert ml.add_multilayer_links([((0, 3), (1, 2))]) is ml
    assert ml.add_multilayer_intra_link(1, 1, 2) is ml
    assert ml.add_multilayer_intra_links([(1, 2, 3)]) is ml
    assert ml.add_multilayer_inter_link(1, 1, 2) is ml
    assert ml.add_multilayer_inter_links([(2, 1, 1)]) is ml

    states = Network()
    assert states.add_state_node(1, 1) is states
    assert states.add_state_nodes([(2, 1)]) is states


def test_chained_construction_builds_expected_counts():
    net = Network().add_node(1, "a").add_node(2).add_links([(1, 2), (2, 3)])
    assert net.num_nodes == 3
    assert net.num_links == 2


def test_num_nodes_and_num_links_track_additions():
    net = Network()
    assert net.num_nodes == 0
    assert net.num_links == 0
    net.add_links([(1, 2), (2, 3), (3, 1)])
    assert net.num_nodes == 3
    assert net.num_links == 3


def test_state_nodes_produce_higher_order_counts():
    net = Network()
    net.add_state_nodes([(10, 1), (11, 1), (20, 2)])
    assert net.num_nodes == 3


def test_default_constructor_makes_owned_core():
    a = Network()
    b = Network()
    assert a._core is not b._core


def test_shared_core_is_used_when_passed():
    im = Infomap(silent=True)
    net = Network(core=im._core)
    assert net._core is im._core
    net.add_link(1, 2)
    # Building through the shared Network is visible to the owning Infomap.
    assert im.num_links == net.num_links == 1


def test_remove_multilayer_link_raises():
    net = Network()
    with pytest.raises(NotImplementedError):
        net.remove_multilayer_link()


def test_multilayer_node_namedtuple_accepted():
    net = Network()
    src = MultilayerNode(layer_id=0, node_id=1)
    dst = MultilayerNode(layer_id=1, node_id=2)
    assert net.add_multilayer_link(src, dst) is net


def test_set_meta_data_bulk_mapping():
    links = [(0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3), (2, 3)]
    net = Network()
    net.add_links(links)
    # Crossing categories incur a meta cost; proves the {node: category} bulk
    # form sets per-node metadata (and returns self).
    assert net.set_meta_data({0: 0, 1: 1, 2: 0, 3: 0, 4: 1, 5: 1}) is net
    result = net.run(
        options={"silent": True, "num_trials": 5, "seed": 1, "meta_data_rate": 1.0}
    )
    assert result.meta_codelength > 0
