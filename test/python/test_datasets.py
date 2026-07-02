from importlib.resources import as_file, files

import pytest
from infomap import Network, datasets

pytestmark = pytest.mark.fast

# (loader name, packaged file name, engine-reported num_nodes, num_links).
# num_nodes counts state nodes for the higher-order networks (states and the
# multilayer variants), and the parser expands *Inter rows / relax-generated
# inter-layer movement into explicit links, so all multilayer variants report
# 16 links.
DATASETS = [
    ("two_triangles", "twotriangles.net", 6, 7),
    ("nine_triangles", "ninetriangles.net", 27, 39),
    ("bipartite", "bipartite.net", 5, 4),
    ("multilayer", "multilayer.net", 6, 16),
    ("multilayer_intra_inter", "multilayer_intra_inter.net", 6, 16),
    ("multilayer_intra", "multilayer_intra.net", 6, 16),
    ("states", "states.net", 6, 16),
    ("modular_w", "modular_w.net", 25, 43),
    ("modular_wd", "modular_wd.net", 25, 48),
]


@pytest.mark.parametrize(
    ("loader_name", "num_nodes", "num_links"),
    [(name, nodes, links) for name, _, nodes, links in DATASETS],
)
def test_loader_returns_network_with_expected_counts(loader_name, num_nodes, num_links):
    net = getattr(datasets, loader_name)()

    assert isinstance(net, Network)
    assert net.num_nodes == num_nodes
    assert net.num_links == num_links


@pytest.mark.parametrize("loader_name", [name for name, _, _, _ in DATASETS])
def test_loader_network_runs(loader_name):
    result = getattr(datasets, loader_name)().run(options={"seed": 123})

    assert result.num_top_modules > 0
    assert result.codelength > 0


def test_loaders_return_fresh_networks():
    assert datasets.two_triangles() is not datasets.two_triangles()


def test_all_lists_every_loader():
    assert sorted(datasets.__all__) == sorted(name for name, _, _, _ in DATASETS)


def test_bipartite_start_id_is_set():
    assert datasets.bipartite().bipartite_start_id == 4


def test_states_network_is_higher_order():
    # The file declares 5 physical vertices carrying 6 state nodes; num_nodes
    # reports state nodes for higher-order networks.
    assert datasets.states().num_nodes == 6


def test_directed_default_and_run_override():
    # modular_wd bakes --flow-model directed into its Core. Read undirected,
    # its asymmetric arc pairs accumulate to exactly modular_w, so:
    # - the default (directed) run must differ from an undirected override,
    #   proving the baked flag is active, and
    # - the undirected override must match modular_w, proving per-run options
    #   win over the construction options.
    directed = datasets.modular_wd().run(options={"seed": 123})
    overridden = datasets.modular_wd().run(
        options={"flow_model": "undirected", "seed": 123}
    )
    undirected_twin = datasets.modular_w().run(options={"seed": 123})

    assert directed.codelength != pytest.approx(overridden.codelength)
    assert overridden.codelength == pytest.approx(undirected_twin.codelength)


def test_directed_loaders_bake_directed_flow():
    # states.net has equal out-strength everywhere, so directed and undirected
    # flow coincide numerically; assert the effective engine config instead.
    for loader in (datasets.states, datasets.modular_wd):
        net = loader()
        net.run(options={"seed": 123})
        assert not net._core.isUndirectedFlow()

    plain = datasets.two_triangles()
    plain.run(options={"seed": 123})
    assert plain._core.isUndirectedFlow()


def test_directed_loader_exports_directed_graph():
    nx = pytest.importorskip("networkx")
    from infomap import to_networkx

    directed = to_networkx(datasets.modular_wd().run(options={"seed": 123}))
    assert isinstance(directed, nx.DiGraph)

    undirected = to_networkx(datasets.modular_w().run(options={"seed": 123}))
    assert isinstance(undirected, nx.Graph)
    assert not isinstance(undirected, nx.DiGraph)


def test_packaged_data_matches_examples_networks(test_paths):
    # The packaged copies must stay byte-identical to the canonical files in
    # examples/networks/. To fix a failure, re-copy the canonical file:
    #   cp examples/networks/<name>.net interfaces/python/src/infomap/datasets/data/
    examples = test_paths.example_networks
    if not examples.is_dir():
        pytest.skip("examples/networks not available (installed-package run)")

    data_root = files("infomap.datasets") / "data"
    expected_names = {file_name for _, file_name, _, _ in DATASETS}

    for file_name in sorted(expected_names):
        canonical = examples / file_name
        assert canonical.is_file(), f"missing canonical file {canonical}"
        with as_file(data_root / file_name) as packaged:
            assert packaged.read_bytes() == canonical.read_bytes(), (
                f"{file_name} is out of sync with examples/networks/; "
                f"re-copy the canonical file into datasets/data/"
            )

    packaged_names = {
        entry.name for entry in data_root.iterdir() if entry.name.endswith(".net")
    }
    assert packaged_names == expected_names
