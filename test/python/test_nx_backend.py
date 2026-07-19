from __future__ import annotations

import pytest

nx = pytest.importorskip("networkx")

# Requires a NetworkX that defines the backend-only ``infomap_communities`` API
# (NetworkX >= the release that adds it). Skip cleanly on older versions.
if not hasattr(nx.community, "infomap_communities"):
    pytest.skip(
        "networkx does not provide infomap_communities", allow_module_level=True
    )

pytestmark = pytest.mark.fast


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
    import infomap.io.nx_backend as backend

    captured: dict = {}

    class FakeInfomap:
        def __init__(self, **kwargs):
            captured.update(kwargs)

        def add_networkx_graph(self, G, **kwargs):
            self._labels = list(G)
            return dict(enumerate(self._labels))

        def run(self):
            pass

        def get_modules(self):
            return {i: i for i in range(len(self._labels))}

    monkeypatch.setattr(backend, "Infomap", FakeInfomap)

    G = nx.karate_club_graph()
    backend.BackendInterface.infomap_communities(G, seed=99, num_trials=5)

    assert captured["seed"] == 99
    assert captured["num_trials"] == 5
    assert captured["two_level"] is True


@pytest.mark.parametrize(
    ("multi", "simple"),
    [(nx.MultiGraph, nx.Graph), (nx.MultiDiGraph, nx.DiGraph)],
)
def test_multigraph_parallel_edges_are_summed(multi, simple):
    """The backend accepts (di)multigraphs: add_networkx_graph sums parallel
    edges, so the result matches the equivalent weighted simple graph. Two
    triangles with heavy (3x parallel) intra-group edges and a single light
    bridge -- the two groups are distinct only if the parallel edges are summed,
    so this would fail if they were dropped or counted once."""
    intra = [(0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3)]
    MG = multi([e for e in intra for _ in range(3)] + [(2, 3)])
    SG = simple()
    SG.add_weighted_edges_from([(*e, 3) for e in intra] + [(2, 3, 1)])
    mg_part = nx.community.infomap_communities(MG, backend="infomap", seed=42)
    assert nx.community.is_partition(MG, mg_part)
    assert len(mg_part) >= 2  # structure recovered, not collapsed to one module
    sg_part = nx.community.infomap_communities(SG, backend="infomap", seed=42)
    assert _normalized(mg_part) == _normalized(sg_part)


def test_isolated_node_is_retained():
    G = nx.karate_club_graph()  # integer-labeled nodes 0..33
    G.add_node(34)  # isolated, same label type
    partition = nx.community.infomap_communities(G, backend="infomap", seed=42)
    # Isolated nodes must not be dropped from the partition.
    assert nx.community.is_partition(G, partition)
    assert 34 in set().union(*partition)


def test_backend_two_level_matches_native_infomap():
    """The infomap backend reaches the same two-level optimum as NetworkX's
    native infomap_communities on a genuinely multilevel graph -- the parity
    that lets the two implementations stand in for each other."""
    from networkx.algorithms.community.quality import map_equation

    G = nx.ring_of_cliques(12, 6)
    native = nx.community.infomap_communities(G, weight=None, seed=42, num_trials=20)
    backend = nx.community.infomap_communities(
        G, backend="infomap", weight=None, seed=42, num_trials=20
    )
    assert map_equation(G, backend, weight=None) == pytest.approx(
        map_equation(G, native, weight=None), abs=1e-6
    )


def test_backend_infomap_partitions_yields_nested_levels():
    """The backend implements infomap_partitions, yielding a valid partition per
    hierarchy level (coarsest first) -- the same contract as native."""
    G = nx.ring_of_cliques(12, 6)
    levels = list(nx.community.infomap_partitions(G, backend="infomap", seed=42))
    assert len(levels) >= 2  # ring of cliques is genuinely multilevel
    for partition in levels:
        assert nx.community.is_partition(G, partition)
    # Coarsest first: each subsequent level has at least as many communities.
    assert [len(p) for p in levels] == sorted(len(p) for p in levels)


def test_backend_map_equation_matches_native():
    """The backend's map_equation (C++ engine, no optimization) returns the same
    two-level codelength as networkx's native map_equation for a partition."""
    from networkx.algorithms.community.quality import map_equation

    G = nx.karate_club_graph()
    communities = nx.community.infomap_communities(G, seed=42, num_trials=10)
    native = map_equation(G, communities)
    backend = nx.community.map_equation(G, communities, backend="infomap")
    assert backend == pytest.approx(native, abs=1e-6)


def test_backend_map_equation_directed_matches_native():
    """Directed map_equation matches native, confirming the backend computes
    directed flow (not undirected) for a DiGraph."""
    from networkx.algorithms.community.quality import map_equation

    G = nx.DiGraph([(0, 1), (1, 2), (2, 0), (3, 4), (4, 5), (5, 3), (2, 3), (0, 4)])
    communities = [{0, 1, 2}, {3, 4, 5}]
    native = map_equation(G, communities)
    backend = nx.community.map_equation(G, communities, backend="infomap")
    assert backend == pytest.approx(native, abs=1e-6)


def test_backend_invalid_num_trials():
    """num_trials must be a positive integer; the backend raises ValueError (the
    native contract), not the C++ RuntimeError, so the two paths agree."""
    G = nx.karate_club_graph()
    for bad in (0, -1, 1.5):
        with pytest.raises(ValueError, match="num_trials"):
            nx.community.infomap_communities(G, backend="infomap", num_trials=bad)


def test_backend_rejects_negative_weight():
    """Negative weights are rejected at ingestion, matching native
    infomap_communities rather than silently producing a meaningless map."""
    G = nx.Graph()
    G.add_edge(0, 1, weight=-1.0)
    G.add_edge(1, 2, weight=2.0)
    with pytest.raises(ValueError):
        nx.community.infomap_communities(G, backend="infomap")
