from __future__ import annotations

import importlib.util
import json
from pathlib import Path

import pytest


pytestmark = pytest.mark.fast


def _find_repo_root(start: Path) -> Path:
    target = Path("scripts") / "benchmarks" / "run_native_benchmarks.py"
    for candidate in (start, *start.parents):
        if (candidate / target).exists():
            return candidate
    raise FileNotFoundError(f"Could not locate repository root containing {target!s} from {start!s}")


def _load_benchmark_module():
    repo_root = _find_repo_root(Path(__file__).resolve())
    module_path = repo_root / "scripts" / "benchmarks" / "run_native_benchmarks.py"
    spec = importlib.util.spec_from_file_location("run_native_benchmarks", module_path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_benchmark_case_tolerates_missing_rebuild_metrics(monkeypatch, tmp_path: Path):
    benchmark_module = _load_benchmark_module()
    payload = {
        "flags": "--silent --no-file-output --num-trials 1 --seed 123",
        "total_sec": 0.3,
        "read_input_sec": 0.1,
        "run_sec": 0.2,
        "peak_rss_bytes": 4096,
        "node_size_bytes": 32,
        "edge_size_bytes": 16,
        "num_nodes": 5,
        "num_links": 4,
        "num_top_modules": 2,
        "num_levels": 1,
        "codelength": 1.5,
        "higher_order_input": False,
        "directed_input": False,
        "runs": [
            {
                "iteration": 1,
                "read_input_sec": 0.0,
                "run_sec": 0.2,
                "total_sec": 0.2,
                "peak_rss_bytes": 4096,
                "num_top_modules": 2,
                "num_levels": 1,
                "codelength": 1.5,
            }
        ],
    }

    def fake_run(*args, **kwargs):
        return benchmark_module.subprocess.CompletedProcess(args[0], 0, stdout=json.dumps(payload), stderr="")

    monkeypatch.setattr(benchmark_module.subprocess, "run", fake_run)

    result = benchmark_module.benchmark_case(
        binary=tmp_path / "infomap_native_benchmark",
        name="toy",
        network_path=tmp_path / "toy.net",
        repeats=1,
        iterations=1,
        flags="--silent",
    )

    assert result["rebuild"]["mean_total_sec"] == 0.0
    assert result["rebuild"]["median_total_calls"] == 0
    assert result["rebuild"]["module_size_buckets"] == {}


def test_benchmark_case_collects_dynamic_rebuild_bucket_labels(monkeypatch, tmp_path: Path):
    benchmark_module = _load_benchmark_module()
    payload = {
        "flags": "--silent --no-file-output --num-trials 1 --seed 123",
        "total_sec": 0.5,
        "read_input_sec": 0.1,
        "run_sec": 0.4,
        "peak_rss_bytes": 8192,
        "node_size_bytes": 32,
        "edge_size_bytes": 16,
        "num_nodes": 8,
        "num_links": 12,
        "num_top_modules": 3,
        "num_levels": 2,
        "codelength": 2.5,
        "higher_order_input": False,
        "directed_input": True,
        "runs": [
            {
                "iteration": 1,
                "read_input_sec": 0.0,
                "run_sec": 0.4,
                "total_sec": 0.4,
                "peak_rss_bytes": 8192,
                "num_top_modules": 3,
                "num_levels": 2,
                "codelength": 2.5,
                "rebuild": {
                    "total_sec": 0.05,
                    "network_sec": 0.01,
                    "module_sec": 0.04,
                    "total_calls": 7,
                    "network_calls": 2,
                    "module_calls": 5,
                    "module_size_buckets": {
                        "33-64": {
                            "calls": 2,
                            "sec": 0.03,
                            "prep_sec": 0.01,
                            "index_sec": 0.005,
                            "reserve_sec": 0.004,
                            "clone_sec": 0.006,
                            "edge_clone_sec": 0.005,
                        }
                    },
                },
            }
        ],
    }

    def fake_run(*args, **kwargs):
        return benchmark_module.subprocess.CompletedProcess(args[0], 0, stdout=json.dumps(payload), stderr="")

    monkeypatch.setattr(benchmark_module.subprocess, "run", fake_run)

    result = benchmark_module.benchmark_case(
        binary=tmp_path / "infomap_native_benchmark",
        name="toy",
        network_path=tmp_path / "toy.net",
        repeats=1,
        iterations=1,
        flags="--silent",
    )

    assert result["rebuild"]["mean_total_sec"] == pytest.approx(0.05)
    assert result["rebuild"]["module_size_buckets"]["33-64"]["mean_sec"] == pytest.approx(0.03)


def test_build_benchmark_cases_pr_profile_focuses_on_stable_cases(monkeypatch, tmp_path: Path):
    benchmark_module = _load_benchmark_module()
    repo_root = Path(__file__).resolve().parents[2]
    generated_paths: list[Path] = []

    def stub_generate_state_ring(path: Path, physical_nodes: int) -> None:
        generated_paths.append(path)

    def stub_generate_sparse_graph(path: Path, num_nodes: int, avg_degree: int, seed: int) -> None:
        generated_paths.append(path)

    def stub_generate_ring_of_cliques(path: Path, clique_count: int, clique_size: int) -> None:
        generated_paths.append(path)

    monkeypatch.setattr(benchmark_module, "generate_state_ring", stub_generate_state_ring)
    monkeypatch.setattr(benchmark_module, "generate_sparse_graph", stub_generate_sparse_graph)
    monkeypatch.setattr(benchmark_module, "generate_ring_of_cliques", stub_generate_ring_of_cliques)

    cases = benchmark_module.build_benchmark_cases("pr", repo_root, tmp_path)
    names = [case["name"] for case in cases]
    paths = [Path(case["path"]) for case in cases]

    assert names == ["states_meta", "state_ring_5k", "sparse_100k", "ring_of_cliques_100k"]
    assert paths[0] == repo_root / "examples" / "networks" / "states.net"
    assert paths[1:] == generated_paths
    assert [path.name for path in generated_paths] == ["state_ring_5k.net", "sparse_100k.net", "ring_of_cliques_100k.net"]


def test_build_benchmark_cases_smoke_skips_100k_generation(monkeypatch, tmp_path: Path):
    benchmark_module = _load_benchmark_module()
    repo_root = _find_repo_root(Path(__file__).resolve())
    calls: list[int] = []

    original_generate_sparse_graph = benchmark_module.generate_sparse_graph

    def recording_generate_sparse_graph(path: Path, num_nodes: int, avg_degree: int, seed: int) -> None:
        calls.append(num_nodes)
        original_generate_sparse_graph(path, num_nodes, avg_degree, seed)

    monkeypatch.setattr(benchmark_module, "generate_sparse_graph", recording_generate_sparse_graph)

    benchmark_module.build_benchmark_cases("smoke", repo_root, tmp_path)

    assert calls == [10_000]
