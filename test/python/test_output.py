from __future__ import annotations

import json

import pytest

pytestmark = pytest.mark.fast


def _run_small_network(make_infomap, load_graph_fixture):
    im = make_infomap()
    load_graph_fixture(im, "twotriangles_unweighted.edges", method="add_link")
    im.run()
    return im


def test_output_writers_are_stable_across_repeat_calls(
    make_infomap, load_graph_fixture, output_dir, validate_json_schema
):
    im = _run_small_network(make_infomap, load_graph_fixture)

    tree_a = output_dir / "first.tree"
    tree_b = output_dir / "second.tree"
    json_a = output_dir / "first.json"
    json_b = output_dir / "second.json"
    clu_a = output_dir / "first.clu"
    clu_b = output_dir / "second.clu"

    im.write_tree(str(tree_a))
    im.write_tree(str(tree_b))
    im.write_json(str(json_a))
    im.write_json(str(json_b))
    im.write_clu(str(clu_a))
    im.write_clu(str(clu_b))

    assert tree_a.read_text() == tree_b.read_text()
    assert json_a.read_text() == json_b.read_text()
    assert clu_a.read_text() == clu_b.read_text()

    parsed = json.loads(json_a.read_text())
    validate_json_schema(parsed, "partition-output.schema.json")
    assert parsed["version"].startswith("v")
    assert "nodes" in parsed
    assert "modules" in parsed
    assert "# path flow name node_id" in tree_a.read_text()
    assert "# module level 1" in clu_a.read_text()


def test_state_output_writers_include_state_columns(
    make_infomap, network_fixture_path, output_dir, validate_json_schema
):
    im = make_infomap()
    im.read_file(str(network_fixture_path("states.net")))
    im.run()

    tree_path = output_dir / "states.tree"
    json_path = output_dir / "states.json"
    clu_path = output_dir / "states.clu"

    im.write_tree(str(tree_path), states=True)
    im.write_json(str(json_path), states=True)
    im.write_clu(str(clu_path), states=True)

    tree_text = tree_path.read_text()
    json_text = json_path.read_text()
    clu_text = clu_path.read_text()

    assert "# path flow name state_id node_id" in tree_text
    assert "# state_id module flow node_id" in clu_text

    parsed = json.loads(json_text)
    validate_json_schema(parsed, "partition-output.schema.json")
    assert parsed["stateLevel"] is True
    assert parsed["higherOrder"] is True


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_json_metadata_values_remain_strings(
    make_infomap, output_dir, validate_json_schema
):
    im = make_infomap()
    im.add_links(((1, 2), (1, 3), (2, 3), (3, 4), (4, 5), (4, 6), (5, 6)))
    for node_id, category in {
        1: 10,
        2: 10,
        3: 20,
    }.items():
        im.set_meta_data(node_id, category)

    im.run(meta_data_rate=0)

    json_path = output_dir / "metadata.json"
    im.write_json(str(json_path))
    parsed = json.loads(json_path.read_text())
    validate_json_schema(parsed, "partition-output.schema.json")

    metadata_nodes = [node for node in parsed["nodes"] if "metadata" in node]
    assert metadata_nodes
    assert {node["metadata"]["0"] for node in metadata_nodes} == {"10", "20"}
    assert all(isinstance(node["metadata"]["0"], str) for node in metadata_nodes)
    assert all(node["metadata"] for node in metadata_nodes)
    assert any("metadata" not in node for node in parsed["nodes"])


@pytest.mark.parametrize(
    "network_name,states_output",
    [
        ("ninetriangles.net", False),
        ("states.net", False),
        ("states.net", True),
        ("multilayer.net", True),
    ],
    ids=[
        "physical",
        "states_physical_tree",
        "states_state_tree",
        "multilayer_state_tree",
    ],
)
def test_tree_cluster_data_roundtrip(
    make_infomap, example_network_path, output_dir, network_name, states_output
):
    network_path = str(example_network_path(network_name))

    baseline = make_infomap(num_trials=5)
    baseline.read_file(network_path)
    baseline.run()
    expected_codelength = baseline.codelength

    tree_path = output_dir / f"{network_name}.{int(states_output)}.tree"
    baseline.write_tree(str(tree_path), states=states_output)

    # --no-infomap path: codelength should match exactly.
    no_infomap = make_infomap(no_infomap=True, cluster_data=str(tree_path))
    no_infomap.read_file(network_path)
    no_infomap.run()
    assert no_infomap.codelength == pytest.approx(expected_codelength, abs=1e-9)

    # Continuing optimization: codelength must not regress, no crashes/leaks.
    continued = make_infomap(cluster_data=str(tree_path))
    continued.read_file(network_path)
    continued.run()
    assert continued.codelength <= expected_codelength + 1e-9


def test_writers_accept_pathlike_filenames(
    make_infomap, load_graph_fixture, output_dir
):
    im = _run_small_network(make_infomap, load_graph_fixture)

    tree_path = output_dir / "pathlike.tree"
    clu_path = output_dir / "pathlike.clu"

    im.write_tree(tree_path)
    im.write_clu(clu_path)
    im.write(output_dir / "pathlike2.tree")

    assert tree_path.is_file()
    assert clu_path.is_file()
    assert (output_dir / "pathlike2.tree").is_file()


def test_write_without_extension_raises_value_error(
    make_infomap, load_graph_fixture, output_dir
):
    im = _run_small_network(make_infomap, load_graph_fixture)

    with pytest.raises(ValueError, match="no extension"):
        im.write(str(output_dir / "noext"))


def test_write_with_unknown_extension_raises_not_implemented(
    make_infomap, load_graph_fixture, output_dir
):
    im = _run_small_network(make_infomap, load_graph_fixture)

    with pytest.raises(NotImplementedError, match="bogus"):
        im.write(str(output_dir / "file.bogus"))


def test_infomap_set_meta_data_accepts_bulk_mapping(make_infomap):
    links = [(0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3), (2, 3)]
    im = make_infomap(num_trials=5, seed=1)
    im.add_links(links)
    # Mirrors Network.set_meta_data: the {node: category} bulk form sets
    # per-node metadata in one call (and returns None, not self).
    assert im.set_meta_data({0: 0, 1: 1, 2: 0, 3: 0, 4: 1, 5: 1}) is None
    result = im.run(meta_data_rate=1.0)
    assert result.meta_codelength > 0
