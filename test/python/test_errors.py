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

import pytest

from infomap import (
    Infomap,
    InfomapError,
    Network,
    NetworkParseError,
    NotRunError,
    StaleResultError,
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
