import pytest

# These tests build and run multilayer networks through the stateful Infomap
# compat surface (no_infomap / matchable_multilayer_ids), so the pending
# deprecation on advanced-tier kwargs is expected here; it is asserted in
# test_deprecations.py.
pytestmark = pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")


def test_matchable_multilayer_ids_with_python_read_file(
    make_infomap, example_network_path
):
    im = make_infomap(no_infomap=True, matchable_multilayer_ids=3)
    im.read_file(str(example_network_path("multilayer.net")))
    im.run()

    nodes = sorted((node.state_id, node.node_id, node.layer_id) for node in im.nodes)

    assert im.num_nodes == 6
    assert im.num_physical_nodes == 5
    assert nodes == [
        (9, 1, 1),
        (10, 1, 2),
        (18, 2, 2),
        (26, 3, 2),
        (33, 4, 1),
        (41, 5, 1),
    ]


def _build_multilayer(make_infomap):
    im = make_infomap(no_infomap=True, silent=True)
    im.add_multilayer_intra_link(1, 1, 4, 1.0)
    im.add_multilayer_intra_link(1, 1, 5, 1.0)
    im.add_multilayer_intra_link(1, 4, 5, 1.0)
    im.add_multilayer_intra_link(2, 1, 2, 1.0)
    im.add_multilayer_intra_link(2, 1, 3, 1.0)
    im.add_multilayer_intra_link(2, 2, 3, 1.0)
    im.add_multilayer_inter_link(1, 1, 2, 0.2)
    return im


def test_initial_partition_accepts_physical_multilayer_keys(make_infomap):
    from infomap import MultilayerNode

    # Discover the generated state ids once.
    probe = _build_multilayer(make_infomap)
    probe.run(no_infomap=True)
    physical_to_state = {
        (node.layer_id, node.node_id): node.state_id for node in probe.nodes
    }
    # Group each layer into its own module by physical identity.
    physical_partition = {
        (layer_id, node_id): (0 if layer_id == 1 else 1)
        for (layer_id, node_id) in physical_to_state
    }
    state_partition = {
        physical_to_state[key]: module for key, module in physical_partition.items()
    }

    state_im = _build_multilayer(make_infomap)
    state_im.initial_partition = state_partition
    state_im.run(no_infomap=True)

    physical_im = _build_multilayer(make_infomap)
    physical_im.initial_partition = physical_partition
    physical_im.run(no_infomap=True)

    assert physical_im.codelength == pytest.approx(state_im.codelength)
    assert physical_im.num_top_modules == state_im.num_top_modules

    # MultilayerNode keys are also accepted.
    node_im = _build_multilayer(make_infomap)
    node_im.initial_partition = {
        MultilayerNode(layer_id=layer_id, node_id=node_id): module
        for (layer_id, node_id), module in physical_partition.items()
    }
    node_im.run(no_infomap=True)
    assert node_im.codelength == pytest.approx(state_im.codelength)


def test_initial_partition_physical_unknown_key_raises(make_infomap):
    im = _build_multilayer(make_infomap)
    im.initial_partition = {(9, 9): 0}
    with pytest.raises((ValueError, RuntimeError)):
        im.run(no_infomap=True)


def test_initial_partition_physical_bad_tuple_shape_raises(make_infomap):
    im = _build_multilayer(make_infomap)
    with pytest.raises(ValueError):
        im.initial_partition = {(1, 1, 1): 0}
