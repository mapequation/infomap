#!/usr/bin/env python3
"""Run the CLI's OpenMP paths under ThreadSanitizer.

Only meaningful for a binary built with INFOMAP_ENABLE_TSAN (``make test-tsan``),
with Archer loaded through OMP_TOOL_LIBRARIES so TSan sees OpenMP's own
synchronisation. Covers the paths the C++ tests barely reach:

  * the recursive partition tasks, on a nested network with many modules
  * --inner-parallelization, which only engages at 10k+ nodes
  * --parallel-trials
at several thread counts. Fails if any run prints a TSan report or exits non-zero.
"""

import os
import random
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

# 8 x 8 x 16 modules of 12 nodes: 12288 nodes, above the 10k floor for
# --inner-parallelization, and deep enough that sub-modules spawn tasks.
BRANCHING = (8, 8, 16)
LEAF_SIZE = 12


def _write_nested_network(path: Path, directed: bool) -> None:
    rng = random.Random(1171)
    num_nodes = BRANCHING[0] * BRANCHING[1] * BRANCHING[2] * LEAF_SIZE
    per_mid = BRANCHING[2] * LEAF_SIZE
    per_top = BRANCHING[1] * per_mid
    links = set()

    def add(source: int, target: int) -> None:
        if source != target:
            links.add(
                (source, target)
                if directed
                else (min(source, target), max(source, target))
            )

    for node in range(num_nodes):
        leaf = node // LEAF_SIZE
        for _ in range(4):
            add(node, leaf * LEAF_SIZE + rng.randrange(LEAF_SIZE))
        mid_start = node // per_mid * per_mid
        add(node, mid_start + rng.randrange(per_mid))
        if rng.random() < 0.5:
            top_start = node // per_top * per_top
            add(node, top_start + rng.randrange(per_top))
        if rng.random() < 0.1:
            add(node, rng.randrange(num_nodes))

    lines = [f"*Vertices {num_nodes}"]
    lines.append("*Arcs" if directed else "*Edges")
    lines += [f"{s + 1} {t + 1}" for s, t in sorted(links)]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _cases(work: Path) -> list[tuple[str, Path, list[str], list[int]]]:
    nested = work / "nested.net"
    nested_directed = work / "nested_directed.net"
    _write_nested_network(nested, directed=False)
    _write_nested_network(nested_directed, directed=True)
    examples = REPO_ROOT / "examples" / "networks"
    return [
        ("recursive partition", nested, [], [2, 4, 8]),
        ("recursive partition, directed", nested_directed, ["--directed"], [4]),
        ("inner parallelization", nested, ["--inner-parallelization"], [2, 4]),
        (
            "inner parallelization, two-level",
            nested,
            ["--inner-parallelization", "--two-level"],
            [4],
        ),
        (
            "inner parallelization, directed",
            nested_directed,
            ["--inner-parallelization", "--directed"],
            [4],
        ),
        ("parallel trials", nested, ["--parallel-trials", "--num-trials", "4"], [2, 4]),
        (
            "parallel trials, small",
            examples / "modular_wd.net",
            ["--parallel-trials", "--num-trials", "8", "--directed"],
            [4],
        ),
        (
            "parallel trials, states",
            examples / "states.net",
            ["--parallel-trials", "--num-trials", "8"],
            [4],
        ),
    ]


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <Infomap binary>", file=sys.stderr)
        return 2
    binary = sys.argv[1]
    if not os.environ.get("OMP_TOOL_LIBRARIES"):
        print(
            "warning: OMP_TOOL_LIBRARIES is unset; without Archer TSan reports OpenMP's own synchronisation",
            file=sys.stderr,
        )

    failures = []
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        for index, (name, network, flags, thread_counts) in enumerate(_cases(work)):
            for threads in thread_counts:
                out_dir = work / f"out_{index}_{threads}"
                out_dir.mkdir()
                command = [
                    binary,
                    str(network),
                    str(out_dir),
                    "--seed",
                    "3",
                    "--num-threads",
                    str(threads),
                    "--silent",
                    *flags,
                ]
                label = f"{name} ({threads} threads)"
                print(f"-- {label}: {' '.join(command[3:])}", flush=True)
                result = subprocess.run(
                    command, capture_output=True, text=True, check=False
                )
                reported = "WARNING: ThreadSanitizer" in result.stderr
                if reported or result.returncode != 0:
                    failures.append(label)
                    print(f"FAILED: exit {result.returncode}", flush=True)
                    sys.stdout.write(result.stderr)

    if failures:
        print(f"{len(failures)} run(s) failed: {', '.join(failures)}", file=sys.stderr)
        return 1
    print("all runs clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
