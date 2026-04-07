from operator import itemgetter
import os
from pathlib import Path
import subprocess
import sys

import networkx as nx
import pytest

import infomap as infomap_module


pytestmark = pytest.mark.fast


def test_run_with_empty_initial_partition_overrides_persisted_partition(make_infomap):
    baseline = make_infomap()
    baseline.add_links([(1, 2), (1, 3), (2, 3), (2, 4)])
    baseline.run(no_infomap=True)

    im = make_infomap()
    im.add_links([(1, 2), (1, 3), (2, 3), (2, 4)])
    im.initial_partition = {1: 0, 2: 0, 3: 1, 4: 1}

    im.run(no_infomap=True, initial_partition={})

    assert im.codelength == pytest.approx(baseline.codelength)
    assert dict(im.initial_partition) == {1: 0, 2: 0, 3: 1, 4: 1}


def test_add_networkx_state_graph_uses_stable_first_seen_phys_ids(make_infomap):
    graph = nx.Graph()
    graph.add_node("state-b", phys_id="beta")
    graph.add_node("state-a", phys_id="alpha")
    graph.add_node("state-b-2", phys_id="beta")
    graph.add_edge("state-b", "state-a")
    graph.add_edge("state-a", "state-b-2")

    im = make_infomap(no_infomap=True)
    mapping = im.add_networkx_graph(graph)
    im.run()

    nodes = sorted(
        [(node.state_id, node.node_id, node.module_id) for node in im.nodes],
        key=itemgetter(0),
    )

    assert mapping == {0: "state-b", 1: "state-a", 2: "state-b-2"}
    assert dict(im.names) == {0: "beta", 1: "alpha"}
    assert nodes == [(0, 0, 1), (1, 1, 1), (2, 0, 1)]


def test_add_networkx_multilayer_graph_with_string_ids(make_infomap):
    graph = nx.Graph()
    graph.add_node("state-b-layer-1", phys_id="beta", layer_id=1)
    graph.add_node("state-a-layer-1", phys_id="alpha", layer_id=1)
    graph.add_node("state-a-layer-2", phys_id="alpha", layer_id=2)
    graph.add_edge("state-b-layer-1", "state-a-layer-1", weight=2.0)
    graph.add_edge("state-a-layer-1", "state-a-layer-2")

    im = make_infomap(no_infomap=True)
    mapping = im.add_networkx_graph(graph)
    im.run()

    nodes = sorted(
        [(node.state_id, node.node_id, node.layer_id) for node in im.nodes],
        key=itemgetter(0),
    )

    assert mapping == {
        0: "state-b-layer-1",
        1: "state-a-layer-1",
        2: "state-a-layer-2",
    }
    assert dict(im.names) == {0: "beta", 1: "alpha"}
    assert nodes == [(0, 0, 1), (1, 1, 1), (2, 1, 2)]


def test_get_dataframe_supports_states_and_depth_level(make_infomap, example_network_path):
    pytest.importorskip("pandas")

    states_im = make_infomap(num_trials=10)
    states_im.read_file(str(example_network_path("states.net")))
    states_im.run()

    physical_df = states_im.get_dataframe(
        columns=["node_id", "module_id"],
        states=False,
    )
    expected_physical = [
        (node.node_id, node.module_id)
        for node in states_im.get_nodes(depth_level=1, states=False)
    ]
    assert list(physical_df.itertuples(index=False, name=None)) == expected_physical

    hierarchical_im = make_infomap(num_trials=10)
    hierarchical_im.read_file(str(example_network_path("ninetriangles.net")))
    hierarchical_im.run()

    depth_df = hierarchical_im.get_dataframe(
        columns=["node_id", "module_id"],
        states=False,
        depth_level=2,
    )
    expected_depth = [
        (node.node_id, node.module_id)
        for node in hierarchical_im.get_nodes(depth_level=2, states=False)
    ]
    assert list(depth_df.itertuples(index=False, name=None)) == expected_depth


def test_infomap_options_to_args_matches_construct_args():
    options = infomap_module.InfomapOptions(
        no_infomap=True,
        no_file_output=True,
        output=["json", "tree"],
        recorded_teleportation=True,
        variable_markov_time=True,
        variable_markov_damping=0.42,
        verbosity_level=3,
    )

    rendered = options.to_args(base_args="--existing")
    expected = infomap_module._construct_args(
        args="--existing",
        no_infomap=True,
        no_file_output=True,
        output=["json", "tree"],
        recorded_teleportation=True,
        variable_markov_time=True,
        variable_markov_damping=0.42,
        verbosity_level=3,
    )

    assert rendered == expected


def test_from_options_uses_package_construct_args(monkeypatch):
    captured = {}

    def fake_construct_args(args=None, **kwargs):
        captured["args"] = args
        captured["kwargs"] = kwargs
        return "--silent"

    def fake_init(self, args):
        captured["rendered_args"] = args

    monkeypatch.setattr(infomap_module, "_construct_args", fake_construct_args)
    monkeypatch.setattr(infomap_module.InfomapWrapper, "__init__", fake_init)

    options = infomap_module.InfomapOptions(
        silent=True,
        no_file_output=True,
        num_trials=7,
    )
    im = infomap_module.Infomap.from_options(options, args="--existing")

    assert isinstance(im, infomap_module.Infomap)
    assert captured["args"] == "--existing"
    assert captured["rendered_args"] == "--silent"
    assert captured["kwargs"]["silent"] is True
    assert captured["kwargs"]["no_file_output"] is True
    assert captured["kwargs"]["num_trials"] == 7


def test_run_with_options_forwards_to_run(monkeypatch):
    captured = {}
    im = infomap_module.Infomap(silent=True, no_file_output=True)

    def fake_run(self, **kwargs):
        captured["kwargs"] = kwargs
        return "ok"

    monkeypatch.setattr(infomap_module.Infomap, "run", fake_run)

    options = infomap_module.InfomapOptions(variable_markov_time=True, num_trials=5)
    result = im.run_with_options(
        options,
        args="--existing",
        initial_partition={1: 1},
    )

    assert result == "ok"
    assert captured["kwargs"]["args"] == "--existing"
    assert captured["kwargs"]["initial_partition"] == {1: 1}
    assert captured["kwargs"]["variable_markov_time"] is True
    assert captured["kwargs"]["num_trials"] == 5


def test_main_preserves_cli_argument_boundaries(example_network_path, tmp_path):
    network_path = tmp_path / "with space.net"
    network_path.write_text(example_network_path("twotriangles.net").read_text())

    repo_root = Path(__file__).resolve().parents[2]
    env = dict(**os.environ)
    existing_pythonpath = env.get("PYTHONPATH")
    src_path = str(repo_root / "interfaces" / "python" / "src")
    env["PYTHONPATH"] = (
        src_path if not existing_pythonpath else f"{src_path}{os.pathsep}{existing_pythonpath}"
    )

    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "infomap",
            str(network_path),
            "--silent",
            "--no-file-output",
        ],
        capture_output=True,
        text=True,
        env=env,
    )

    assert result.returncode == 0, result.stderr


def test_config_hides_unusable_raw_members():
    conf = infomap_module.Config("", False)

    for attr in ("startDate", "additionalInput", "parsedOptions"):
        assert not hasattr(conf, attr)
