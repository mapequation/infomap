"""Cluster data on state and multilayer networks, scored against the reference.

The coverage map in #1224 (item 6) lists a cluster file on a multilayer
network, tree rows with state ids, and the memory map equation's physical-node
bookkeeping under a multi-level tree as never run. Each test reads a partition
chosen here with ``--no-infomap`` and checks two things: the run keeps exactly
that partition, and its codelength is the one ``reference_codelength`` gives
for it. The reference merges the states of a physical node within a module
(the memory map equation), so a partition that splits or joins a physical
node's states is where the two can disagree.
"""

from __future__ import annotations

from pathlib import Path

import pytest
from infomap import run
from reference_codelength import Leaf, Partition, codelength, tree_depth

pytestmark = pytest.mark.fast

REPO = Path(__file__).resolve().parents[2]
EXAMPLES = REPO / "examples" / "networks"
STATES = EXAMPLES / "states.net"
MULTILAYER = EXAMPLES / "multilayer_intra_inter.net"

REL = 1e-10
ABS = 1e-12


def _run(network, cluster_file, **options):
    return run(
        str(network),
        cluster_data=str(cluster_file),
        no_infomap=True,
        num_trials=1,
        seed=7,
        **options,
    )


def _leaves(result):
    return [node for node in result.tree(states=True) if node.is_leaf]


def _assert_reference(result, *, directed):
    leaves = {
        node.state_id: Leaf(
            path=tuple(node.path), flow=node.flow, physical_id=node.node_id
        )
        for node in _leaves(result)
    }
    partition = Partition(
        leaves=leaves,
        links=list(result.links("flow")),
        undirected_links=not directed,
        memory=True,
    )
    reference = codelength(partition)
    assert result.num_levels == tree_depth(partition)
    assert result.codelength == pytest.approx(reference.total, rel=REL, abs=ABS)


def _grouping(paths):
    """The nested modules as sets of leaves per level, free of module ids.

    Infomap renumbers modules and leaves by flow, so only which leaves share a
    module at each level is compared.
    """
    depth = max(len(path) for path in paths.values())
    levels = []
    for level in range(1, depth):
        groups = {}
        for key, path in paths.items():
            if len(path) > level:
                groups.setdefault(path[:level], set()).add(key)
        levels.append({frozenset(group) for group in groups.values()})
    return levels


def _modules_by(result, key):
    return _grouping({key(node): tuple(node.path) for node in _leaves(result)})


def _modules(tree):
    return _grouping(tree)


# --- state network, tree rows with state ids --------------------------------

# states.net: states 1 and 4 are physical node 1, the rest one state each.
STATE_TREES = {
    # States 1 and 4 share a module: one codeword for physical node 1.
    "two-level-joined": {
        1: (1, 1),
        4: (1, 2),
        2: (1, 3),
        3: (2, 1),
        5: (2, 2),
        6: (2, 3),
    },
    # States 1 and 4 in different modules.
    "two-level-split": {
        1: (1, 1),
        2: (1, 2),
        3: (1, 3),
        4: (2, 1),
        5: (2, 2),
        6: (2, 3),
    },
    # Same top module, different sub-modules.
    "three-level-split-below": {
        1: (1, 1, 1),
        2: (1, 1, 2),
        3: (1, 1, 3),
        4: (1, 2, 1),
        5: (1, 2, 2),
        6: (2, 1),
    },
    # Same sub-module.
    "three-level-joined": {
        1: (1, 1, 1),
        4: (1, 1, 2),
        2: (1, 2, 1),
        3: (1, 2, 2),
        5: (2, 1),
        6: (2, 2),
    },
}
STATE_PHYSICAL = {1: 1, 2: 2, 3: 3, 4: 1, 5: 4, 6: 5}


def _write_state_tree(path, tree):
    lines = [
        f'{":".join(map(str, p))} 0 "s{s}" {s} {STATE_PHYSICAL[s]}'
        for s, p in sorted(tree.items(), key=lambda item: item[1])
    ]
    path.write_text("\n".join(lines) + "\n")
    return path


