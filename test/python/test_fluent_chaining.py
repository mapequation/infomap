"""Fluent-chaining: mutators return self so building can be chained (SP2bd)."""

import pytest

from infomap import Infomap


@pytest.mark.fast
def test_single_layer_mutators_return_self():
    im = Infomap(silent=True, no_file_output=True)
    assert im.add_node(1) is im
    assert im.add_nodes([2, 3]) is im
    assert im.add_state_node(10, 1) is im
    assert im.add_state_nodes([(11, 2)]) is im
    assert im.set_name(1, "a") is im
    assert im.set_names({2: "b"}) is im
    assert im.add_link(1, 2) is im
    assert im.add_links([(2, 3)]) is im
    assert im.set_meta_data(1, 0) is im
    assert im.remove_link(2, 3) is im
    assert im.remove_links([(1, 2)]) is im


@pytest.mark.fast
def test_multilayer_mutators_return_self():
    a = Infomap(silent=True, no_file_output=True)
    assert a.add_multilayer_intra_link(1, 1, 2) is a
    assert a.add_multilayer_intra_links([(1, 2, 3)]) is a
    assert a.add_multilayer_inter_link(1, 1, 2) is a
    assert a.add_multilayer_inter_links([(2, 1, 1)]) is a
    b = Infomap(silent=True, no_file_output=True)
    assert b.add_multilayer_link((0, 1), (1, 2)) is b
    assert b.add_multilayer_links([((0, 3), (1, 2))]) is b


@pytest.mark.fast
def test_fluent_chain_builds_and_runs():
    im = (
        Infomap(silent=True, no_file_output=True, seed=1)
        .add_links([(1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 6), (6, 4)])
        .add_node(7, "isolated-ish")
        .set_name(1, "a")
    )
    assert isinstance(im, Infomap)
    im.run()
    assert im.num_top_modules >= 1
    assert im.get_name(1) == "a"
