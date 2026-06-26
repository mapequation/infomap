"""Fluent chaining lives on Network; Infomap mutators return the released value.

``Network`` mutators return ``self`` so building can be chained (SP2bd).
``Infomap`` mutators must NOT return ``self`` -- they mirror the value the
released (origin/master) Python API returned, which is the underlying SWIG
call's result (an id / bool / None), routed through the shared ``Core``.
"""

import pytest

from infomap import Infomap, Network


# --- Network is fluent: every mutator returns self -----------------------------


@pytest.mark.fast
def test_network_single_layer_mutators_return_self():
    net = Network()
    assert net.add_node(1) is net
    assert net.add_nodes([2, 3]) is net
    assert net.add_state_node(10, 1) is net
    assert net.add_state_nodes([(11, 2)]) is net
    assert net.set_name(1, "a") is net
    assert net.set_names({2: "b"}) is net
    assert net.add_link(1, 2) is net
    assert net.add_links([(2, 3)]) is net
    assert net.set_meta_data(1, 0) is net
    assert net.remove_link(2, 3) is net
    assert net.remove_links([(1, 2)]) is net


@pytest.mark.fast
def test_network_multilayer_mutators_return_self():
    a = Network()
    assert a.add_multilayer_intra_link(1, 1, 2) is a
    assert a.add_multilayer_intra_links([(1, 2, 3)]) is a
    assert a.add_multilayer_inter_link(1, 1, 2) is a
    assert a.add_multilayer_inter_links([(2, 1, 1)]) is a
    b = Network()
    assert b.add_multilayer_link((0, 1), (1, 2)) is b
    assert b.add_multilayer_links([((0, 3), (1, 2))]) is b


@pytest.mark.fast
def test_network_fluent_chain_builds_and_runs():
    net = (
        Network()
        .add_links([(1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 6), (6, 4)])
        .add_node(7, "isolated-ish")
        .set_name(1, "a")
    )
    assert isinstance(net, Network)
    result = net.run(options={"silent": True, "no_file_output": True, "seed": 1})
    assert result.num_top_modules >= 1


# --- Infomap mutators return the released value, NOT self ----------------------


@pytest.mark.fast
def test_infomap_single_layer_mutators_do_not_return_self():
    im = Infomap(silent=True, no_file_output=True)
    assert im.add_node(1) is not im
    assert im.add_nodes([2, 3]) is not im
    assert im.add_state_node(10, 1) is not im
    assert im.add_state_nodes([(11, 2)]) is not im
    assert im.set_name(1, "a") is not im
    assert im.set_names({2: "b"}) is not im
    assert im.add_link(1, 2) is not im
    assert im.add_links([(2, 3)]) is not im
    assert im.set_meta_data(1, 0) is not im
    assert im.remove_link(2, 3) is not im
    assert im.remove_links([(1, 2)]) is not im


@pytest.mark.fast
def test_infomap_multilayer_mutators_do_not_return_self():
    a = Infomap(silent=True, no_file_output=True)
    assert a.add_multilayer_intra_link(1, 1, 2) is not a
    assert a.add_multilayer_intra_links([(1, 2, 3)]) is not a
    assert a.add_multilayer_inter_link(1, 1, 2) is not a
    assert a.add_multilayer_inter_links([(2, 1, 1)]) is not a
    b = Infomap(silent=True, no_file_output=True)
    assert b.add_multilayer_link((0, 1), (1, 2)) is not b
    assert b.add_multilayer_links([((0, 3), (1, 2))]) is not b


@pytest.mark.fast
def test_infomap_mutators_match_released_core_returns():
    # The released API returned the underlying SWIG call's result, routed now
    # through the shared Core. add_node returns whatever the binding returns
    # (an id or None), never self.
    im = Infomap(silent=True, no_file_output=True)
    assert not isinstance(im.add_node(3), Infomap)
    # remove_link mirrors network().removeLink, a bool: True if a link existed.
    im.add_link(3, 4)
    assert im.remove_link(3, 4) is True
    assert im.remove_link(99, 100) is False


@pytest.mark.fast
def test_infomap_builds_and_runs_without_chaining():
    im = Infomap(silent=True, no_file_output=True, seed=1)
    im.add_links([(1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 6), (6, 4)])
    im.add_node(7, "isolated-ish")
    im.set_name(1, "a")
    im.run()
    assert im.num_top_modules >= 1
    assert im.get_name(1) == "a"
