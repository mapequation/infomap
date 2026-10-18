"""Large networks and deep hierarchies, each run under a time limit (#1252).

Every run happens in a child process with a timeout, so a hang fails the test
at the limit and a crash or stack overflow fails it with the signal instead of
taking pytest down. The parent checks what the run returned:

- the tree is valid: each node is one leaf, the leaf flow sums to 1, the
  children of each module are numbered 1 to k, and a module holds either
  leaves or modules;
- the reported codelength, one-level codelength and number of levels match
  the independent recomputation in ``reference_codelength``;
- the codelength is no worse than the one-level codelength.

Networks are generated: planted partitions of 10^4 (fast) and 2 * 10^5 (slow)
nodes, nested blocks whose search finds a hierarchy many levels deep, and a
caterpillar tree read as cluster data. The search finds about log(n) levels,
so the caterpillar is what drives the C++ functions that recurse over the tree
(sorting children on flow, the ``InfoNode`` destructor chain and others) to
depth: it puts node ``i`` at depth ``i``. None of them has a depth guard. A
depth of 1000 runs in a 128 KB stack, so overflowing the default 8 MB one
takes a depth near 65,000. A tree file of depth d has O(d^2) characters, and
reading it takes O(n d^2 log n) time, so input that deep is out of reach. The
reference recomputation is O(n d^2) too, which caps the slow test at 1000.

The time limits are about ten times the local run time, so they catch a hang
or a large slowdown, not a small regression.
"""

from __future__ import annotations

import os
import pickle
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np
import pytest
from reference_codelength import (
    Leaf,
    Partition,
    codelength,
    one_module,
    total_degree,
    tree_depth,
)

# Floating-point sums over 10^5 terms differ in the last digits between the
# engine and the reference, so the tolerance is looser than in
# test_reference_codelength.
REL = 1e-9
ABS = 1e-12

# The child process runs Infomap and pickles what the parent checks.
CHILD = """
import pickle, sys
from infomap import run
network, out, options = sys.argv[1], sys.argv[2], eval(sys.argv[3])
result = run(network, silent=True, **options)
data = {
    "codelength": result.codelength,
    "one_level_codelength": result.one_level_codelength,
    "num_levels": result.num_levels,
    "leaves": [
        (node.state_id, node.node_id, tuple(node.path), node.flow)
        for node in result.tree(states=True)
        if node.is_leaf
    ],
    "flow_links": list(result.links("flow")),
    "weight_links": list(result.links("weight")),
}
with open(out, "wb") as f:
    pickle.dump(data, f)
"""


