"""Golden-output parity: the new bulk-extraction ``Result`` surface must be
byte-identical to the legacy per-node ``Infomap`` accessors across
representative networks (simple, multilayer, state, bipartite, meta).

``src/`` is the runtime authority; this gate proves the internal compute path
change (single C++ traversal -> snapshot) introduces no numeric or structural
drift.
"""

from __future__ import annotations

import pytest

from infomap import Infomap

pytest.importorskip("pandas")


def _simple(example_network_path) -> Infomap:
    im = Infomap(silent=True, no_file_output=True, num_trials=5)
    im.read_file(str(example_network_path("twotriangles.net")))
    return im


def _nine(example_network_path) -> Infomap:
    im = Infomap(silent=True, no_file_output=True, num_trials=10)
    im.read_file(str(example_network_path("ninetriangles.net")))
    return im


def _states(example_network_path) -> Infomap:
    im = Infomap(silent=True, no_file_output=True)
    im.read_file(str(example_network_path("states.net")))
    return im


def _multilayer(example_network_path) -> Infomap:
    im = Infomap(silent=True, no_file_output=True)
    im.read_file(str(example_network_path("multilayer.net")))
    return im


def _bipartite(example_network_path) -> Infomap:
    im = Infomap(silent=True, no_file_output=True)
    im.read_file(str(example_network_path("bipartite.net")))
    return im


def _meta(example_network_path) -> Infomap:
    im = Infomap(silent=True, no_file_output=True)
    for source, target in [(0, 1), (1, 2), (2, 0), (2, 3), (3, 4), (4, 5), (5, 3)]:
        im.add_link(source, target)
    for node_id in range(6):
        im.set_meta_data(node_id, node_id % 2)
    return im


_BUILDERS = {
    "simple": _simple,
    "nine": _nine,
    "states": _states,
    "multilayer": _multilayer,
    "bipartite": _bipartite,
    "meta": _meta,
}

# Column / option variants exercised for to_dataframe parity.
_DATAFRAME_VARIANTS = [
    {},
    {"states": True},
    {"columns": ["node_id", "module_id"]},
    {"columns": ["node_id", "community", "flow"]},
    {"columns": ["path", "flow", "name", "node_id"]},
    {"columns": ["node_id", "state_id", "depth", "layer_id", "child_index", "flow"]},
    {
        "columns": ["node_id", "community", "flow"],
        "index": "node_id",
        "sort": ["community", "flow"],
    },
    {"columns": ["node_id", "module_id"], "depth_level": 2, "sort": True},
    {"columns": ["node_id", "module_id"], "level": -1},
    {"columns": ["node_id", "name"]},
]


@pytest.mark.parametrize("network", sorted(_BUILDERS))
def test_to_dataframe_parity(network, example_network_path):
    im = _BUILDERS[network](example_network_path)
    result = im.run()

    for variant in _DATAFRAME_VARIANTS:
        legacy = im.to_dataframe(**variant)
        new = result.to_dataframe(**variant)
        assert list(new.columns) == list(legacy.columns), (network, variant)
        assert list(new.index) == list(legacy.index), (network, variant)
        assert new.equals(legacy), (network, variant)
        # Dtypes (path -> object tuples, ids -> int, flow -> float) must match.
        assert list(new.dtypes) == list(legacy.dtypes), (network, variant)


@pytest.mark.parametrize("network", sorted(_BUILDERS))
def test_modules_parity(network, example_network_path):
    im = _BUILDERS[network](example_network_path)
    result = im.run()

    for depth in (1, 2, -1):
        for states in (False, True):
            try:
                legacy = im.get_modules(depth, states)
                legacy_error = None
            except RuntimeError as exc:
                legacy, legacy_error = None, str(exc)

            if legacy_error is not None:
                with pytest.raises(RuntimeError, match="higher-order"):
                    result.modules(depth, states=states)
            else:
                assert result.modules(depth, states=states) == legacy


@pytest.mark.parametrize("network", sorted(_BUILDERS))
def test_nodes_parity(network, example_network_path):
    im = _BUILDERS[network](example_network_path)
    result = im.run()

    for states in (False, True):
        legacy = [
            (
                node.node_id,
                node.state_id,
                node.module_id,
                node.flow,
                node.depth,
                node.layer_id,
                node.child_index,
                tuple(node.path),
            )
            for node in im.get_nodes(depth_level=1, states=states)
        ]
        new = [
            (
                node.node_id,
                node.state_id,
                node.module_id,
                node.flow,
                node.depth,
                node.layer_id,
                node.child_index,
                tuple(node.path),
            )
            for node in result.nodes(1, states=states)
        ]
        assert new == legacy, (network, states)


@pytest.mark.parametrize("network", sorted(_BUILDERS))
def test_scalar_parity(network, example_network_path):
    im = _BUILDERS[network](example_network_path)
    result = im.run()

    assert result.codelength == im.codelength
    assert result.num_top_modules == im.num_top_modules
    assert result.num_non_trivial_top_modules == im.num_non_trivial_top_modules
    assert result.num_levels == im.num_levels
    assert result.index_codelength == im.index_codelength
    assert result.module_codelength == im.module_codelength
    assert result.one_level_codelength == im.one_level_codelength
    assert result.relative_codelength_savings == im.relative_codelength_savings
    assert result.entropy_rate == im.entropy_rate
    assert result.meta_codelength == im.meta_codelength
    assert result.have_memory == im.have_memory
    assert result.names == im.names
