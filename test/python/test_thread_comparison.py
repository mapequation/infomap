"""Threaded against serial results, on networks large enough for the parallel paths (#1258).

The thread checks in ``test_metamorphic.py`` (#1200) run on networks of at most
24 nodes. These generate planted hierarchies large enough that each parallel
path does real work, and compare the results.

What is promised, and what is asserted
--------------------------------------
The recursive search
    Every module of the multilevel search is an OpenMP task, and a sub-module
    with at least 16 children is a task of its own
    (``minChildDegreeForPartitionTask``). Each sub-Infomap re-seeds from the
    config seed, so with a fixed seed the result does not depend on the thread
    count or on the order the tasks run in. Asserted: the same codelengths and
    the same tree, bit for bit, for 1, 2 and 4 threads.

``--parallel-trials``
    Trial ``j`` runs with seed ``seed + j`` in every mode, so the run gives the
    result of a serial run (the HPC workflow docs). A worker rebuilds the
    network for each trial, so a sum can come out in the last bit (#1200).
    Asserted: the per-trial codelengths to 1e-14 and the same modules, for 1, 2
    and 4 workers.

``--inner-parallelization``
    Documented to "produce a different partition than serial optimization",
    so it is not compared with a serial run. It is deterministic for a fixed
    seed and thread count, which the C++ suite checks on 6 nodes, below the
    10,000 active nodes where the parallel sweep starts
    (``minNetworkSizeForInnerParallelization``). Asserted, on 10,000 nodes:
    two runs with 4 threads give the same codelength and tree, and the
    reported codelength is that of the tree, rescored with ``--no-infomap``.
    The sweep proposes moves read-only and commits them serially in the
    sweep's order, so today the result is also the same for any thread count
    from 2. Nothing documents that, so it is not asserted.

Every run passes ``num_threads`` and the module unsets ``SLURM_CPUS_PER_TASK``,
which outranks ``OMP_NUM_THREADS`` in the thread budget. The run report's
``threads_used`` shows that the threads ran. Without OpenMP every path is
serial and there is nothing to compare, so the tests skip.
"""

from __future__ import annotations

import json
import os
import random
from pathlib import Path

import pytest
from infomap import run

REL = 1e-10


def _threads_used(source, tmp_path: Path, **options) -> tuple[object, int]:
    report = tmp_path / "report.json"
    result = run(source, silent=True, timing_json=str(report), **options)
    return result, json.loads(report.read_text())["threads_used"]


def _has_openmp() -> bool:
    import tempfile

    with tempfile.TemporaryDirectory() as tmp:
        # Two triangles: the recursion opens a team even for a tiny network.
        links = [(1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 6), (6, 4)]
        return _threads_used(links, Path(tmp), num_threads=2, num_trials=1)[1] > 1


pytestmark = pytest.mark.slow

OPENMP = _has_openmp()


@pytest.fixture(autouse=True)
def _threaded(monkeypatch):
    if not OPENMP:
        # The weekly thread job sets this, so a build that lost OpenMP fails
        # there instead of skipping.
        if os.environ.get("INFOMAP_TEST_REQUIRE_OPENMP"):
            pytest.fail("the extension is built without OpenMP")
        pytest.skip("built without OpenMP")
    monkeypatch.delenv("SLURM_CPUS_PER_TASK", raising=False)


# --- networks ----------------------------------------------------------------------


def planted(branching, leaf_size, seed):
    """Links of a planted hierarchy of ``prod(branching) * leaf_size`` nodes.

    Each node draws four links in its leaf group, two in the group above and
    one in the group above that (self-links skipped, repeats kept), and with
    probability 0.3 one link to any node.
    """
    rng = random.Random(seed)
    sizes = [leaf_size]
    for b in reversed(branching):
        sizes.append(sizes[-1] * b)
    n = sizes[-1]
    links = []
    for source in range(n):
        for size, draws in zip(sizes, (4, 2, 1), strict=False):
            start = source - source % size
            for _ in range(draws):
                target = start + rng.randrange(size)
                if target != source:
                    links.append((source, target))
        if rng.random() < 0.3:
            links.append((source, rng.randrange(n)))
    return n, links


def write_links(path: Path, n, links) -> Path:
    lines = [f"*Vertices {n}"]
    lines += [f'{i + 1} "{i + 1}"' for i in range(n)]
    lines.append("*Links")
    lines += [f"{s + 1} {t + 1}" for s, t in links]
    path.write_text("\n".join(lines) + "\n")
    return path


def write_states(path: Path, n, links) -> Path:
    """Two state nodes per physical node; each link keeps or switches memory."""
    rng = random.Random(len(links))
    lines = [f"*Vertices {n}"]
    lines += [f'{i + 1} "{i + 1}"' for i in range(n)]
    lines.append("*States")
    lines += [f"{2 * i + m + 1} {i + 1}" for i in range(n) for m in (0, 1)]
    lines.append("*Links")
    lines += [
        f"{2 * s + rng.randrange(2) + 1} {2 * t + rng.randrange(2) + 1}"
        for s, t in links
    ]
    path.write_text("\n".join(lines) + "\n")
    return path


def write_multilayer(path: Path, n, links) -> Path:
    """Two layers that each keep about two thirds of the links."""
    rng = random.Random(len(links))
    lines = [f"*Vertices {n}", "*Intra"]
    for layer in (1, 2):
        lines += [f"{layer} {s + 1} {t + 1} 1" for s, t in links if rng.random() < 0.67]
    path.write_text("\n".join(lines) + "\n")
    return path


def _tree(result) -> dict[int, tuple[int, ...]]:
    return {node.state_id: tuple(node.path) for node in result.tree() if node.is_leaf}


