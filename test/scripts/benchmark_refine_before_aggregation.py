#!/usr/bin/env python3
"""Manual benchmark for the experimental --refine-before-aggregation path."""

from __future__ import annotations

import argparse
import math
import re
import signal
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from pathlib import Path
from time import perf_counter

try:
    import networkx as nx
except ImportError as exc:  # pragma: no cover - manual helper
    raise SystemExit("This benchmark requires networkx: python3 -m pip install networkx") from exc


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BINARY = ROOT / "Infomap"


class LfrGenerationTimeout(RuntimeError):
    pass


def comb2(value: int) -> float:
    return value * (value - 1) / 2.0


def adjusted_rand_index(a: list[int], b: list[int]) -> float:
    n = len(a)
    if n != len(b):
        raise ValueError("Label vectors must have equal length")
    if n < 2:
        return 1.0

    counts_a = Counter(a)
    counts_b = Counter(b)
    counts_ab = Counter(zip(a, b))
    sum_ab = sum(comb2(v) for v in counts_ab.values())
    sum_a = sum(comb2(v) for v in counts_a.values())
    sum_b = sum(comb2(v) for v in counts_b.values())
    total = comb2(n)
    expected = sum_a * sum_b / total if total else 0.0
    maximum = 0.5 * (sum_a + sum_b)
    denominator = maximum - expected
    return 1.0 if denominator == 0.0 else (sum_ab - expected) / denominator


def normalized_mutual_info(a: list[int], b: list[int]) -> float:
    n = len(a)
    if n != len(b):
        raise ValueError("Label vectors must have equal length")
    if n == 0:
        return 1.0

    counts_a = Counter(a)
    counts_b = Counter(b)
    counts_ab = Counter(zip(a, b))

    mi = 0.0
    for (label_a, label_b), count in counts_ab.items():
        mi += (count / n) * math.log((count * n) / (counts_a[label_a] * counts_b[label_b]))

    entropy_a = -sum((count / n) * math.log(count / n) for count in counts_a.values())
    entropy_b = -sum((count / n) * math.log(count / n) for count in counts_b.values())
    denominator = entropy_a + entropy_b
    return 1.0 if denominator == 0.0 else 2.0 * mi / denominator


def write_pajek(graph: nx.Graph, path: Path) -> None:
    with path.open("w", encoding="utf-8") as out:
        out.write(f"*Vertices {graph.number_of_nodes()}\n")
        for node in range(graph.number_of_nodes()):
            out.write(f'{node + 1} "{node + 1}"\n')
        out.write("*Edges\n")
        for source, target in graph.edges():
            out.write(f"{source + 1} {target + 1} 1\n")


def parse_codelength(tree_file: Path) -> float:
    for line in tree_file.read_text(encoding="utf-8").splitlines():
        if line.startswith("# codelength "):
            return float(line.split()[2])
    raise ValueError(f"No codelength found in {tree_file}")


def parse_partition(clu_file: Path, num_nodes: int) -> list[int]:
    labels = [0] * num_nodes
    for line in clu_file.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        node_id, module_id, *_ = line.split()
        labels[int(node_id) - 1] = int(module_id)
    return labels


def parse_refinement_stats(stdout: str) -> dict[str, int]:
    decisions = Counter(re.findall(r"decision=([^,\s]+)", stdout))
    split_kept = 0
    for line in stdout.splitlines():
        if "Refine before aggregation" not in line or "decision=accepted" not in line:
            continue
        match = re.search(r"splitParentModules=(\d+)", line)
        if match:
            split_kept += int(match.group(1))
    return {
        "accepted": decisions["accepted"],
        "rejected_no_gain": decisions["rejected-no-gain"],
        "rejected_worse": decisions["rejected-worse"],
        "unchanged": decisions["unchanged"],
        "split_kept": split_kept,
    }


