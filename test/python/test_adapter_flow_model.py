"""The flow model a graph adapter infers must survive option rendering.

The adapters resolve directedness from the input (``g.is_directed()`` for the
graph libraries, an explicit ``directed=`` for scipy/edge_index). Options reach
the engine as a rendered argument string that the engine re-parses into a fresh
config, so an inferred flow model that lives only on the engine is discarded the
moment any per-run option changes that string. These tests pin the inference to
the *result*, not to the mechanism: the same input must give the same partition
whether or not an unrelated keyword such as ``seed`` was passed.

Explicit configuration always wins over the inference, whether it is given at
construction, on the options carrier, or as a per-run keyword.
"""

from __future__ import annotations

import infomap
import networkx as nx
import pytest

pytestmark = pytest.mark.fast

# A directed graph whose directed and undirected flow models disagree.
_EDGES = [(1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 6), (6, 4)]


def _reference(flow_model: str, *, seed: int = 42, num_trials: int = 1) -> float:
    """Codelength of the same links with the flow model stated explicitly."""
    return infomap.run(
        list(_EDGES),
        seed=seed,
        num_trials=num_trials,
        options=infomap.Options(flow_model=flow_model),
    ).codelength


@pytest.fixture(scope="module")
def directed_codelength() -> float:
    return _reference("directed")


@pytest.fixture(scope="module")
def undirected_codelength() -> float:
    return _reference("undirected")


@pytest.fixture(scope="module")
def directed_codelength_at_defaults() -> float:
    """Reference for the runs that pass no keywords at all.

    Those runs take the engine defaults, so the reference has to use the same
    ones -- comparing them against the seed=42 reference above would pass only
    while both seeds happen to find the same optimum. Read the defaults from
    ``Options()`` so this follows a change to either default.
    """
    defaults = infomap.Options()
    return _reference("directed", seed=defaults.seed, num_trials=defaults.num_trials)


@pytest.fixture
def digraph() -> nx.DiGraph:
    return nx.DiGraph(_EDGES)


def test_references_disagree(directed_codelength, undirected_codelength):
    # Guards the fixture itself: if these ever coincide the assertions below
    # would pass without testing anything.
    assert directed_codelength != pytest.approx(undirected_codelength)


def test_functional_run_infers_directed(digraph, directed_codelength):
    result = infomap.run(digraph, seed=42, num_trials=1)

    assert result.codelength == pytest.approx(directed_codelength)


def test_network_constructor_infers_directed(digraph, directed_codelength):
    result = infomap.Network.from_networkx(digraph).run(seed=42, num_trials=1)

    assert result.codelength == pytest.approx(directed_codelength)


def test_network_constructor_infers_directed_without_keywords(
    digraph, directed_codelength_at_defaults
):
    result = infomap.Network.from_networkx(digraph).run()

    assert result.codelength == pytest.approx(directed_codelength_at_defaults)


def test_builder_infers_directed_without_keywords(
    digraph, directed_codelength_at_defaults
):
    im = infomap.Infomap()
    im.add_networkx_graph(digraph)

    assert im.run().codelength == pytest.approx(directed_codelength_at_defaults)


def test_builder_inference_survives_an_unrelated_keyword(digraph, directed_codelength):
    # The regression: passing *any* keyword changed the rendered argument
    # string, so the engine was reconfigured from that string alone and the
    # adapter's directedness was lost.
    im = infomap.Infomap(num_trials=1)
    im.add_networkx_graph(digraph)

    assert im.run(seed=42).codelength == pytest.approx(directed_codelength)


@pytest.mark.parametrize(
    "overrides",
    [
        pytest.param({"flow_model": "undirected"}, id="flow_model"),
        pytest.param({"directed": False}, id="directed"),
    ],
)
def test_explicit_per_run_option_overrides_the_inference(
    digraph, undirected_codelength, overrides
):
    result = infomap.Network.from_networkx(digraph).run(
        seed=42, num_trials=1, **overrides
    )

    assert result.codelength == pytest.approx(undirected_codelength)


def test_explicit_options_carrier_overrides_the_inference(
    digraph, undirected_codelength
):
    result = infomap.Network.from_networkx(digraph).run(
        options=infomap.Options(flow_model="undirected", seed=42, num_trials=1)
    )

    assert result.codelength == pytest.approx(undirected_codelength)


def test_construction_time_flow_model_overrides_the_inference(
    digraph, undirected_codelength
):
    # The adapter must not override a flow model the caller chose when the
    # engine was created.
    im = infomap.Infomap(flow_model="undirected", num_trials=1)
    im.add_networkx_graph(digraph)

    assert im.run(seed=42).codelength == pytest.approx(undirected_codelength)


def test_undirected_input_is_not_forced_directed(undirected_codelength):
    graph = nx.Graph(_EDGES)

    result = infomap.Network.from_networkx(graph).run(seed=42, num_trials=1)

    assert result.codelength == pytest.approx(undirected_codelength)


def test_scipy_adapter_inference_survives_a_keyword(directed_codelength):
    sparse = pytest.importorskip("scipy.sparse")
    matrix = sparse.coo_matrix(
        (
            [1.0] * len(_EDGES),
            (
                [source - 1 for source, _ in _EDGES],
                [target - 1 for _, target in _EDGES],
            ),
        ),
        shape=(6, 6),
    ).tocsr()

    network = infomap.Network.from_scipy_sparse_matrix(matrix, directed=True)

    assert network.run(seed=42, num_trials=1).codelength == pytest.approx(
        directed_codelength
    )


def test_edge_index_adapter_inference_survives_a_keyword(directed_codelength):
    np = pytest.importorskip("numpy")
    edge_index = np.array(
        [[source - 1 for source, _ in _EDGES], [target - 1 for _, target in _EDGES]]
    )

    network = infomap.Network.from_edge_index(edge_index, num_nodes=6)

    assert network.run(seed=42, num_trials=1).codelength == pytest.approx(
        directed_codelength
    )