def _modules(result) -> set[frozenset]:
    """Every module as its set of leaves, whatever its position in the tree."""
    members: dict[tuple[int, ...], set[int]] = {}
    for node, path in _tree(result).items():
        for depth in range(1, len(path)):
            members.setdefault(path[:depth], set()).add(node)
    return {frozenset(m) for m in members.values()}


# --- the recursive search and parallel trials --------------------------------------

# 8 x 8 groups of 30, and 4 x 5 x 4 groups of 20: every sub-module has more than
# 16 children, so the recursion spawns tasks below the top level too.
SHAPES = {"8x8x30": ((8, 8), 30), "4x5x4x20": ((4, 5, 4), 20)}

VARIANTS = {
    "undirected": {},
    "directed": {"directed": True},
    "directed-to-nodes": {"directed": True, "to_nodes": True},
    "recorded": {"directed": True, "recorded_teleportation": True},
    "undirdir": {"flow_model": "undirdir"},
    "markov-time": {"markov_time": 1.5},
    "preferred": {"preferred_number_of_modules": 5},
    "entropy": {"entropy_corrected": True},
    "regularized": {"regularized": True},
    "regularized-directed": {"regularized": True, "directed": True},
}

WRITERS = {"links": write_links, "states": write_states, "multilayer": write_multilayer}


def _cases():
    for shape in SHAPES:
        for variant in VARIANTS:
            yield shape, "links", variant
        yield shape, "states", "directed"
        yield shape, "multilayer", "undirected"


@pytest.mark.parametrize("shape", SHAPES)
def test_the_hierarchy_is_found(shape):
    # Sub-modules are tasks of their own only if the recursion accepts them.
    # Not every objective does (recorded teleportation, Markov time and the
    # regularized prior stop at two levels here), but the default one must.
    _, links = planted(*SHAPES[shape], seed=11)
    result = run(links, silent=True, seed=7, num_trials=2, num_threads=1)
    assert result.num_levels == len(SHAPES[shape][0]) + 1


@pytest.mark.parametrize(("shape", "kind", "variant"), list(_cases()))
def test_thread_count_does_not_change_a_large_result(tmp_path, shape, kind, variant):
    n, links = planted(*SHAPES[shape], seed=11)
    path = str(WRITERS[kind](tmp_path / "net.net", n, links))
    options = {"seed": 7, **VARIANTS[variant]}

    serial, used = _threads_used(path, tmp_path, num_threads=1, num_trials=2, **options)
    assert used == 1
    for threads in (2, 4):
        threaded, used = _threads_used(
            path, tmp_path, num_threads=threads, num_trials=2, **options
        )
        assert used == threads
        assert threaded.codelengths == serial.codelengths, threads
        assert _tree(threaded) == _tree(serial), threads

    serial, _ = _threads_used(path, tmp_path, num_threads=1, num_trials=4, **options)
    for threads in (1, 2, 4):
        parallel, used = _threads_used(
            path,
            tmp_path,
            num_threads=threads,
            num_trials=4,
            parallel_trials=True,
            **options,
        )
        assert used == threads
        assert parallel.codelengths == pytest.approx(serial.codelengths, rel=1e-14)
        assert parallel.codelength == pytest.approx(serial.codelength, rel=1e-14)
        assert _modules(parallel) == _modules(serial), threads


# --- inner parallelization -----------------------------------------------------------

# 10 x 25 groups of 40 nodes: exactly the 10,000 active nodes the sweep needs.
INNER_SHAPE = ((10, 25), 40)

INNER_VARIANTS = {
    "undirected": {},
    "directed": {"directed": True},
    "recorded": {"directed": True, "recorded_teleportation": True},
    "entropy": {"entropy_corrected": True},
    "preferred": {"preferred_number_of_modules": 12},
    "regularized": {"regularized": True},
}


@pytest.fixture(scope="module")
def inner_network(tmp_path_factory):
    n, links = planted(*INNER_SHAPE, seed=5)
    assert n == 10_000
    return str(write_links(tmp_path_factory.mktemp("inner") / "net.net", n, links))


def _write_tree(path: Path, tree: dict[int, tuple[int, ...]]) -> Path:
    lines = ["# path flow name node_id"]
    lines += [f'{":".join(map(str, p))} 0 "{n}" {n}' for n, p in tree.items()]
    path.write_text("\n".join(lines) + "\n")
    return path


@pytest.mark.parametrize("levels", ["two-level", "multilevel"])
@pytest.mark.parametrize("variant", INNER_VARIANTS)
def test_inner_parallelization_is_repeatable_and_scores_its_tree(
    tmp_path, inner_network, variant, levels
):
    options = {
        "seed": 7,
        "num_trials": 1,
        "two_level": levels == "two-level",
        **INNER_VARIANTS[variant],
    }
    first, used = _threads_used(
        inner_network, tmp_path, num_threads=4, inner_parallelization=True, **options
    )
    # Two-level has no recursion, so only the inner sweep opens a team.
    assert used == 4
    second, _ = _threads_used(
        inner_network, tmp_path, num_threads=4, inner_parallelization=True, **options
    )
    assert second.codelength == first.codelength
    assert _tree(second) == _tree(first)

    tree = _tree(first)
    rescored = run(
        inner_network,
        silent=True,
        num_threads=1,
        no_infomap=True,
        cluster_data=str(_write_tree(tmp_path / "found.tree", tree)),
        **options,
    )
    assert _modules(rescored) == _modules(first)
    assert rescored.codelength == pytest.approx(first.codelength, rel=REL)
