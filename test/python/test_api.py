import importlib.resources
from operator import itemgetter
import tomllib

import networkx as nx
import pytest

import infomap as infomap_module
import infomap._summary as summary_module
import infomap._results as results_module
from infomap._bindings import InfomapWrapper


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


def test_add_networkx_state_graph_uses_stable_first_seen_node_ids(make_infomap):
    graph = nx.Graph()
    graph.add_node("state-b", node_id="beta")
    graph.add_node("state-a", node_id="alpha")
    graph.add_node("state-b-2", node_id="beta")
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


def test_elapsed_time_is_exposed_after_run(make_infomap):
    im = make_infomap(no_infomap=True)
    im.add_links([(1, 2), (2, 3), (3, 1)])
    im.run()

    assert isinstance(im.elapsed_time, float)
    assert im.elapsed_time >= 0.0


def test_add_networkx_multilayer_graph_with_string_ids(make_infomap):
    graph = nx.Graph()
    graph.add_node("state-b-layer-1", node_id="beta", layer_id=1)
    graph.add_node("state-a-layer-1", node_id="alpha", layer_id=1)
    graph.add_node("state-a-layer-2", node_id="alpha", layer_id=2)
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


def test_get_dataframe_supports_states_and_depth_level(
    make_infomap, example_network_path
):
    pytest.importorskip("pandas")

    states_im = make_infomap(num_trials=10)
    states_im.read_file(str(example_network_path("states.net")))
    states_im.run()

    with pytest.deprecated_call(match="get_dataframe.*to_dataframe"):
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

    with pytest.deprecated_call(match="get_dataframe.*to_dataframe"):
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


def test_to_dataframe_alias_matches_get_dataframe(make_infomap, example_network_path):
    pytest.importorskip("pandas")

    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("twotriangles.net")))
    im.run()

    columns = ["node_id", "module_id"]
    with pytest.deprecated_call(match="get_dataframe.*to_dataframe"):
        expected = im.get_dataframe(columns=columns, states=False)
    actual = im.to_dataframe(columns=columns, states=False)

    assert actual.equals(expected)


def test_to_dataframe_uses_pandas_friendly_defaults(make_infomap, example_network_path):
    pytest.importorskip("pandas")

    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("twotriangles.net")))
    im.run()

    dataframe = im.to_dataframe()

    assert list(dataframe.columns) == ["node_id", "module_id", "flow", "path", "name"]
    assert set(dataframe["node_id"]) == {1, 2, 3, 4, 5, 6}
    assert set(dataframe["module_id"]) == {1, 2}


def test_to_dataframe_supports_index_sort_and_community_alias(
    make_infomap, example_network_path
):
    pytest.importorskip("pandas")

    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("twotriangles.net")))
    im.run()

    dataframe = im.to_dataframe(
        columns=["node_id", "community", "flow"],
        index="node_id",
        sort=["community", "flow"],
    )

    assert dataframe.index.name == "node_id"
    assert list(dataframe.columns) == ["community", "flow"]
    assert list(dataframe["community"]) == sorted(dataframe["community"])


def test_to_dataframe_caches_names_for_name_column(
    monkeypatch, make_infomap, example_network_path
):
    pytest.importorskip("pandas")

    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("twotriangles.net")))
    im.run()
    names = dict(im._get_names_impl())
    access_count = 0

    def fake_names(self):
        nonlocal access_count
        access_count += 1
        return names

    # to_dataframe reads names through the internal, non-deprecated helper.
    monkeypatch.setattr(type(im), "_get_names_impl", fake_names)

    with pytest.deprecated_call():
        dataframe = im.to_dataframe(columns=["node_id", "name"])

    assert access_count == 1
    assert set(dataframe["name"]) == {"A", "B", "C", "D", "E", "F"}


def test_to_dataframe_supports_true_sort_and_depth_level_alias(
    make_infomap, example_network_path
):
    pytest.importorskip("pandas")

    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("ninetriangles.net")))
    im.run()

    dataframe = im.to_dataframe(
        columns=["node_id", "module_id"],
        states=False,
        depth_level=2,
        sort=True,
    )
    with pytest.deprecated_call(match="get_dataframe.*to_dataframe"):
        expected = (
            im.get_dataframe(
                columns=["node_id", "module_id"],
                states=False,
                depth_level=2,
            )
            .sort_values(["module_id", "node_id"])
            .reset_index(drop=True)
        )

    assert dataframe.equals(expected)


def test_to_dataframe_rejects_unknown_columns(make_infomap, example_network_path):
    pytest.importorskip("pandas")

    im = make_infomap(num_trials=10)
    im.read_file(str(example_network_path("twotriangles.net")))
    im.run()

    with pytest.raises(ValueError, match="Unknown DataFrame column 'modul_id'"):
        im.to_dataframe(columns=["node_id", "modul_id"])


