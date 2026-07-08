"""Measurements for the nanobind spike (#743).

Compares the SWIG backend (the shipped extension) against the nanobind spike
module on the four axes the issue asks for: artifact size, import time,
get_node_data throughput, and result parity. Run from the repo root after
building the spike (see CMakeLists.txt):

    .venv/bin/python spikes/nanobind/bench.py
"""

from __future__ import annotations

import statistics
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[2]
SPIKE_BUILD = REPO_ROOT / "build" / "nanobind-spike"
sys.path.insert(0, str(SPIKE_BUILD))

RUN_FLAGS = "--two-level --silent --seed 123 --num-trials 1"


def _sizes() -> None:
    import infomap._swig as _  # ensure the package resolves

    swig_so = next(
        (REPO_ROOT / "interfaces" / "python" / "src" / "infomap").glob("_infomap*.so")
    )
    nb_so = next(SPIKE_BUILD.glob("_infomap_nb*.so"))
    print("== artifact size ==")
    for label, path in (("SWIG", swig_so), ("nanobind", nb_so)):
        raw = path.stat().st_size
        stripped = subprocess.run(
            ["sh", "-c", f"cp {path} /tmp/bench_strip.so && strip /tmp/bench_strip.so && stat -c %s /tmp/bench_strip.so"],
            capture_output=True, text=True, check=True,
        ).stdout.strip()
        print(f"  {label:9s} {raw/1e6:6.1f} MB   (stripped {int(stripped)/1e6:.1f} MB)")
    shadow = (REPO_ROOT / "interfaces" / "python" / "src" / "infomap" / "_swig.py").stat().st_size
    print(f"  SWIG shadow module (_swig.py): {shadow/1e3:.0f} kB")


def _import_time() -> None:
    print("== import time (best of 5, fresh process) ==")
    cases = {
        "SWIG (_infomap + _swig shadow)": "import infomap._swig",
        "nanobind (_infomap_nb)": f"import sys; sys.path.insert(0, {str(SPIKE_BUILD)!r}); import _infomap_nb",
    }
    for label, stmt in cases.items():
        timings = []
        for _ in range(5):
            code = f"import time; t0 = time.perf_counter(); {stmt}; print(time.perf_counter() - t0)"
            out = subprocess.run(
                [sys.executable, "-c", code], capture_output=True, text=True, check=True,
                cwd=REPO_ROOT,
            ).stdout.strip()
            timings.append(float(out) * 1000)
        print(f"  {label:34s} {min(timings):6.1f} ms")


def _links(num_nodes: int, num_links: int, seed: int = 42) -> np.ndarray:
    rng = np.random.default_rng(seed)
    links = rng.integers(0, num_nodes, size=(num_links, 2), dtype=np.uint64)
    return links[links[:, 0] != links[:, 1]]


def _throughput_and_parity(num_nodes: int = 100_000, num_links: int = 500_000) -> None:
    import _infomap_nb
    from infomap import Infomap

    links = _links(num_nodes, num_links)
    print(f"== get_node_data throughput ({num_nodes:,} nodes, {len(links):,} links) ==")

    nb_core = _infomap_nb.Core(RUN_FLAGS)
    nb_core.addLinksFromArray(links)
    t0 = time.perf_counter()
    nb_core.run("")
    run_seconds = time.perf_counter() - t0

    # Identical input path: both backends see the same addLink sequence in
    # the same order (the adapters aggregate duplicates differently, which
    # would break result parity for reasons unrelated to the binding layer).
    im = Infomap(args=RUN_FLAGS)
    im._core.addLinks(
        [int(s) for s in links[:, 0]],
        [int(t) for t in links[:, 1]],
        [1.0] * len(links),
    )
    im.run()
    swig_core = im._core

    def bench(fn, repeat=7):
        timings = []
        for _ in range(repeat):
            t0 = time.perf_counter()
            fn()
            timings.append(time.perf_counter() - t0)
        return min(timings) * 1000, statistics.median(timings) * 1000

    # SWIG as the package consumes it today (result.py _Snapshot):
    # one traversal, then list() per column.
    def swig_lists():
        data = swig_core.get_node_data(1, False)
        return [list(data.node_id), list(data.state_id), list(data.module_id),
                list(data.flow), list(data.depth), list(data.layer_id),
                list(data.child_index), list(data.modular_centrality),
                list(data.path_flat), list(data.path_len)]

    # SWIG best case: numpy conversion of the SWIG vectors instead of lists.
    def swig_numpy():
        data = swig_core.get_node_data(1, False)
        return [np.asarray(col) for col in (
            data.node_id, data.state_id, data.module_id, data.flow, data.depth,
            data.layer_id, data.child_index, data.modular_centrality,
            data.path_flat, data.path_len)]

    # nanobind: one traversal, zero-copy views.
    def nb_views():
        return nb_core.get_node_data(1, False)

    for label, fn in (("SWIG -> lists (today's path)", swig_lists),
                      ("SWIG -> np.asarray", swig_numpy),
                      ("nanobind -> zero-copy views", nb_views)):
        best, median = bench(fn)
        print(f"  {label:31s} best {best:8.1f} ms   median {median:8.1f} ms")
    print(f"  (engine run itself: {run_seconds:.1f} s)")

    print("== parity ==")
    nb_modules = dict(nb_core.getModules(1, False))
    swig_modules = dict(swig_core.getModules(1, False))
    print(f"  getModules identical: {nb_modules == swig_modules} "
          f"({len(nb_modules):,} nodes)")
    print(f"  codelength identical: {nb_core.codelength() == swig_core.codelength()}")
    nb_data = nb_core.get_node_data(1, False)
    swig_data = swig_core.get_node_data(1, False)
    same = all(
        np.array_equal(np.asarray(nb_data[col]), np.asarray(getattr(swig_data, col)))
        for col in ("node_id", "state_id", "module_id", "flow", "depth",
                    "layer_id", "child_index", "modular_centrality",
                    "path_flat", "path_len")
    )
    print(f"  NodeData columns identical: {same}")


if __name__ == "__main__":
    _sizes()
    _import_time()
    _throughput_and_parity()