def run_infomap(binary: Path, network: Path, out_dir: Path, name: str, flags: list[str], seed: int, trials: int, num_nodes: int) -> dict[str, object]:
    run_out = out_dir / name
    run_out.mkdir()
    command = [
        str(binary),
        str(network),
        str(run_out),
        "--seed",
        str(seed),
        "--num-trials",
        str(trials),
        "--clu",
        "--tree",
        "--out-name",
        "result",
        "--verbose",
        *flags,
    ]
    started = perf_counter()
    try:
        completed = subprocess.run(command, check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(f"Infomap failed for {name}:\n{exc.stdout}") from exc
    wall_time = perf_counter() - started
    return {
        "codelength": parse_codelength(run_out / "result.tree"),
        "partition": parse_partition(run_out / "result.clu", num_nodes),
        "wall_time": wall_time,
        **parse_refinement_stats(completed.stdout),
    }


def make_sbm_sparse_bridges(seed: int) -> tuple[nx.Graph, list[int]]:
    sizes = [100] * 16
    probs = [[0.001] * len(sizes) for _ in sizes]
    for i in range(len(sizes)):
        probs[i][i] = 0.035
    graph = nx.stochastic_block_model(sizes, probs, seed=seed)
    labels = [block for block, size in enumerate(sizes) for _ in range(size)]
    return graph, labels


def make_hierarchical(seed: int) -> tuple[nx.Graph, list[int]]:
    sizes = [75] * 20
    probs = []
    for i in range(20):
        row = []
        for j in range(20):
            if i == j:
                row.append(0.06)
            elif i // 4 == j // 4:
                row.append(0.01)
            else:
                row.append(0.0008)
        probs.append(row)
    graph = nx.stochastic_block_model(sizes, probs, seed=seed)
    labels = [block // 4 for block, size in enumerate(sizes) for _ in range(size)]
    return graph, labels


def planted_lfr_labels(graph: nx.Graph) -> list[int]:
    community_ids = {}
    labels = []
    for node in range(graph.number_of_nodes()):
        community = frozenset(graph.nodes[node]["community"])
        if community not in community_ids:
            community_ids[community] = len(community_ids)
        labels.append(community_ids[community])
    return labels


def run_with_timeout(seconds: float, fn):
    if seconds <= 0:
        return fn()

    def timeout_handler(_signum, _frame):
        raise LfrGenerationTimeout(f"LFR generation exceeded {seconds:g}s")

    old_handler = signal.getsignal(signal.SIGALRM)
    signal.signal(signal.SIGALRM, timeout_handler)
    signal.setitimer(signal.ITIMER_REAL, seconds)
    try:
        return fn()
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, old_handler)


def make_lfr(seed: int, n: int, mu: float, timeout: float) -> tuple[nx.Graph, list[int], int]:
    params = {
        "n": n,
        "tau1": 3.0,
        "tau2": 1.5,
        "mu": mu,
        "average_degree": 10 if n <= 500 else 12 if n <= 750 else 15,
        "min_community": 20 if n <= 500 else 30 if n <= 750 else 50,
        "max_iters": 500,
    }
    last_error = None
    for attempt in range(30):
        candidate_seed = seed + attempt * 10000
        try:
            graph = run_with_timeout(timeout, lambda: nx.LFR_benchmark_graph(seed=candidate_seed, **params))
            graph = nx.Graph(graph)
            graph.remove_edges_from(nx.selfloop_edges(graph))
            return graph, planted_lfr_labels(graph), candidate_seed
        except Exception as exc:  # pragma: no cover - depends on NetworkX generator search
            last_error = exc
    raise RuntimeError(f"Could not generate LFR graph n={n} mu={mu} seed={seed}: {last_error}")


def parse_uint_list(value: str) -> list[int]:
    return [int(item.strip()) for item in value.split(",") if item.strip()]


def parse_float_list(value: str) -> list[float]:
    return [float(item.strip()) for item in value.split(",") if item.strip()]


def synthetic_benchmark_cases() -> list[tuple[str, nx.Graph, list[int]]]:
    cases = []
    for seed in (482, 487, 488):
        graph, labels = make_sbm_sparse_bridges(seed)
        cases.append((f"sbm_16x100_sparse_bridges_seed{seed}", graph, labels))
    graph, labels = make_hierarchical(522)
    cases.append(("hier_20x75_5super_seed522", graph, labels))
    return cases


def lfr_benchmark_cases(ns: list[int], mus: list[float], cases_per_mu: int, seed_start: int, timeout: float, progress: bool) -> list[tuple[str, nx.Graph, list[int]]]:
    cases = []
    for n in ns:
        for mu in mus:
            for repeat in range(cases_per_mu):
                base_seed = seed_start + n * 1000 + int(round(mu * 1000)) * 100 + repeat
                if progress:
                    print(f"Generating LFR n={n} mu={mu:.2f} repeat={repeat + 1}/{cases_per_mu}...", file=sys.stderr, flush=True)
                graph, labels, seed = make_lfr(base_seed, n, mu, timeout)
                cases.append((f"lfr_n{n}_mu{mu:.2f}_seed{seed}", graph, labels))
    return cases


def benchmark_cases(suite: str, ns: list[int], mus: list[float], lfr_cases_per_mu: int, lfr_seed_start: int, lfr_generation_timeout: float, progress: bool) -> list[tuple[str, nx.Graph, list[int]]]:
    cases = []
    if suite in ("synthetic", "all"):
        cases.extend(synthetic_benchmark_cases())
    if suite in ("lfr", "all"):
        cases.extend(lfr_benchmark_cases(ns, mus, lfr_cases_per_mu, lfr_seed_start, lfr_generation_timeout, progress))
    return cases


def variants() -> list[tuple[str, list[str]]]:
    return [
        ("default", []),
        ("refine", ["--refine-before-aggregation"]),
        ("refine_min8", ["--refine-before-aggregation", "--refine-min-module-size", "8"]),
        ("refine_min16", ["--refine-before-aggregation", "--refine-min-module-size", "16"]),
        ("refine_one_module", ["--refine-before-aggregation", "--refine-start-mode", "one-module"]),
        ("refine_min8_one_module", ["--refine-before-aggregation", "--refine-min-module-size", "8", "--refine-start-mode", "one-module"]),
        ("refine_min16_one_module", ["--refine-before-aggregation", "--refine-min-module-size", "16", "--refine-start-mode", "one-module"]),
    ]


def print_rows(rows: list[dict[str, object]]) -> None:
    headers = [
        "case",
        "variant",
        "codelength",
        "delta",
        "ari_vs_default",
        "ari_vs_planted",
        "nmi_vs_default",
        "nmi_vs_planted",
        "runtime_ratio",
        "accepted",
        "rejected_no_gain",
        "rejected_worse",
        "unchanged",
        "split_kept",
    ]
    print("\t".join(headers))
    for row in rows:
        print(
            "\t".join(
                str(row[header]) if not isinstance(row[header], float) else f"{row[header]:.9f}"
                for header in headers
            )
        )


def print_aggregate(rows: list[dict[str, object]]) -> None:
    baseline_by_case = {row["case"]: row for row in rows if row["variant"] == "default"}
    aggregate: defaultdict[str, dict[str, float]] = defaultdict(lambda: {
        "n": 0,
        "better": 0,
        "same": 0,
        "worse": 0,
        "delta": 0.0,
        "ari_delta": 0.0,
        "nmi_delta": 0.0,
        "runtime_ratio": 0.0,
        "accepted": 0,
        "split_kept": 0,
    })

    for row in rows:
        if row["variant"] == "default":
            continue
        baseline = baseline_by_case[row["case"]]
        item = aggregate[str(row["variant"])]
        item["n"] += 1
        item["delta"] += float(row["delta"])
        item["ari_delta"] += float(row["ari_vs_planted"]) - float(baseline["ari_vs_planted"])
        item["nmi_delta"] += float(row["nmi_vs_planted"]) - float(baseline["nmi_vs_planted"])
        item["runtime_ratio"] += float(row["runtime_ratio"])
        item["accepted"] += int(row["accepted"])
        item["split_kept"] += int(row["split_kept"])
        if float(row["delta"]) < -1e-9:
            item["better"] += 1
        elif float(row["delta"]) > 1e-9:
            item["worse"] += 1
        else:
            item["same"] += 1

    print("\nAGGREGATE")
    print("variant\tn\tbetter\tsame\tworse\tmean_delta\tmean_ari_planted_delta\tmean_nmi_planted_delta\tmean_runtime_ratio\taccepted_total\tsplit_kept_total")
    for variant, item in aggregate.items():
        n = item["n"]
        print(
            f"{variant}\t{int(n)}\t{int(item['better'])}\t{int(item['same'])}\t{int(item['worse'])}"
            f"\t{item['delta'] / n:.9f}\t{item['ari_delta'] / n:.9f}\t{item['nmi_delta'] / n:.9f}"
            f"\t{item['runtime_ratio'] / n:.3f}\t{int(item['accepted'])}\t{int(item['split_kept'])}"
        )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=DEFAULT_BINARY)
    parser.add_argument("--seed", type=int, default=123)
    parser.add_argument("--num-trials", type=int, default=20)
    parser.add_argument("--suite", choices=("synthetic", "lfr", "all"), default="synthetic")
    parser.add_argument("--lfr-ns", default="500,750,1000")
    parser.add_argument("--lfr-mus", default="0.1,0.3,0.5")
    parser.add_argument("--lfr-cases-per-mu", type=int, default=1)
    parser.add_argument("--lfr-seed-start", type=int, default=7000)
    parser.add_argument("--lfr-generation-timeout", type=float, default=10.0)
    parser.add_argument("--progress", action="store_true")
    args = parser.parse_args()

    if not args.binary.exists():
        raise SystemExit(f"Infomap binary not found: {args.binary}")

    ns = parse_uint_list(args.lfr_ns)
    mus = parse_float_list(args.lfr_mus)

    with tempfile.TemporaryDirectory(prefix="infomap-refine-bench-") as tmp:
        tmp_path = Path(tmp)
        rows = []
        for case_name, graph, planted in benchmark_cases(args.suite, ns, mus, args.lfr_cases_per_mu, args.lfr_seed_start, args.lfr_generation_timeout, args.progress):
            network = tmp_path / f"{case_name}.net"
            write_pajek(graph, network)

            baseline = None
            for variant_name, flags in variants():
                if args.progress:
                    print(f"Running {case_name} {variant_name}...", file=sys.stderr, flush=True)
                result = run_infomap(args.binary, network, tmp_path, f"{case_name}_{variant_name}", flags, args.seed, args.num_trials, graph.number_of_nodes())
                if baseline is None:
                    baseline = result
                rows.append(
                    {
                        "case": case_name,
                        "variant": variant_name,
                        "codelength": result["codelength"],
                        "delta": result["codelength"] - baseline["codelength"],
                        "ari_vs_default": adjusted_rand_index(baseline["partition"], result["partition"]),
                        "ari_vs_planted": adjusted_rand_index(planted, result["partition"]),
                        "nmi_vs_default": normalized_mutual_info(baseline["partition"], result["partition"]),
                        "nmi_vs_planted": normalized_mutual_info(planted, result["partition"]),
                        "runtime_ratio": result["wall_time"] / baseline["wall_time"] if baseline["wall_time"] else 0.0,
                        "accepted": result["accepted"],
                        "rejected_no_gain": result["rejected_no_gain"],
                        "rejected_worse": result["rejected_worse"],
                        "unchanged": result["unchanged"],
                        "split_kept": result["split_kept"],
                    }
                )

    print_rows(rows)
    print_aggregate(rows)


if __name__ == "__main__":
    main()
