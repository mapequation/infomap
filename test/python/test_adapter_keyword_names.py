"""The graph-input keyword rename (#791 §2).

``node_id`` / ``layer_id`` named the *attribute* to read an id from and
``node_ids`` an explicit *sequence of labels* -- two unrelated parameters that
read as singular and plural of one. From 2.16 they are ``node_id_attribute`` /
``layer_id_attribute`` and ``node_labels``; the old spellings keep working,
announce themselves on the legacy tier at the caller's line, and leave in 3.0.
Typing both spellings with the new one set away from its default is an error.
"""

from __future__ import annotations

import warnings

import infomap
import networkx as nx
import pytest
from infomap._options import LEGACY_SURFACE_WARNING

pytestmark = pytest.mark.fast


def _state_graph() -> nx.Graph:
    graph = nx.Graph()
    graph.add_node("a1", phys="alpha", layer=1)
    graph.add_node("b1", phys="beta", layer=1)
    graph.add_node("a2", phys="alpha", layer=2)
    graph.add_edge("a1", "b1")
    graph.add_edge("a1", "a2")
    return graph


def _legacy_only(records):
    return [r for r in records if issubclass(r.category, LEGACY_SURFACE_WARNING)]


def test_from_networkx_attribute_names_are_canonical_and_old_spellings_announce():
    graph = _state_graph()
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        canonical = infomap.Network.from_networkx(
            graph, node_id_attribute="phys", layer_id_attribute="layer"
        )
    assert canonical.node_id_to_label == {0: "a1", 1: "b1", 2: "a2"}

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        legacy = infomap.Network.from_networkx(graph, node_id="phys", layer_id="layer")
    messages = [str(r.message) for r in _legacy_only(records)]
    assert len(messages) == 2
    assert any(
        "'node_id' is deprecated" in m and "node_id_attribute='phys'" in m
        for m in messages
    )
    assert any(
        "'layer_id' is deprecated" in m and "layer_id_attribute='layer'" in m
        for m in messages
    )
    assert all(r.filename == __file__ for r in _legacy_only(records))
    assert legacy.node_id_to_label == canonical.node_id_to_label
    assert (
        infomap.run(legacy, seed=1).codelength
        == infomap.run(canonical, seed=1).codelength
    )


def test_from_networkx_rejects_both_spellings_when_they_disagree():
    graph = _state_graph()
    with pytest.raises(
        ValueError,
        match="both node_id_attribute='phys' and the deprecated node_id='other'",
    ):
        infomap.Network.from_networkx(graph, node_id_attribute="phys", node_id="other")
    # The old spelling next to the new one left at its default is accepted
    # (it is what an old call site looks like), with the warning.
    with pytest.warns(LEGACY_SURFACE_WARNING):
        infomap.Network.from_networkx(graph, node_id="phys", layer_id="layer")


def test_from_igraph_attribute_names_match_the_networkx_constructor():
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1), (0, 2)])
    graph.vs["phys"] = ["alpha", "beta", "alpha"]
    graph.vs["layer"] = [1, 1, 2]
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        canonical = infomap.Network.from_igraph(
            graph, node_id_attribute="phys", layer_id_attribute="layer"
        )
    with pytest.warns(
        LEGACY_SURFACE_WARNING,
        match="'node_id' is deprecated on Network.from_igraph",
    ):
        legacy = infomap.Network.from_igraph(graph, node_id="phys", layer_id="layer")
    assert (
        infomap.run(legacy, seed=1).codelength
        == infomap.run(canonical, seed=1).codelength
    )


def test_matrix_constructors_take_node_labels_and_announce_node_ids():
    sp = pytest.importorskip("scipy.sparse")
    matrix = sp.csr_matrix([[0, 1, 0], [1, 0, 1], [0, 1, 0]])
    labels = ["gene_a", "gene_b", "gene_c"]

    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        canonical = infomap.Network.from_scipy_sparse_matrix(matrix, node_labels=labels)
    assert canonical.node_id_to_label == dict(enumerate(labels))
    with pytest.warns(
        LEGACY_SURFACE_WARNING,
        match="'node_ids' is deprecated on Network.from_scipy_sparse_matrix",
    ) as records:
        legacy = infomap.Network.from_scipy_sparse_matrix(matrix, node_ids=labels)
    assert legacy.node_id_to_label == canonical.node_id_to_label
    assert records[0].filename == __file__
    with pytest.raises(ValueError, match="pass only node_labels"):
        infomap.Network.from_scipy_sparse_matrix(
            matrix, node_labels=labels, node_ids=labels
        )

    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        canonical_ei = infomap.Network.from_edge_index(
            [[0, 1], [1, 2]], node_labels=labels
        )
    with pytest.warns(
        LEGACY_SURFACE_WARNING,
        match="'node_ids' is deprecated on Network.from_edge_index",
    ):
        legacy_ei = infomap.Network.from_edge_index([[0, 1], [1, 2]], node_ids=labels)
    assert (
        legacy_ei.node_id_to_label
        == canonical_ei.node_id_to_label
        == dict(enumerate(labels))
    )


