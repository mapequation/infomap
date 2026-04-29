from __future__ import annotations

import json

import pytest


pytestmark = pytest.mark.fast


def _run_small_network(make_infomap, load_graph_fixture):
    im = make_infomap()
    load_graph_fixture(im, "twotriangles_unweighted.edges", method="add_link")
    im.run()
    return im


def test_output_writers_are_stable_across_repeat_calls(make_infomap, load_graph_fixture, output_dir):
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
    assert parsed["version"].startswith("v")
    assert "nodes" in parsed
    assert "modules" in parsed
    assert "# path flow name node_id" in tree_a.read_text()
    assert "# module level 1" in clu_a.read_text()


def test_state_output_writers_include_state_columns(make_infomap, network_fixture_path, output_dir):
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
    assert parsed["stateLevel"] is True
    assert parsed["higherOrder"] is True
