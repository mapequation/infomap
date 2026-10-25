"""Tree input edge cases, scored against the reference.

The coverage map in #1224 (item 7) lists tree rows for nodes that are not in
the network, and ``--assign-to-neighbouring-module`` with tree input, as never
run. Each test reads a tree with ``--no-infomap`` and checks the partition the
run keeps against the one the tree and the option text describe, and the
codelength against ``reference_codelength``.
"""

from __future__ import annotations

import subprocess
import sys
import textwrap
from pathlib import Path

import pytest
from infomap import InfomapError, run
from reference_codelength import Leaf, Partition, codelength, tree_depth

pytestmark = pytest.mark.fast

REPO = Path(__file__).resolve().parents[2]
TWOTRIANGLES = REPO / "examples" / "networks" / "twotriangles.net"

REL = 1e-10
ABS = 1e-12

# twotriangles.net: triangles 1-2-3 and 4-5-6, bridged by 3-4.


def _write_tree(path, rows, *, state_ids=False):
    lines = []
    for node, tree_path in rows:
        line = f'{":".join(map(str, tree_path))} 0 "{node}" {node}'
        lines.append(f"{line} {node}" if state_ids else line)
    path.write_text("\n".join(lines) + "\n")
    return path


def _run(tree, **options):
    return run(
        str(TWOTRIANGLES),
        cluster_data=str(tree),
        no_infomap=True,
        num_trials=1,
        seed=7,
        **options,
    )


def _leaves(result):
    return [node for node in result.tree() if node.is_leaf]


def _grouping(paths):
    """Which leaves share a module at each level, free of module ids."""
    depth = max(len(path) for path in paths.values())
    levels = []
    for level in range(1, depth):
        groups = {}
        for key, path in paths.items():
            if len(path) > level:
                groups.setdefault(path[:level], set()).add(key)
        levels.append({frozenset(group) for group in groups.values()})
    return levels


def _assert_partition(result, expected):
    got = {node.node_id: tuple(node.path) for node in _leaves(result)}
    assert _grouping(got) == _grouping(expected)
    leaves = {
        node.node_id: Leaf(path=tuple(node.path), flow=node.flow)
        for node in _leaves(result)
    }
    partition = Partition(
        leaves=leaves, links=list(result.links("flow")), undirected_links=True
    )
    assert result.num_levels == tree_depth(partition)
    assert result.codelength == pytest.approx(
        codelength(partition).total, rel=REL, abs=ABS
    )


THREE_LEVEL = [
    (1, (1, 1, 1)),
    (2, (1, 1, 2)),
    (3, (1, 2, 1)),
    (4, (2, 1, 1)),
    (5, (2, 1, 2)),
    (6, (2, 2, 1)),
]
TWO_LEVEL = [(n, (p[0], p[2])) for n, p in THREE_LEVEL]


@pytest.mark.parametrize("state_ids", [False, True], ids=["physical", "state-ids"])
@pytest.mark.parametrize(
    "rows", [TWO_LEVEL, THREE_LEVEL], ids=["two-level", "three-level"]
)
def test_rows_for_nodes_not_in_network_are_skipped(tmp_path, rows, state_ids):
    extra = [(7, (1, 1, 3)[: len(rows[0][1])]), (42, (3, 1, 1)[: len(rows[0][1])])]
    tree = _write_tree(tmp_path / "seed.tree", rows + extra, state_ids=state_ids)
    _assert_partition(_run(tree), dict(rows))


def test_only_rows_for_nodes_not_in_network(tmp_path):
    # Every node is missing from the tree, so each gets a module of its own.
    tree = _write_tree(tmp_path / "seed.tree", [(7, (1, 1, 1)), (8, (1, 2, 1))])
    result = _run(tree)
    _assert_partition(result, {n: (n, 1) for n in range(1, 7)})


# Nodes 3 and 4 are missing. The option text: "With --cluster-data, assign
# nodes missing module ids to a neighboring node's module when possible."
# Orphans are placed in node order, out-links first: node 3
# links out only to node 4, which has no module yet, so it takes node 1's (an
# in-link), sub-module 1:1. Node 4 links out to node 5 first, sub-module 2:1.
ORPHANS = [(n, p) for n, p in THREE_LEVEL if n not in (3, 4)]


@pytest.mark.parametrize("assign", [False, True], ids=["own-module", "neighbour"])
def test_missing_nodes_in_three_level_tree(tmp_path, assign):
    tree = _write_tree(tmp_path / "seed.tree", ORPHANS)
    result = _run(tree, assign_to_neighbouring_module=assign)
    paths = {node.node_id: tuple(node.path) for node in _leaves(result)}
    if assign:
        # Each orphan shares the deepest module of one of its neighbours.
        neighbours = {3: (1, 2, 4), 4: (3, 5, 6)}
        for orphan, candidates in neighbours.items():
            assert any(paths[orphan][:-1] == paths[n][:-1] for n in candidates), orphan
        expected = dict(ORPHANS) | {3: (1, 1, 9), 4: (2, 1, 9)}
    else:
        expected = dict(ORPHANS) | {3: (8, 1), 4: (9, 1)}
    _assert_partition(result, expected)


# --- repeated ids ------------------------------------------------------------


def _subprocess_run(tree):
    # A refusal of the file is caught in the child, so only a native failure
    # gives a non-zero exit. A sanitizer failure can also exit 1.
    script = textwrap.dedent(
        f"""
        from infomap import InfomapError, run
        try:
            run({str(TWOTRIANGLES)!r}, cluster_data={str(tree)!r}, no_infomap=True,
                num_trials=1, silent=True)
        except InfomapError:
            pass
        """
    )
    return subprocess.run(
        [sys.executable, "-c", script],
        check=False,
        capture_output=True,
        text=True,
        timeout=120,
    )


def test_repeated_physical_row_keeps_the_first(tmp_path):
    tree = _write_tree(tmp_path / "seed.tree", THREE_LEVEL + [(3, (2, 2, 2))])
    _assert_partition(_run(tree), dict(THREE_LEVEL))


def test_repeated_state_row_is_refused(tmp_path):
    # It used to be a segmentation fault (#1335), so the crash check runs in a
    # subprocess before the refusal is checked here.
    tree = _write_tree(
        tmp_path / "seed.tree", THREE_LEVEL + [(3, (2, 2, 2))], state_ids=True
    )
    completed = _subprocess_run(tree)
    assert completed.returncode == 0, completed.stderr
    with pytest.raises(InfomapError, match="State id 3 appears more than once"):
        _run(tree)
