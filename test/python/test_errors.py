"""The public error taxonomy (infomap.errors, #744).

Engine errors cross the SWIG boundary as bare ``RuntimeError``; the package
re-raises them as :class:`InfomapError` subclasses at the ``Core`` boundary.
Pins three contracts:

1. The MRO compatibility promises: every taxonomy class is a ``RuntimeError``
   through 2.x, and ``NotRunError`` additionally keeps ``ValueError`` (the
   type the results-before-run guard used to raise).
2. The boundary mapping: construction, ``read_file``, ``run``-time input
   files, writers, and the higher-order guard each raise the right class
   with the C++ message preserved.
3. Parity: the legacy ``Infomap`` accessors and the ``Result`` path raise
   the same taxonomy type with the same message.
"""

from __future__ import annotations

from pathlib import Path

import pytest
from infomap import (
    Infomap,
    InfomapError,
    Network,
    NetworkParseError,
    NotRunError,
    StaleResultError,
    run,
)
from infomap import (
    errors as errors_module,
)
from infomap.result import _StaleResultError

pytestmark = pytest.mark.fast

# The taxonomy classes as exported at the package top level, keyed by name.
_TOP_LEVEL_EXPORTS = {
    "InfomapError": InfomapError,
    "NetworkParseError": NetworkParseError,
    "NotRunError": NotRunError,
    "StaleResultError": StaleResultError,
}


@pytest.fixture
def unparsable_network_file(tmp_path):
    path = tmp_path / "broken.net"
    path.write_text("*Links\n1 two\n")
    return str(path)


@pytest.fixture
def higher_order_run(make_infomap):
    im = make_infomap()
    im.add_state_node(1, 1)
    im.add_state_node(2, 1)
    im.add_state_node(3, 2)
    im.add_link(1, 2)
    im.add_link(2, 3)
    im.add_link(1, 3)
    return im, im.run()


def test_taxonomy_mro_keeps_runtime_error_through_2x():
    for error_class in (NetworkParseError, NotRunError, StaleResultError):
        assert issubclass(error_class, InfomapError)
    assert issubclass(InfomapError, RuntimeError)
    # The results-before-run guard raised ValueError before the taxonomy;
    # NotRunError keeps it in the MRO so `except ValueError` code survives 2.x.
    assert issubclass(NotRunError, ValueError)


def test_module_all_matches_package_exports():
    assert sorted(errors_module.__all__) == sorted(_TOP_LEVEL_EXPORTS)
    for name, exported in _TOP_LEVEL_EXPORTS.items():
        assert getattr(errors_module, name) is exported


def test_stale_result_private_alias_is_the_public_class():
    assert _StaleResultError is StaleResultError


def test_unrecognized_option_raises_infomap_error():
    with pytest.raises(InfomapError, match="Unrecognized option"):
        Infomap(args="--bogus-flag", silent=True)


def test_empty_network_run_raises_clear_infomap_error():
    """An empty network gets a build-the-network pointer, not the engine's
    file-oriented "No input file to read network" -- meaningless on the
    in-memory library surface."""
    with pytest.raises(InfomapError, match="empty network"):
        Infomap().run()
    with pytest.raises(InfomapError, match="empty network"):
        run([])
    with pytest.raises(InfomapError, match="empty network"):
        Network().run()


def test_read_file_missing_file_raises_network_parse_error(make_infomap):
    with pytest.raises(NetworkParseError, match="Cannot open file"):
        make_infomap().read_file("/nonexistent/network.net")


def test_read_file_parse_error_raises_network_parse_error(
    make_infomap, unparsable_network_file
):
    with pytest.raises(NetworkParseError, match="Can't parse link data"):
        make_infomap().read_file(unparsable_network_file)


def test_network_from_file_raises_the_same_type_as_infomap_read_file(
    unparsable_network_file,
):
    with pytest.raises(NetworkParseError, match="Can't parse link data"):
        Network.from_file(unparsable_network_file)


