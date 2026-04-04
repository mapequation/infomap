from __future__ import annotations

import argparse
import json
import random
import statistics
import subprocess
import tempfile
from pathlib import Path

MODULE_SIZE_BUCKET_LABELS = ("1-2", "3-4", "5-8", "9-16", "17-32", "33-64", "65+")


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


def metric_stats(values: list[float]) -> dict[str, float]:
    if not values:
        return {"mean": 0.0, "stdev": 0.0, "cv": 0.0}
    mean_value = statistics.mean(values)
    stdev_value = statistics.stdev(values) if len(values) > 1 else 0.0
    cv_value = stdev_value / mean_value if mean_value else 0.0
    return {"mean": mean_value, "stdev": stdev_value, "cv": cv_value}


def benchmark_case(
    binary: Path,
    name: str,
    network_path: Path,
    repeats: int,
    iterations: int,
    flags: str,
) -> dict[str, object]:
    samples: list[dict[str, object]] = []
    run_samples: list[dict[str, object]] = []
    for repeat in range(repeats):
        completed = subprocess.run(
            [
                str(binary),
                "--input",
                str(network_path),
                "--name",
                name,
                "--flags",
                flags,
                "--iterations",
                str(iterations),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        sample = json.loads(completed.stdout)
        samples.append(sample)
        for run in sample.get("runs", [sample]):
            run_sample = dict(run)
            run_sample["repeat"] = repeat + 1
            run_samples.append(run_sample)

    rebuild_total_stats = metric_stats([float(run.get("rebuild", {}).get("total_sec", 0.0)) for run in run_samples])
    rebuild_network_stats = metric_stats([float(run.get("rebuild", {}).get("network_sec", 0.0)) for run in run_samples])
    rebuild_module_stats = metric_stats([float(run.get("rebuild", {}).get("module_sec", 0.0)) for run in run_samples])
    rebuild_module_prep_stats = metric_stats([float(run.get("rebuild", {}).get("module_prep_sec", 0.0)) for run in run_samples])
    rebuild_module_index_stats = metric_stats([float(run.get("rebuild", {}).get("module_index_sec", 0.0)) for run in run_samples])
    rebuild_module_reserve_stats = metric_stats([float(run.get("rebuild", {}).get("module_reserve_sec", 0.0)) for run in run_samples])
    rebuild_module_clone_stats = metric_stats([float(run.get("rebuild", {}).get("module_clone_sec", 0.0)) for run in run_samples])
    rebuild_module_edge_clone_stats = metric_stats([float(run.get("rebuild", {}).get("module_edge_clone_sec", 0.0)) for run in run_samples])
    total_stats = metric_stats([float(sample["total_sec"]) for sample in samples])
    run_stats = metric_stats([float(run["run_sec"]) for run in run_samples])
    read_input_stats = metric_stats([float(sample["read_input_sec"]) for sample in samples])
    peak_rss_stats = metric_stats([float(run["peak_rss_bytes"]) for run in run_samples])
    rebuild_bucket_stats: dict[str, dict[str, float | int]] = {}
    for label in MODULE_SIZE_BUCKET_LABELS:
        bucket_calls = [int(run.get("rebuild", {}).get("module_size_buckets", {}).get(label, {}).get("calls", 0)) for run in run_samples]
        bucket_sec = [float(run.get("rebuild", {}).get("module_size_buckets", {}).get(label, {}).get("sec", 0.0)) for run in run_samples]
        bucket_prep_sec = [float(run.get("rebuild", {}).get("module_size_buckets", {}).get(label, {}).get("prep_sec", 0.0)) for run in run_samples]
        bucket_index_sec = [float(run.get("rebuild", {}).get("module_size_buckets", {}).get(label, {}).get("index_sec", 0.0)) for run in run_samples]
        bucket_reserve_sec = [float(run.get("rebuild", {}).get("module_size_buckets", {}).get(label, {}).get("reserve_sec", 0.0)) for run in run_samples]
        bucket_clone_sec = [float(run.get("rebuild", {}).get("module_size_buckets", {}).get(label, {}).get("clone_sec", 0.0)) for run in run_samples]
        bucket_edge_clone_sec = [float(run.get("rebuild", {}).get("module_size_buckets", {}).get(label, {}).get("edge_clone_sec", 0.0)) for run in run_samples]
        bucket_sec_stats = metric_stats(bucket_sec)
        bucket_prep_sec_stats = metric_stats(bucket_prep_sec)
        bucket_index_sec_stats = metric_stats(bucket_index_sec)
        bucket_reserve_sec_stats = metric_stats(bucket_reserve_sec)
        bucket_clone_sec_stats = metric_stats(bucket_clone_sec)
        bucket_edge_clone_sec_stats = metric_stats(bucket_edge_clone_sec)
        rebuild_bucket_stats[label] = {
            "median_calls": statistics.median(bucket_calls),
            "mean_calls": statistics.mean(bucket_calls) if bucket_calls else 0.0,
            "median_sec": statistics.median(bucket_sec),
            "mean_sec": bucket_sec_stats["mean"],
            "stdev_sec": bucket_sec_stats["stdev"],
            "cv_sec": bucket_sec_stats["cv"],
            "median_prep_sec": statistics.median(bucket_prep_sec),
            "mean_prep_sec": bucket_prep_sec_stats["mean"],
            "median_index_sec": statistics.median(bucket_index_sec),
            "mean_index_sec": bucket_index_sec_stats["mean"],
            "median_reserve_sec": statistics.median(bucket_reserve_sec),
            "mean_reserve_sec": bucket_reserve_sec_stats["mean"],
            "median_clone_sec": statistics.median(bucket_clone_sec),
            "mean_clone_sec": bucket_clone_sec_stats["mean"],
            "median_edge_clone_sec": statistics.median(bucket_edge_clone_sec),
            "mean_edge_clone_sec": bucket_edge_clone_sec_stats["mean"],
        }

    return {
        "name": name,
        "path": str(network_path),
        "repeats": repeats,
        "iterations": iterations,
        "flags": samples[0]["flags"],
        "samples": samples,
        "run_samples": run_samples,
        "median_total_sec": statistics.median(float(sample["total_sec"]) for sample in samples),
        "median_read_input_sec": statistics.median(float(sample["read_input_sec"]) for sample in samples),
        "median_run_sec": statistics.median(float(run["run_sec"]) for run in run_samples),
        "mean_total_sec": total_stats["mean"],
        "stdev_total_sec": total_stats["stdev"],
        "cv_total_sec": total_stats["cv"],
        "mean_read_input_sec": read_input_stats["mean"],
        "stdev_read_input_sec": read_input_stats["stdev"],
        "cv_read_input_sec": read_input_stats["cv"],
        "mean_run_sec": run_stats["mean"],
        "stdev_run_sec": run_stats["stdev"],
        "cv_run_sec": run_stats["cv"],
        "mean_peak_rss_bytes": peak_rss_stats["mean"],
        "stdev_peak_rss_bytes": peak_rss_stats["stdev"],
        "cv_peak_rss_bytes": peak_rss_stats["cv"],
        "peak_rss_bytes_max": max(int(run["peak_rss_bytes"]) for run in run_samples),
        "num_nodes": samples[0]["num_nodes"],
        "num_links": samples[0]["num_links"],
        "num_top_modules": samples[0]["num_top_modules"],
        "num_levels": samples[0]["num_levels"],
        "codelength": samples[0]["codelength"],
        "higher_order_input": samples[0]["higher_order_input"],
        "directed_input": samples[0]["directed_input"],
        "node_size_bytes": samples[0]["node_size_bytes"],
        "edge_size_bytes": samples[0]["edge_size_bytes"],
        "rebuild": {
            "median_total_sec": statistics.median(float(run.get("rebuild", {}).get("total_sec", 0.0)) for run in run_samples),
            "mean_total_sec": rebuild_total_stats["mean"],
            "stdev_total_sec": rebuild_total_stats["stdev"],
            "cv_total_sec": rebuild_total_stats["cv"],
            "median_network_sec": statistics.median(float(run.get("rebuild", {}).get("network_sec", 0.0)) for run in run_samples),
            "mean_network_sec": rebuild_network_stats["mean"],
            "median_module_sec": statistics.median(float(run.get("rebuild", {}).get("module_sec", 0.0)) for run in run_samples),
            "mean_module_sec": rebuild_module_stats["mean"],
            "median_module_prep_sec": statistics.median(float(run.get("rebuild", {}).get("module_prep_sec", 0.0)) for run in run_samples),
            "mean_module_prep_sec": rebuild_module_prep_stats["mean"],
            "median_module_index_sec": statistics.median(float(run.get("rebuild", {}).get("module_index_sec", 0.0)) for run in run_samples),
            "mean_module_index_sec": rebuild_module_index_stats["mean"],
            "median_module_reserve_sec": statistics.median(float(run.get("rebuild", {}).get("module_reserve_sec", 0.0)) for run in run_samples),
            "mean_module_reserve_sec": rebuild_module_reserve_stats["mean"],
            "median_module_clone_sec": statistics.median(float(run.get("rebuild", {}).get("module_clone_sec", 0.0)) for run in run_samples),
            "mean_module_clone_sec": rebuild_module_clone_stats["mean"],
            "median_module_edge_clone_sec": statistics.median(float(run.get("rebuild", {}).get("module_edge_clone_sec", 0.0)) for run in run_samples),
            "mean_module_edge_clone_sec": rebuild_module_edge_clone_stats["mean"],
            "median_total_calls": statistics.median(int(run.get("rebuild", {}).get("total_calls", 0)) for run in run_samples),
            "median_network_calls": statistics.median(int(run.get("rebuild", {}).get("network_calls", 0)) for run in run_samples),
            "median_module_calls": statistics.median(int(run.get("rebuild", {}).get("module_calls", 0)) for run in run_samples),
            "peak_rss_bytes_max": max(int(run.get("rebuild", {}).get("peak_rss_bytes_max", 0)) for run in run_samples),
            "peak_rss_delta_bytes_max": max(int(run.get("rebuild", {}).get("peak_rss_delta_bytes_max", 0)) for run in run_samples),
            "module_size_buckets": rebuild_bucket_stats,
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
    parser.add_argument("--iterations", type=int, default=1, help="Number of in-process iterations per benchmark run.")
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
    args = parser.parse_args()

    if args.iterations < 1:
        parser.error("--iterations must be at least 1")

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
            benchmark_case(args.binary, name, path, args.repeats, args.iterations, args.flags)
            for name, path in cases
        ]

    report = {
        "profile": args.profile,
        "repeats": args.repeats,
        "iterations": args.iterations,
        "binary": str(args.binary),
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
