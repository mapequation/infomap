import shlex

import pytest

import infomap as infomap_module


pytestmark = pytest.mark.fast


def test_construct_args_renders_expected_cli_flags():
    args = infomap_module._construct_args(
        args="--existing",
        no_infomap=True,
        no_file_output=True,
        output=["json", "tree"],
        recorded_teleportation=True,
        variable_markov_time=True,
        variable_markov_damping=0.42,
        verbosity_level=3,
    )

    assert shlex.split(args) == [
        "--existing",
        "--no-infomap",
        "-vvv",
        "--no-file-output",
        "--output",
        "json,tree",
        "--recorded-teleportation",
        "--variable-markov-time",
        "--variable-markov-damping",
        "0.42",
    ]


def test_construct_args_deduplicates_no_self_links():
    with pytest.deprecated_call(
        match="include_self_links is deprecated, use no_self_links to exclude self-links"
    ):
        args = infomap_module._construct_args(
            include_self_links=False,
            no_self_links=True,
        )

    tokens = shlex.split(args)
    assert tokens.count("--no-self-links") == 1


def test_construct_args_quotes_values_with_spaces():
    args = infomap_module._construct_args(
        out_name="foo bar",
        meta_data="path with spaces.clu",
    )

    tokens = shlex.split(args)
    assert tokens == [
        "--meta-data",
        "path with spaces.clu",
        "--out-name",
        "foo bar",
    ]

    conf = infomap_module.Config(args, False)
    assert conf.outName == "foo bar"
    assert conf.metaDataFile == "path with spaces.clu"


def test_run_forwards_variable_markov_options(monkeypatch):
    captured = {}
    im = infomap_module.Infomap(silent=True, no_file_output=True)

    def fake_construct_args(args=None, **kwargs):
        captured["args"] = args
        captured["kwargs"] = kwargs
        return "--synthetic"

    def fake_run(self, args):
        captured["rendered_args"] = args
        return args

    monkeypatch.setattr(infomap_module, "_construct_args", fake_construct_args)
    monkeypatch.setattr(infomap_module.InfomapWrapper, "run", fake_run)

    result = im.run(variable_markov_time=True, variable_markov_damping=0.25)

    assert result == "--synthetic"
    assert captured["rendered_args"] == "--synthetic"
    assert captured["kwargs"]["variable_markov_time"] is True
    assert captured["kwargs"]["variable_markov_damping"] == 0.25


def test_no_file_output_runs_without_output_directory(make_infomap, load_graph_fixture):
    im = make_infomap(no_file_output=True)
    load_graph_fixture(im, "twotriangles_unweighted.edges")

    im.run()

    assert im.num_top_modules == 2