def test_run_time_input_failure_raises_network_parse_error(make_infomap):
    im = make_infomap(cluster_data="/nonexistent/partition.clu")
    im.add_link(1, 2)

    with pytest.raises(NetworkParseError, match="Cannot open file"):
        im.run()


def test_writer_failure_raises_infomap_error_not_parse_error(make_infomap):
    im = make_infomap()
    im.add_link(1, 2)
    im.run()

    with pytest.raises(InfomapError, match="write permissions") as excinfo:
        im.write_tree("/nonexistent-dir/result.tree")
    # An unwritable *output* is an engine error, not an input parse error.
    assert not isinstance(excinfo.value, NetworkParseError)


def test_higher_order_modules_parity_between_legacy_and_result(higher_order_run):
    im, result = higher_order_run

    with pytest.raises(InfomapError) as legacy:
        im.get_modules()
    with pytest.raises(InfomapError) as blessed:
        result.modules()

    assert str(legacy.value) == str(blessed.value)
    assert type(legacy.value) is type(blessed.value)


def test_higher_order_multilevel_modules_parity(higher_order_run):
    im, result = higher_order_run

    with pytest.raises(InfomapError) as legacy:
        im.get_multilevel_modules()
    with pytest.raises(InfomapError) as blessed:
        result.multilevel_modules()

    assert str(legacy.value) == str(blessed.value)
    assert type(legacy.value) is type(blessed.value)


def test_stale_result_raises_stale_result_error(make_infomap):
    im = make_infomap()
    im.add_link(1, 2)
    stale = im.run()
    im.run()

    with pytest.raises(StaleResultError, match="stale Result"):
        stale.modules()


def test_export_before_run_raises_not_run_error(make_infomap):
    networkx = pytest.importorskip("networkx")
    from infomap.io.export import annotate_networkx_graph

    graph = networkx.Graph([(0, 1)])
    im = make_infomap()
    im.add_networkx_graph(graph)

    with pytest.raises(NotRunError, match="Run Infomap"):
        annotate_networkx_graph(graph, im)


def test_run_time_parse_message_fragments_still_appear_in_engine_source():
    """Guard against silent drift of the run-time parse-error classifier.

    ``errors._classify_run_message`` recognizes an *input* parse failure
    surfacing from ``run()`` by matching hand-maintained fragments of the C++
    engine's error strings (``_PARSE_MESSAGE_PREFIXES`` /
    ``_PARSE_MESSAGE_SUBSTRINGS``). If the engine rewords one of those
    messages, the fragment stops matching and the failure silently downgrades
    from :class:`NetworkParseError` to a bare :class:`InfomapError` -- with no
    other test failing, because the classifier still runs, just less
    specifically. This asserts every fragment still occurs verbatim somewhere
    in the C++ sources, so a rewording breaks the build and points here.
    """
    src_root = Path(__file__).resolve().parents[2] / "src"
    if not src_root.is_dir():
        pytest.skip("C++ sources not available in this layout")

    sources = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in src_root.rglob("*")
        if path.suffix in {".cpp", ".h"}
    )
    fragments = (
        errors_module._PARSE_MESSAGE_PREFIXES
        + errors_module._PARSE_MESSAGE_SUBSTRINGS
        # The empty-network message is re-worded verbatim into a friendly
        # build-the-network pointer; if the engine rewords it, the match stops
        # firing and the raw file-oriented message leaks back to the user.
        + (errors_module._EMPTY_NETWORK_MESSAGE,)
    )
    missing = [fragment for fragment in fragments if fragment not in sources]
    assert not missing, (
        f"run-time engine message fragments no longer found in src/: {missing}. "
        "The engine likely reworded a message; update errors._PARSE_MESSAGE_* / "
        "_EMPTY_NETWORK_MESSAGE to match, or run() input failures will classify "
        "as InfomapError instead of NetworkParseError (or the empty-network "
        "error will lose its friendly re-message)."
    )