@pytest.mark.parametrize("directed", [False, True], ids=["undirected", "directed"])
@pytest.mark.parametrize("tree", STATE_TREES)
def test_state_tree_with_state_ids(tmp_path, tree, directed):
    cluster_file = _write_state_tree(tmp_path / "seed.tree", STATE_TREES[tree])
    result = _run(STATES, cluster_file, directed=directed)
    assert _modules_by(result, lambda n: n.state_id) == _modules(STATE_TREES[tree])
    _assert_reference(result, directed=directed)


def test_state_network_physical_tree_keeps_unsplit_nodes(tmp_path):
    # A physical tree cannot say which state of a split node goes where (#908),
    # but a physical node with one row puts all its states in that module.
    cluster_file = tmp_path / "physical.tree"
    cluster_file.write_text(
        '1:1 0 "i" 1\n1:2 0 "j" 2\n2:1 0 "k" 3\n2:2 0 "l" 4\n2:3 0 "m" 5\n'
    )
    result = _run(STATES, cluster_file)
    modules = {node.state_id: node.path[0] for node in _leaves(result)}
    assert modules == {1: 1, 4: 1, 2: 1, 3: 2, 5: 2, 6: 2}
    _assert_reference(result, directed=False)


# --- multilayer network ------------------------------------------------------

# multilayer_intra_inter.net: layer 1 has nodes 1, 4, 5 and layer 2 nodes 1, 2,
# 3. Physical node 1 is in both layers.
MULTILAYER_PARTITIONS = {
    "split": {(1, 1): 2, (1, 4): 1, (1, 5): 1, (2, 1): 1, (2, 2): 2, (2, 3): 2},
    "joined": {(1, 1): 1, (1, 4): 1, (1, 5): 2, (2, 1): 1, (2, 2): 2, (2, 3): 2},
}


@pytest.mark.parametrize("partition", MULTILAYER_PARTITIONS)
def test_multilayer_clu_with_layer_ids(tmp_path, partition):
    modules = MULTILAYER_PARTITIONS[partition]
    # The state ids in the file are not the network's: rows are matched by
    # layer and node id. A row for a node the network does not have in that
    # layer is skipped.
    lines = [
        f"{100 + i} {module} 0 {node} {layer}"
        for i, ((layer, node), module) in enumerate(sorted(modules.items()))
    ]
    lines.append("200 3 0 4 2")
    cluster_file = tmp_path / "seed.clu"
    cluster_file.write_text("\n".join(lines) + "\n")
    result = _run(MULTILAYER, cluster_file, two_level=True)
    got = _modules_by(result, lambda n: (n.layer_id, n.node_id))
    assert got == _grouping({state: (module, 1) for state, module in modules.items()})
    _assert_reference(result, directed=True)


MULTILAYER_TREES = {
    "three-level-split-below": {
        (1, 1): (1, 1, 1),
        (1, 4): (1, 1, 2),
        (2, 1): (1, 2, 1),
        (2, 2): (1, 2, 2),
        (1, 5): (2, 1),
        (2, 3): (2, 2),
    },
    "three-level-joined": {
        (1, 1): (1, 1, 1),
        (2, 1): (1, 1, 2),
        (1, 4): (1, 2, 1),
        (1, 5): (1, 2, 2),
        (2, 2): (2, 1),
        (2, 3): (2, 2),
    },
}


@pytest.mark.parametrize("tree", MULTILAYER_TREES)
def test_multilayer_tree_with_layer_ids(tmp_path, tree):
    paths = MULTILAYER_TREES[tree]
    lines = [
        f'{":".join(map(str, p))} 0 "n{node}" {100 + i} {node} {layer}'
        for i, ((layer, node), p) in enumerate(
            sorted(paths.items(), key=lambda kv: kv[1])
        )
    ]
    cluster_file = tmp_path / "seed.tree"
    cluster_file.write_text("\n".join(lines) + "\n")
    result = _run(MULTILAYER, cluster_file)
    assert _modules_by(result, lambda n: (n.layer_id, n.node_id)) == _modules(paths)
    _assert_reference(result, directed=True)
