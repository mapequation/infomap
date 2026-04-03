from __future__ import annotations

import argparse
import json
import random
import statistics
import subprocess
import tempfile
from pathlib import Path


def write_pajek_edges(path: Path, num_nodes: int, edges: set[tuple[int, int]]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        handle.write("*Vertices\n")
        for node_id in range(num_nodes):
            handle.write(f'{node_id} "{node_id}"\n')
        handle.write("*Edges\n")
        for source, target in sorted(edges):
            handle.write(f"{source} {target}\n")


def generate_sparse_graph(path: Path, num_nodes: int, avg_degree: int, seed: int) -> None:
    rng = random.Random(seed)
    edges: set[tuple[int, int]] = set()
    ring_span = max(1, avg_degree // 4)
    for node in range(num_nodes):
        for step in range(1, ring_span + 1):
            target = (node + step) % num_nodes
            edge = (node, target) if node < target else (target, node)
            edges.add(edge)

    target_edges = max(num_nodes, num_nodes * avg_degree // 2)
    while len(edges) < target_edges:
        source = rng.randrange(num_nodes)
        target = rng.randrange(num_nodes)
        if source == target:
            continue
        edge = (source, target) if source < target else (target, source)
        edges.add(edge)

    write_pajek_edges(path, num_nodes, edges)


def generate_ring_of_cliques(path: Path, clique_count: int, clique_size: int) -> None:
    edges: set[tuple[int, int]] = set()
    num_nodes = clique_count * clique_size
    for clique in range(clique_count):
        offset = clique * clique_size
        nodes = [offset + index for index in range(clique_size)]
        for i, source in enumerate(nodes):
            for target in nodes[i + 1 :]:
                edges.add((source, target))
        next_offset = ((clique + 1) % clique_count) * clique_size
        bridge = (offset, next_offset)
        edges.add(bridge if bridge[0] < bridge[1] else (bridge[1], bridge[0]))

    write_pajek_edges(path, num_nodes, edges)


def generate_state_ring(path: Path, physical_nodes: int) -> None:
    with path.open("w", encoding="utf-8") as handle:
        handle.write(f"*Vertices {physical_nodes}\n")
        for node_id in range(1, physical_nodes + 1):
            handle.write(f'{node_id} "{node_id}"\n')
        handle.write("*States\n")
        for node_id in range(1, physical_nodes + 1):
            handle.write(f'{2 * node_id - 1} {node_id} "{node_id}a"\n')
            handle.write(f'{2 * node_id} {node_id} "{node_id}b"\n')
        handle.write("*Links\n")
        for node_id in range(physical_nodes):
            current_a = 2 * node_id + 1
            current_b = 2 * node_id + 2
            next_a = (2 * ((node_id + 1) % physical_nodes)) + 1
            next_b = next_a + 1
            handle.write(f"{current_a} {next_a} 1\n")
            handle.write(f"{current_a} {next_b} 1\n")
            handle.write(f"{current_b} {next_a} 1\n")
            handle.write(f"{current_b} {next_b} 1\n")


def run_case(binary: Path, name: str, network_path: Path, flags: str, backend_mode: str) -> dict[str, object]:
    completed = subprocess.run(
        [str(binary), "--input", str(network_path), "--name", name, "--flags", flags, "--backend-mode", backend_mode],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(completed.stdout)


def benchmark_case(binary: Path, name: str, network_path: Path, repeats: int, warmup_repeats: int, flags: str, backend_mode: str) -> dict[str, object]:
    for _ in range(warmup_repeats):
        run_case(binary, name, network_path, flags, backend_mode)

    runs: list[dict[str, object]] = []
    for _ in range(repeats):
        runs.append(run_case(binary, name, network_path, flags, backend_mode))

    def median(key: str) -> float:
        return statistics.median(float(run[key]) for run in runs)

    def stats_median(key: str) -> float:
        return statistics.median(float(run["benchmark_stats"][key]) for run in runs)

    return {
        "name": name,
        "path": str(network_path),
        "repeats": repeats,
        "warmup_repeats": warmup_repeats,
        "backend_mode": runs[0]["backend_mode"],
        "flags": runs[0]["flags"],
        "median_total_sec": median("total_sec"),
        "median_read_input_sec": median("read_input_sec"),
        "median_run_sec": median("run_sec"),
        "peak_rss_bytes_max": max(int(run["peak_rss_bytes"]) for run in runs),
        "num_nodes": runs[0]["num_nodes"],
        "num_links": runs[0]["num_links"],
        "num_top_modules": runs[0]["num_top_modules"],
        "num_levels": runs[0]["num_levels"],
        "codelength": runs[0]["codelength"],
        "higher_order_input": runs[0]["higher_order_input"],
        "directed_input": runs[0]["directed_input"],
        "node_size_bytes": runs[0]["node_size_bytes"],
        "edge_size_bytes": runs[0]["edge_size_bytes"],
        "active_payload_nodes": runs[0].get("active_payload_nodes", 0),
        "active_payload_bytes": runs[0].get("active_payload_bytes", 0),
        "benchmark_stats_median": {
            "calculate_flow_sec": stats_median("calculate_flow_sec"),
            "init_network_sec": stats_median("init_network_sec"),
            "run_partition_sec": stats_median("run_partition_sec"),
            "find_top_modules_sec": stats_median("find_top_modules_sec"),
            "fine_tune_sec": stats_median("fine_tune_sec"),
            "coarse_tune_sec": stats_median("coarse_tune_sec"),
            "recursive_partition_sec": stats_median("recursive_partition_sec"),
            "super_modules_sec": stats_median("super_modules_sec"),
            "super_modules_fast_sec": stats_median("super_modules_fast_sec"),
            "find_top_modules_calls": runs[0]["benchmark_stats"]["find_top_modules_calls"],
            "fine_tune_calls": runs[0]["benchmark_stats"]["fine_tune_calls"],
            "coarse_tune_calls": runs[0]["benchmark_stats"]["coarse_tune_calls"],
            "recursive_partition_calls": runs[0]["benchmark_stats"]["recursive_partition_calls"],
            "super_modules_calls": runs[0]["benchmark_stats"]["super_modules_calls"],
            "super_modules_fast_calls": runs[0]["benchmark_stats"]["super_modules_fast_calls"],
            "consolidation_count": runs[0]["benchmark_stats"]["consolidation_count"],
            "consolidations": runs[0]["benchmark_stats"]["consolidations"],
        },
    }


def render_markdown(results: list[dict[str, object]]) -> str:
    lines = [
        "## Native Benchmark Summary",
        "",
        "| Case | Nodes | Links | Median total (s) | Median run (s) | Peak RSS (MiB) | Top Modules | Levels |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for result in results:
        lines.append(
            "| {name} | {num_nodes} | {num_links} | {median_total_sec:.6f} | {median_run_sec:.6f} | {peak_rss_mib:.2f} | {num_top_modules} | {num_levels} |".format(
                peak_rss_mib=float(result["peak_rss_bytes_max"]) / (1024.0 * 1024.0),
                **result,
            )
        )
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Run native Infomap benchmarks and write a JSON report.")
    parser.add_argument("--binary", type=Path, required=True, help="Path to the native benchmark executable.")
    parser.add_argument("--output", type=Path, required=True, help="Path to write the JSON benchmark report.")
    parser.add_argument("--summary", type=Path, default=None, help="Optional Markdown summary output.")
    parser.add_argument("--repeats", type=int, default=3, help="Number of runs per case.")
    parser.add_argument("--warmup-repeats", type=int, default=0, help="Number of warmup runs per case before recording.")
    parser.add_argument(
        "--profile",
        choices=("smoke", "baseline", "full"),
        default="baseline",
        help="Benchmark corpus size.",
    )
    parser.add_argument(
        "--flags",
        default="--silent --no-file-output --num-trials 1 --seed 123",
        help="Infomap flags forwarded to the native benchmark executable.",
    )
    parser.add_argument(
        "--backend-mode",
        choices=("auto", "pointer"),
        default="auto",
        help="Internal active-graph backend mode for the benchmark executable.",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    if not args.binary.is_file():
        raise FileNotFoundError(f"Benchmark binary not found: {args.binary}")

    cases: list[tuple[str, Path]] = [
        ("twotriangles", repo_root / "examples" / "networks" / "twotriangles.net"),
        ("ninetriangles", repo_root / "examples" / "networks" / "ninetriangles.net"),
        ("modular_w", repo_root / "examples" / "networks" / "modular_w.net"),
        ("modular_wd", repo_root / "examples" / "networks" / "modular_wd.net"),
        ("states", repo_root / "examples" / "networks" / "states.net"),
        ("multilayer", repo_root / "examples" / "networks" / "multilayer.net"),
        ("bipartite", repo_root / "examples" / "networks" / "bipartite.net"),
    ]

    with tempfile.TemporaryDirectory() as temp_dir:
        generated_dir = Path(temp_dir)
        sparse_10k = generated_dir / "sparse_10k.net"
        ring_10k = generated_dir / "ring_of_cliques_10k.net"
        state_5k = generated_dir / "state_ring_5k.net"
        generate_sparse_graph(sparse_10k, num_nodes=10_000, avg_degree=10, seed=123)
        generate_ring_of_cliques(ring_10k, clique_count=1_000, clique_size=10)
        generate_state_ring(state_5k, physical_nodes=5_000)
        cases.extend(
            [
                ("sparse_10k", sparse_10k),
                ("ring_of_cliques_10k", ring_10k),
                ("state_ring_5k", state_5k),
            ]
        )

        if args.profile in {"baseline", "full"}:
            sparse_100k = generated_dir / "sparse_100k.net"
            ring_100k = generated_dir / "ring_of_cliques_100k.net"
            generate_sparse_graph(sparse_100k, num_nodes=100_000, avg_degree=10, seed=456)
            generate_ring_of_cliques(ring_100k, clique_count=10_000, clique_size=10)
            cases.extend(
                [
                    ("sparse_100k", sparse_100k),
                    ("ring_of_cliques_100k", ring_100k),
                ]
            )

        if args.profile == "full":
            sparse_1m = generated_dir / "sparse_1m.net"
            generate_sparse_graph(sparse_1m, num_nodes=1_000_000, avg_degree=10, seed=789)
            cases.append(("sparse_1m", sparse_1m))

        results = [
            benchmark_case(args.binary, name, path, args.repeats, args.warmup_repeats, args.flags, args.backend_mode)
            for name, path in cases
        ]

    report = {
        "profile": args.profile,
        "repeats": args.repeats,
        "warmup_repeats": args.warmup_repeats,
        "binary": str(args.binary),
        "backend_mode": args.backend_mode,
        "benchmarks": results,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    markdown = render_markdown(results)
    if args.summary is not None:
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(markdown, encoding="utf-8")

    print(markdown)


if __name__ == "__main__":
    main()
