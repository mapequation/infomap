import pytest


pytestmark = pytest.mark.fast


def test_add_state_nodes_accepts_pairs_and_mapping(make_infomap):
    pairs = make_infomap()
    pairs.add_state_nodes([(10, 1), (11, 1), (20, 2)])

    mapping = make_infomap()
    mapping.add_state_nodes({10: 1, 11: 1, 20: 2})

    assert pairs.num_nodes == 3
    assert mapping.num_nodes == pairs.num_nodes
