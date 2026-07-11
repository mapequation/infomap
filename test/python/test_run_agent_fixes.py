"""Agent-usability guards on ``infomap.run`` / ``Network.run`` / ``Options``.

These pin the fixes that turn silent-wrong or unguided-error footguns into
actionable messages: a dense adjacency matrix is rejected instead of silently
misread as link rows; input-adapter kwargs on a link iterable point at
``Network.from_*``; ``initial_partition`` on a graph accepts the graph's own
labels (matching ``find_communities``); ``Network.run`` takes the five
common-tier keywords like its sibling run APIs; ``Options`` rejects an unknown
keyword with a "did you mean" suggestion; a single ``output`` format string is
rejected with a list hint instead of a per-character error; an unknown
``flow_model`` names the valid choices; and inert removed console options warn
instead of no-op.
"""

from __future__ import annotations

import warnings

import networkx as nx
import pytest

import infomap
from infomap import Infomap, Network, Options, run
from infomap.result import Result

pytestmark = pytest.mark.fast

_LINKS = [(1, 2), (2, 3), (1, 3), (3, 4), (4, 5), (4, 6), (5, 6)]


# -- dense adjacency matrix: reject, don't silently misread as link rows ------


def test_run_rejects_dense_adjacency_matrix():
    np = pytest.importorskip("numpy")
    # A 3x3 triangle adjacency matrix used to run happily as 2 link rows
    # (num_nodes=2), silently partitioning a different graph.
    A = np.array([[0, 1, 1], [1, 0, 1], [1, 1, 0]])
    with pytest.raises(TypeError, match="dense adjacency matrix"):
        infomap.run(A, seed=1)


def test_run_rejects_larger_dense_matrix_with_matrix_hint():
    np = pytest.importorskip("numpy")
    A = np.ones((4, 4), dtype=int)
    np.fill_diagonal(A, 0)
    with pytest.raises(TypeError, match="dense adjacency matrix"):
        infomap.run(A, seed=1)


def test_run_still_reads_non_square_float_link_matrix_as_links():
    np = pytest.importorskip("numpy")
    # (2, 3) float link matrix: not square, so the guard leaves it alone and it
    # is read as (source, target, weight) rows.
    links = np.array([[1, 2, 1.5], [2, 3, 2.0]])
    assert isinstance(infomap.run(links, seed=1, num_trials=1), Result)


def test_run_still_reads_integer_edge_index():
    np = pytest.importorskip("numpy")
    edge_index = np.array([[0, 1, 2, 3], [1, 2, 3, 0]])
    assert isinstance(infomap.run(edge_index, seed=1, num_trials=1), Result)


# -- adapter kwargs on a link iterable point at Network.from_* ----------------


def test_run_rejects_adapter_kwarg_on_link_iterable():
    with pytest.raises(TypeError, match="link-iterable or file input"):
        infomap.run(_LINKS, weight="w", seed=1)


def test_run_link_iterable_weight_hint_points_at_tuples():
    with pytest.raises(TypeError, match="inside the tuples"):
        infomap.run(_LINKS, edge_weight=[1.0], seed=1)


def test_run_still_allows_directed_on_link_iterable():
    # `directed` is a legitimate common-tier engine flag, not an adapter kwarg,
    # so the iterable guard must not reject it.
    assert isinstance(
        infomap.run(_LINKS, directed=True, seed=1, num_trials=1), Result
    )


# -- initial_partition on a graph accepts the graph's own labels --------------


def test_run_graph_initial_partition_accepts_string_labels():
    graph = nx.Graph()
    graph.add_edges_from(
        [("a", "b"), ("b", "c"), ("c", "a"), ("d", "e"), ("e", "f"), ("f", "d"), ("c", "d")]
    )
    labelled = {"a": 0, "b": 0, "c": 0, "d": 1, "e": 1, "f": 1}
    # Used to raise "must map integer node/state ids": run() now translates
    # labels to internal ids the way find_communities does.
    result = infomap.run(graph, seed=1, num_trials=1, initial_partition=labelled)
    assert isinstance(result, Result)
    # find_communities accepts the same label-keyed partition.
    communities = infomap.find_communities(
        graph, seed=1, initial_partition=labelled
    )
    assert communities


def test_run_integer_labelled_graph_initial_partition_unaffected():
    graph = nx.Graph([(0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3), (2, 3)])
    result = infomap.run(
        graph, seed=1, num_trials=1, initial_partition={0: 0, 1: 0, 2: 0}
    )
    assert isinstance(result, Result)


# -- Network.run takes the five common-tier keywords --------------------------


def test_network_run_accepts_common_tier_keywords():
    net = Network()
    net.add_links(_LINKS)
    result = net.run(num_trials=5, seed=1, two_level=True)
    assert isinstance(result, Result)


def test_network_run_common_tier_matches_options_carrier():
    net = Network()
    net.add_links(_LINKS)
    direct = net.run(num_trials=5, seed=1, markov_time=2.0)

    net2 = Network()
    net2.add_links(_LINKS)
    carried = net2.run(options=Options(num_trials=5, seed=1, markov_time=2.0))

    assert direct.codelength == pytest.approx(carried.codelength)


def test_network_run_keyword_overrides_options_carrier():
    net = Network()
    net.add_links(_LINKS)
    # A supplied common-tier keyword wins over the same field in the carrier.
    result = net.run(options=Options(num_trials=1, seed=999), seed=1, num_trials=5)
    assert isinstance(result, Result)


# -- Options rejects an unknown keyword with a suggestion ---------------------


def test_options_unknown_keyword_suggests_nearest():
    with pytest.raises(TypeError, match="did you mean 'num_trials'"):
        Options(num_trial=10)


def test_options_unknown_keyword_points_at_reference():
    with pytest.raises(TypeError, match="inspect.getdoc"):
        Options(totally_made_up=1)


# -- output format string vs list --------------------------------------------


def test_options_output_single_string_rejected_with_list_hint():
    with pytest.raises(ValueError, match=r"must be a list or tuple"):
        Options(output="tree")


def test_options_output_list_accepted():
    assert Options(output=["tree", "clu"]).output == ["tree", "clu"]


# -- choice validation names the valid set (previously untested) --------------


def test_options_invalid_flow_model_names_choices():
    with pytest.raises(ValueError, match="choose one of"):
        Options(flow_model="undirectd")


def test_run_invalid_flow_model_is_value_error_not_parse_error():
    # Reaches the Options choice validator before the engine, so it is a clean
    # ValueError, not the engine's misleading NetworkParseError.
    with pytest.raises(ValueError, match="choose one of"):
        infomap.run(_LINKS, options=Options(flow_model="bogus"), seed=1)


# -- inert removed console options warn instead of silently no-op -------------


def _user_warnings(records):
    return [r for r in records if issubclass(r.category, UserWarning)]


def test_verbosity_level_via_options_warns():
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        infomap.run(_LINKS, seed=1, options=Options(verbosity_level=3))
    assert any("no effect" in str(r.message) for r in _user_warnings(records))


def test_print_config_fingerprint_via_options_warns():
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        infomap.run(_LINKS, seed=1, options=Options(print_config_fingerprint=True))
    assert any("no effect" in str(r.message) for r in _user_warnings(records))


def test_default_console_options_do_not_warn():
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        infomap.run(_LINKS, seed=1, options=Options(verbosity_level=1))
    assert not any(
        "no effect" in str(r.message) for r in _user_warnings(records)
    )