def test_finders_take_the_attribute_names_and_announce_the_old_spellings():
    graph = _state_graph()
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        canonical = infomap.find_communities(
            graph, node_id_attribute="phys", layer_id_attribute="layer", seed=1
        )
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        legacy = infomap.find_communities(
            graph, node_id="phys", layer_id="layer", seed=1
        )
    assert len(_legacy_only(records)) == 2
    assert all(r.filename == __file__ for r in _legacy_only(records))
    assert legacy == canonical

    ig = pytest.importorskip("igraph")
    igraph_graph = ig.Graph(edges=[(0, 1), (0, 2)])
    igraph_graph.vs["phys"] = ["alpha", "beta", "alpha"]
    igraph_graph.vs["layer"] = [1, 1, 2]
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        infomap.find_igraph_communities(
            igraph_graph, node_id_attribute="phys", layer_id_attribute="layer", seed=1
        )
    with pytest.warns(
        LEGACY_SURFACE_WARNING,
        match="'layer_id' is deprecated on find_igraph_communities",
    ):
        infomap.find_igraph_communities(
            igraph_graph, node_id_attribute="phys", layer_id="layer", seed=1
        )


def test_functional_run_steers_the_new_names_to_the_constructor():
    # The adapter-kwarg guard knows the new spellings, so infomap.run() names
    # the right constructor instead of a generic unknown-option error.
    graph = _state_graph()
    with pytest.raises(TypeError, match="node_id_attribute.*Network.from_networkx"):
        infomap.run(graph, node_id_attribute="phys")
    sp = pytest.importorskip("scipy.sparse")
    with pytest.raises(
        TypeError, match="node_labels.*Network.from_scipy_sparse_matrix"
    ):
        infomap.run(sp.csr_matrix([[0, 1], [1, 0]]), node_labels=["a", "b"])


def test_explicit_none_for_the_old_spelling_is_still_a_typed_old_spelling():
    # None is a legitimate value of node_ids ("no labels"), so it cannot double
    # as the omission marker: node_ids=None is typed, gets its notice, and next
    # to node_labels it is the documented conflict.
    sp = pytest.importorskip("scipy.sparse")
    matrix = sp.csr_matrix([[0, 1], [1, 0]])
    with pytest.warns(LEGACY_SURFACE_WARNING, match="'node_ids' is deprecated"):
        net = infomap.Network.from_scipy_sparse_matrix(matrix, node_ids=None)
    assert net.node_id_to_label == {0: 0, 1: 1}
    with pytest.raises(ValueError, match="pass only node_labels"):
        infomap.Network.from_scipy_sparse_matrix(
            matrix, node_labels=["a", "b"], node_ids=None
        )


def test_finders_announce_the_old_spelling_on_an_empty_graph_too():
    # The finders return before building an engine for an empty graph; the
    # renamed keywords are resolved before that return so the notice does not
    # depend on the input's size.
    with pytest.warns(
        LEGACY_SURFACE_WARNING, match="'node_id' is deprecated on find_communities"
    ):
        assert infomap.find_communities(nx.Graph(), node_id="phys") == []
    with pytest.raises(ValueError, match="both node_id_attribute"):
        infomap.find_communities(nx.Graph(), node_id_attribute="phys", node_id="other")
    ig = pytest.importorskip("igraph")
    with pytest.warns(
        LEGACY_SURFACE_WARNING,
        match="'layer_id' is deprecated on find_igraph_communities",
    ):
        infomap.find_igraph_communities(ig.Graph(), layer_id="layer")


def test_label_validation_errors_name_the_parameter_the_caller_used():
    sp = pytest.importorskip("scipy.sparse")
    matrix = sp.csr_matrix([[0, 1], [1, 0]])
    with pytest.raises(ValueError, match="`node_labels` length must match"):
        infomap.Network.from_scipy_sparse_matrix(matrix, node_labels=["a"])
    with pytest.raises(ValueError, match="`node_labels` values must be unique"):
        infomap.Network.from_edge_index([[0], [1]], node_labels=["a", "a"])
    # The legacy add_* surface keeps its own wording.
    im = infomap.Infomap(num_trials=1, seed=1)
    with pytest.raises(ValueError, match="`node_ids` values must be unique"):
        im.add_scipy_sparse_matrix(matrix, node_ids=["a", "a"])


def test_igraph_value_errors_name_the_attribute_the_caller_selected():
    # The validators report the vertex attribute as the caller spelled it, so a
    # call through node_id_attribute / layer_id_attribute is never told that a
    # keyword it did not type ("node_id", "layer_id") holds bad values.
    ig = pytest.importorskip("igraph")
    graph = ig.Graph(edges=[(0, 1)])
    graph.vs["phys"] = ["alpha", "beta"]
    graph.vs["layer"] = ["one", "two"]
    with pytest.raises(ValueError, match="`layer` values must be integer-like") as info:
        infomap.Network.from_igraph(
            graph, node_id_attribute="phys", layer_id_attribute="layer"
        )
    assert "layer_id" not in str(info.value)

    # With the default attribute names the wording is unchanged.
    legacy = ig.Graph(edges=[(0, 1)])
    legacy.vs["node_id"] = ["alpha", "beta"]
    legacy.vs["layer_id"] = ["one", "two"]
    with pytest.raises(ValueError, match="`layer_id` values must be integer-like"):
        infomap.Infomap(num_trials=1).add_igraph_graph(legacy)
