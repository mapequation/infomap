import pytest

pytestmark = pytest.mark.fast


TWO_TRIANGLES = [(1, 2), (1, 3), (2, 3), (3, 4), (4, 5), (4, 6), (5, 6)]


def test_add_state_nodes_accepts_pairs_and_mapping(make_infomap):
    pairs = make_infomap()
    pairs.add_state_nodes([(10, 1), (11, 1), (20, 2)])

    mapping = make_infomap()
    mapping.add_state_nodes({10: 1, 11: 1, 20: 2})

    assert pairs.num_nodes == 3
    assert mapping.num_nodes == pairs.num_nodes


def test_json_standard_matches_in_memory(
    make_infomap, network_fixture_path, canonical_modules
):
    """A JSON network yields the same partition as the equivalent in-memory input."""
    in_memory = make_infomap()
    in_memory.add_links(TWO_TRIANGLES)
    in_memory.run()

    from_json = make_infomap()
    from_json.read_file(str(network_fixture_path("json/twotriangles.json")))
    from_json.run()

    assert from_json.num_nodes == in_memory.num_nodes
    assert canonical_modules(from_json.get_modules()) == canonical_modules(
        in_memory.get_modules()
    )


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_json_embedded_meta_matches_set_meta_data(
    make_infomap, network_fixture_path, canonical_modules
):
    """Embedded JSON meta enables coding identically to set_meta_data (Option B)."""
    meta = {1: 0, 2: 0, 3: 1, 4: 1, 5: 0, 6: 0}

    in_memory = make_infomap(num_trials=10)
    in_memory.add_links(TWO_TRIANGLES)
    for node_id, category in meta.items():
        in_memory.set_meta_data(node_id, category)
    in_memory.run(meta_data_rate=2)

    from_json = make_infomap(num_trials=10)
    from_json.read_file(str(network_fixture_path("json/twotriangles_meta.json")))
    from_json.run(meta_data_rate=2)

    assert canonical_modules(from_json.get_modules()) == canonical_modules(
        in_memory.get_modules()
    )


def test_json_embedded_path_seeds_initial_partition(
    make_infomap, network_fixture_path, canonical_modules
):
    """nodes[].path imposes the initial partition; --no-infomap leaves it intact."""
    im = make_infomap()
    im.read_file(str(network_fixture_path("json/twotriangles_paths.json")))
    im.run("--no-infomap")

    assert canonical_modules(im.get_modules()) == [[1, 2], [3, 4], [5, 6]]
