from __future__ import annotations

import pytest

nx = pytest.importorskip("networkx")

# Requires a NetworkX that defines the backend-only ``infomap_communities`` API
# (NetworkX >= the release that adds it). Skip cleanly on older versions.
if not hasattr(nx.community, "infomap_communities"):
    pytest.skip(
        "networkx does not provide infomap_communities", allow_module_level=True
    )


def _normalized(partition):
    return sorted(map(sorted, partition))


def test_backend_is_registered():
    assert "infomap" in nx.community.infomap_communities.backends


def test_explicit_backend_returns_valid_partition():
    G = nx.karate_club_graph()
    partition = nx.community.infomap_communities(G, backend="infomap", seed=42)
    assert nx.community.is_partition(G, partition)
    assert set().union(*partition) == set(G.nodes())


def test_backend_priority_dispatch_matches_explicit():
    G = nx.karate_club_graph()
    explicit = nx.community.infomap_communities(G, backend="infomap", seed=42)

    original_priority = nx.config.backend_priority
    nx.config.backend_priority = ["infomap"]
    try:
        auto = nx.community.infomap_communities(G, seed=42)
    finally:
        nx.config.backend_priority = original_priority

    assert nx.community.is_partition(G, auto)
    assert _normalized(auto) == _normalized(explicit)


def test_backend_only_kwargs_are_forwarded():
    G = nx.karate_club_graph()
    partition = nx.community.infomap_communities(
        G, backend="infomap", two_level=True, markov_time=1.5, seed=42
    )
    assert nx.community.is_partition(G, partition)


def test_directed_graph_supported():
    D = nx.gnc_graph(60, seed=1)
    partition = nx.community.infomap_communities(D, backend="infomap", seed=1)
    assert nx.community.is_partition(D, partition)


def test_seed_is_reproducible():
    G = nx.karate_club_graph()
    a = nx.community.infomap_communities(G, backend="infomap", seed=7)
    b = nx.community.infomap_communities(G, backend="infomap", seed=7)
    assert _normalized(a) == _normalized(b)


def test_weight_none_treats_edges_as_unweighted():
    G = nx.karate_club_graph()
    nx.set_edge_attributes(
        G, {edge: i * i for i, edge in enumerate(G.edges)}, name="weight"
    )
    partition = nx.community.infomap_communities(
        G, backend="infomap", weight=None, seed=42
    )
    assert nx.community.is_partition(G, partition)


def test_seed_and_num_trials_forwarded_to_infomap(monkeypatch):
    import infomap._nx_backend as backend

    captured: dict = {}

    def fake_find_communities(G, **kwargs):
        captured.update(kwargs)
        return [set(G.nodes())]

    monkeypatch.setattr(backend, "find_communities", fake_find_communities)

    G = nx.karate_club_graph()
    backend.BackendInterface.infomap_communities(G, seed=99, num_trials=5)

    assert captured["seed"] == 99
    assert captured["num_trials"] == 5


def test_multigraph_is_rejected():
    G = nx.MultiGraph([(0, 1), (0, 1), (1, 2)])
    with pytest.raises(nx.NetworkXNotImplemented):
        nx.community.infomap_communities(G, backend="infomap")


def test_isolated_node_is_retained():
    G = nx.karate_club_graph()  # integer-labeled nodes 0..33
    G.add_node(34)  # isolated, same label type
    partition = nx.community.infomap_communities(G, backend="infomap", seed=42)
    # Isolated nodes must not be dropped from the partition.
    assert nx.community.is_partition(G, partition)
    assert 34 in set().union(*partition)
