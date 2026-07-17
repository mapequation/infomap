"""Path-typed engine options share the package-wide file-path contract.

The file-reading entry points accept ``str | os.PathLike`` and decode with
``os.fsdecode`` (#809). The path-typed *options* (``cluster_data``,
``meta_data``, the report JSON paths, ``trial_results``) travel to the engine
through the rendered argument string instead, so they need the same
acceptance plus one extra guard: the argument string is split on whitespace
with no quoting support, so a path containing whitespace can never arrive
intact and must be rejected up front with the reason -- not silently
truncated into a confusing engine error.
"""

from __future__ import annotations

import os
from pathlib import Path

import pytest

import infomap
from infomap import Infomap, Options


pytestmark = pytest.mark.fast

_LINKS = [(1, 2), (1, 3), (2, 3), (3, 4), (4, 5), (4, 6), (5, 6)]


@pytest.fixture
def clu_file(tmp_path) -> Path:
    clu = tmp_path / "partition.clu"
    clu.write_text("# node module\n1 1\n2 1\n3 1\n4 2\n5 2\n6 2\n")
    return clu


def test_path_option_table_matches_the_catalog():
    from infomap._options import _OPTION_PATHS

    # The path-typed options, as declared ArgType::path in the C++ catalog.
    # A new path option lands here via make build-binding-options; update the
    # expectation when one is added.
    assert set(_OPTION_PATHS) == {
        "cluster_data",
        "meta_data",
        "timing_json",
        "summary_json",
        "manifest_json",
        "trial_results",
    }


def test_options_normalize_pathlike_to_str(clu_file, fspath_only):
    options = Options(cluster_data=fspath_only(clu_file))

    assert options.cluster_data == str(clu_file)


def test_options_normalize_bytes_to_str(clu_file):
    options = Options(cluster_data=os.fsencode(clu_file))

    assert options.cluster_data == str(clu_file)


def test_options_replace_normalizes_pathlike(clu_file, fspath_only):
    options = Options().replace(cluster_data=fspath_only(clu_file))

    assert options.cluster_data == str(clu_file)


def test_options_to_args_renders_the_real_path(clu_file, fspath_only):
    args = Options(cluster_data=fspath_only(clu_file)).to_args()

    assert str(clu_file) in args
    assert "not-the-path" not in args


def test_run_accepts_pathlike_cluster_data(clu_file, fspath_only):
    expected = infomap.run(_LINKS, cluster_data=str(clu_file), no_infomap=True)

    result = infomap.run(_LINKS, cluster_data=fspath_only(clu_file), no_infomap=True)

    assert result.modules() == expected.modules()


def test_infomap_constructor_accepts_pathlike_cluster_data(clu_file, fspath_only):
    im = Infomap(silent=True, cluster_data=fspath_only(clu_file), no_infomap=True)
    im.add_links(_LINKS)
    result = im.run()

    expected = infomap.run(_LINKS, cluster_data=str(clu_file), no_infomap=True)
    assert result.modules() == expected.modules()


def test_infomap_run_accepts_pathlike_cluster_data(clu_file, fspath_only):
    im = Infomap(silent=True)
    im.add_links(_LINKS)
    result = im.run(cluster_data=fspath_only(clu_file), no_infomap=True)

    expected = infomap.run(_LINKS, cluster_data=str(clu_file), no_infomap=True)
    assert result.modules() == expected.modules()


def test_non_path_option_value_rejected_with_option_name():
    with pytest.raises(TypeError, match="cluster_data"):
        Options(cluster_data=123)


def test_whitespace_path_rejected_at_construction(tmp_path):
    clu = tmp_path / "my partition.clu"
    clu.write_text("1 1\n")

    with pytest.raises(ValueError, match="whitespace"):
        Options(cluster_data=str(clu))


def test_whitespace_out_name_rejected(tmp_path):
    # out_name is string-typed rather than path-typed, but it is rendered
    # into the same whitespace-split argument string.
    with pytest.raises(ValueError, match="whitespace"):
        Options(out_name="my results")


def test_whitespace_path_rejected_at_run(tmp_path):
    clu = tmp_path / "my partition.clu"
    clu.write_text("1 1\n")

    with pytest.raises(ValueError, match="whitespace"):
        infomap.run(_LINKS, cluster_data=str(clu), no_infomap=True)