def run_limited(network: Path, timeout: float, **options) -> dict:
    """Run Infomap on ``network`` in a child process and return what it reports."""
    out = network.with_suffix(".pickle")
    env = {**os.environ}
    # A fixed thread budget, so the time limit does not depend on the runner, and
    # two threads, so inner parallelization does not fall back to the serial loop.
    env.pop("INFOMAP_NUM_THREADS", None)
    env.pop("SLURM_CPUS_PER_TASK", None)
    env["OMP_NUM_THREADS"] = "2"
    try:
        proc = subprocess.run(
            [sys.executable, "-c", CHILD, str(network), str(out), repr(options)],
            env=env,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired:
        pytest.fail(f"Infomap did not finish within {timeout} s on {network.name}")
    assert proc.returncode == 0, (
        f"Infomap exited with {proc.returncode} on {network.name}:\n{proc.stderr}"
    )
    with open(out, "rb") as f:
        return pickle.load(f)


def check_invariants(data: dict, nodes: set[int], *, undirected: bool) -> Partition:
    leaves = data["leaves"]
    node_ids = [node_id for _, node_id, _, _ in leaves]
    assert len(node_ids) == len(set(node_ids)), "a node is in the tree twice"
    assert set(node_ids) == nodes
    paths = [path for _, _, path, _ in leaves]
    assert len(paths) == len(set(paths)), "two leaves share a path"

    flows = np.array([flow for _, _, _, flow in leaves])
    assert (flows >= 0).all()
    assert flows.sum() == pytest.approx(1.0, rel=REL)

    # The children of each module are numbered 1..k, and a module holds either
    # leaves or modules, never both.
    children = defaultdict(set)
    leaf_parents, module_parents = set(), set()
    for _, _, path, _ in leaves:
        assert len(path) >= 1 and all(index >= 1 for index in path), path
        for depth in range(len(path)):
            children[path[:depth]].add(path[depth])
        leaf_parents.add(path[:-1])
        module_parents.update(path[:depth] for depth in range(len(path) - 1))
    for parent, indices in children.items():
        assert indices == set(range(1, len(indices) + 1)), parent
    assert not leaf_parents & module_parents

    partition = Partition(
        leaves={
            state_id: Leaf(path=path, flow=flow, physical_id=node_id)
            for state_id, node_id, path, flow in leaves
        },
        links=data["flow_links"],
        undirected_links=undirected,
        total_degree=total_degree(data["weight_links"]),
    )
    assert data["num_levels"] == tree_depth(partition)
    reference = codelength(partition)
    assert data["codelength"] == pytest.approx(reference.total, rel=REL, abs=ABS)
    one_level = codelength(one_module(partition)).total
    assert data["one_level_codelength"] == pytest.approx(one_level, rel=REL, abs=ABS)
    assert data["codelength"] <= data["one_level_codelength"] * (1 + REL)
    return partition


# --- generators --------------------------------------------------------------


def write_links(path: Path, links: np.ndarray, weights: np.ndarray | None = None):
    """Write 0-based ``links`` as a 1-based link list."""
    with open(path, "w") as f:
        if weights is None:
            np.savetxt(f, links + 1, fmt="%d")
        else:
            table = np.column_stack([links + 1, weights])
            np.savetxt(f, table, fmt=["%d", "%d", "%.10g"])
    return path


def planted_partition(n: int, seed: int, *, degree: int = 10, mixing: float = 0.3):
    """``n`` nodes in groups of 20 to 200, each node with about ``degree`` links.

    Returns the links, without self-links. A fraction ``mixing`` of each node's
    links goes to a uniformly random node, the rest stays in its group.
    """
    rng = np.random.default_rng(seed)
    sizes = []
    while sum(sizes) < n:
        sizes.append(int(rng.integers(20, 201)))
    sizes[-1] -= sum(sizes) - n
    starts = np.repeat(np.cumsum([0] + sizes[:-1]), sizes)
    group_sizes = np.repeat(sizes, sizes)
    half = degree // 2
    sources = np.repeat(np.arange(n), half)
    inside = rng.random(sources.size) >= mixing
    targets = np.where(
        inside,
        starts[sources] + rng.integers(0, group_sizes[sources]),
        rng.integers(0, n, sources.size),
    )
    keep = sources != targets
    return np.column_stack([sources[keep], targets[keep]])


def nested_blocks(branching: int, depth: int, seed: int, *, decay: float = 0.1):
    """``branching ** depth`` nodes in blocks nested ``depth`` levels deep.

    The smallest blocks are cliques of ``branching`` nodes. At each level up,
    sibling blocks form a ring, each joined to the next by two links whose
    weight shrinks by ``decay`` per level, so every level is a weaker module
    around the one below it.
    """
    rng = np.random.default_rng(seed)
    n = branching**depth
    links, weights = [], []
    i, j = np.triu_indices(branching, 1)
    groups = np.arange(0, n, branching)[:, None]
    links.append(np.column_stack([(groups + i).ravel(), (groups + j).ravel()]))
    weights.append(np.ones(links[-1].shape[0]))
    weight = 1.0
    for level in range(1, depth):
        weight *= decay
        size = branching**level
        parents = np.arange(0, n, size * branching)
        for _ in range(2):
            for k in range(branching):
                a = parents + k * size + rng.integers(0, size, parents.size)
                b = (
                    parents
                    + (k + 1) % branching * size
                    + rng.integers(0, size, parents.size)
                )
                links.append(np.column_stack([a, b]))
                weights.append(np.full(parents.size, weight * branching))
    return np.concatenate(links), np.concatenate(weights)


def caterpillar(directory: Path, depth: int) -> tuple[Path, Path]:
    """A path of ``depth`` nodes and a tree file that puts node ``i`` at depth ``i``.

    Module ``2:2:...:2`` (``i`` twos) holds node ``i + 1`` in its child module 1
    and the rest of the chain in its child module 2, so the tree is ``depth``
    levels deep and every module holds modules or a single leaf.
    """
    network = directory / f"caterpillar{depth}.net"
    write_links(network, np.column_stack([np.arange(depth - 1), np.arange(1, depth)]))
    tree = directory / f"caterpillar{depth}.tree"
    with open(tree, "w") as f:
        for node in range(1, depth + 1):
            if node < depth:
                path = [2] * (node - 1) + [1, 1]
            else:
                path = [2] * (depth - 1) + [1]
            f.write(f'{":".join(map(str, path))} 0 "{node}" {node}\n')
    return network, tree


# --- fast: in every PR run ---------------------------------------------------


@pytest.mark.fast
@pytest.mark.parametrize("directed", [False, True], ids=["undirected", "directed"])
def test_planted_partition_ten_thousand_nodes(tmp_path, directed):
    n = 10_000
    network = write_links(tmp_path / "planted.net", planted_partition(n, seed=1))
    data = run_limited(network, timeout=60, directed=directed, num_trials=2, seed=3)
    check_invariants(data, set(range(1, n + 1)), undirected=not directed)


@pytest.mark.fast
@pytest.mark.parametrize("seed", [1, 2, 3])
def test_nested_blocks_give_a_deep_hierarchy(tmp_path, seed):
    branching, depth = 3, 7
    links, weights = nested_blocks(branching, depth, seed)
    network = write_links(tmp_path / "nested.net", links, weights)
    data = run_limited(network, timeout=60, num_trials=1, seed=seed)
    check_invariants(data, set(range(1, branching**depth + 1)), undirected=True)
    # Without the nesting the test would not reach the deep recursion.
    assert data["num_levels"] >= 5


@pytest.mark.fast
def test_caterpillar_tree_is_scored_at_full_depth(tmp_path):
    depth = 300
    network, tree = caterpillar(tmp_path, depth)
    data = run_limited(network, timeout=60, cluster_data=str(tree), no_infomap=True)
    check_invariants(data, set(range(1, depth + 1)), undirected=True)
    assert data["num_levels"] == depth


@pytest.mark.fast
def test_search_from_a_caterpillar_tree(tmp_path):
    depth = 300
    network, tree = caterpillar(tmp_path, depth)
    data = run_limited(network, timeout=60, cluster_data=str(tree), num_trials=2)
    check_invariants(data, set(range(1, depth + 1)), undirected=True)


# --- slow: in the weekly --run-slow job --------------------------------------


@pytest.mark.slow
@pytest.mark.parametrize("directed", [False, True], ids=["undirected", "directed"])
def test_planted_partition_two_hundred_thousand_nodes(tmp_path, directed):
    n = 200_000
    network = write_links(tmp_path / "planted.net", planted_partition(n, seed=2))
    data = run_limited(network, timeout=600, directed=directed, num_trials=2, seed=5)
    check_invariants(data, set(range(1, n + 1)), undirected=not directed)


@pytest.mark.slow
def test_inner_parallelization_two_hundred_thousand_nodes(tmp_path):
    n = 200_000
    network = write_links(tmp_path / "planted.net", planted_partition(n, seed=3))
    data = run_limited(
        network, timeout=600, inner_parallelization=True, num_trials=1, seed=7
    )
    check_invariants(data, set(range(1, n + 1)), undirected=True)


@pytest.mark.slow
def test_nested_blocks_large_and_deep(tmp_path):
    branching, depth = 3, 11
    links, weights = nested_blocks(branching, depth, seed=4)
    network = write_links(tmp_path / "nested.net", links, weights)
    data = run_limited(network, timeout=600, num_trials=1, seed=4)
    check_invariants(data, set(range(1, branching**depth + 1)), undirected=True)
    assert data["num_levels"] >= 7


@pytest.mark.slow
@pytest.mark.parametrize("no_infomap", [True, False], ids=["scored", "search"])
def test_caterpillar_tree_a_thousand_levels(tmp_path, no_infomap):
    depth = 1000
    network, tree = caterpillar(tmp_path, depth)
    data = run_limited(
        network, timeout=600, cluster_data=str(tree), no_infomap=no_infomap
    )
    check_invariants(data, set(range(1, depth + 1)), undirected=True)
    if no_infomap:
        assert data["num_levels"] == depth