def test_get_dataframe_missing_pandas_message(monkeypatch, make_infomap):
    im = make_infomap(no_infomap=True)
    monkeypatch.setattr(results_module, "pandas", None)

    with (
        pytest.deprecated_call(match="get_dataframe.*to_dataframe"),
        pytest.raises(ImportError, match=r"infomap\[pandas\]"),
    ):
        im.get_dataframe()


def test_python_package_extras_are_declared(test_paths):
    pyproject = tomllib.loads((test_paths.repo_root / "pyproject.toml").read_text())
    optional_dependencies = pyproject["project"]["optional-dependencies"]

    assert optional_dependencies["networkx"] == ["networkx"]
    assert optional_dependencies["igraph"] == ["igraph"]
    assert optional_dependencies["pandas"] == ["pandas"]
    assert optional_dependencies["graphrag"] == ["numpy", "pandas", "pyarrow"]
    assert optional_dependencies["scipy"] == ["scipy"]
    assert optional_dependencies["anndata"] == ["anndata", "pandas", "scipy"]
    assert set(optional_dependencies["all"]) == {
        "anndata",
        "igraph",
        "networkx",
        "numpy",
        "pandas",
        "pyarrow",
        "scipy",
    }


def test_py_typed_marker_is_packaged():
    assert importlib.resources.files("infomap").joinpath("py.typed").is_file()


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
    monkeypatch.setattr(InfomapWrapper, "__init__", fake_init)

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


def test_infomap_repr_shows_readable_state(make_infomap):
    im = make_infomap()
    im.add_link(1, 2)

    assert (
        repr(im)
        == "Infomap(nodes=2, links=1, physical_nodes=2, state_nodes=0, status='not run')"
    )
    assert im.summary() == {
        "nodes": 2,
        "links": 1,
        "physical_nodes": 2,
        "state_nodes": 0,
        "higher_order": False,
        "status": "not run",
    }

    im.run()

    representation = repr(im)

    assert representation.startswith(
        "Infomap(nodes=2, links=1, physical_nodes=2, state_nodes=0, status='run'"
    )
    assert "multilayer_network=False" in representation
    assert "levels=2" in representation
    assert "top_modules=1" in representation
    assert "codelength=1.0" in representation

    summary = im.summary()
    assert summary["state_input"] is False
    assert summary["multilayer_input"] is False
    assert summary["multilayer_network"] is False
    assert summary["levels"] == 2
    assert summary["leaf_nodes"] == 2
    assert summary["top_modules"] == 1
    assert summary["non_trivial_top_modules"] == 0
    assert summary["leaf_modules"] == 1
    assert summary["index_codelength"] == 0.0
    assert summary["module_codelength"] == 1.0
    assert summary["one_level_codelength"] == 1.0
    assert summary["relative_codelength_savings"] == 0.0
    assert summary["entropy_rate"] == 0.0
    assert summary["elapsed_time"] >= 0


def test_infomap_html_repr_shows_not_run_state(make_infomap):
    im = make_infomap()
    im.add_link(1, 2)

    html = im._repr_html_()

    assert "Infomap" in html
    assert "Status" in html
    assert "Not run" in html
    assert "Nodes" in html
    assert "Links" in html
    assert "Higher-order" in html
    assert "Top-module flow" not in html


def test_infomap_html_repr_shows_run_summary_and_flow(make_infomap):
    im = make_infomap(num_trials=10)
    im.add_links([(1, 2), (2, 3), (3, 1), (4, 5), (5, 6), (6, 4), (3, 4)])
    im.run()

    html = im._repr_html_()

    assert "Codelength" in html
    assert "Relative savings" in html
    assert "Top modules" in html
    assert "Effective modules" in html
    assert "Top-module flow" in html
    assert "Module 1" in html


def test_infomap_html_repr_escapes_rendered_values(monkeypatch, make_infomap):
    im = make_infomap()

    def fake_summary():
        return {
            "nodes": "<script>",
            "links": 0,
            "physical_nodes": "<script>",
            "higher_order": False,
            "status": "not run",
        }

    monkeypatch.setattr(im, "summary", fake_summary)

    html = im._repr_html_()

    assert "&lt;script&gt;" in html
    assert "<script>" not in html


def test_top_module_flow_bars_caps_tail_as_other(monkeypatch):
    class FakeModule:
        def __init__(self, module_id, flow):
            self.module_id = module_id
            self.flow = flow
            self.depth = 1

    class FakeResult:
        def tree(self, depth=1):
            assert depth == 1
            return [FakeModule(module_id, 1.0) for module_id in range(1, 21)]

    class FakeInfomap:
        _result = FakeResult()

    monkeypatch.setattr(summary_module, "_REPR_MAX_FLOW_FRACTION", 0.5)
    monkeypatch.setattr(summary_module, "_REPR_MAX_MODULES", 4)

    bars = summary_module._top_module_flow_bars(FakeInfomap())

    assert len(bars) == 5
    assert [bar["label"] for bar in bars[:-1]] == [
        "Module 1",
        "Module 2",
        "Module 3",
        "Module 4",
    ]
    assert bars[-1]["label"] == "Other modules"
    assert bars[-1]["fraction"] == pytest.approx(0.8)
