from __future__ import annotations

import importlib.util
import json
from pathlib import Path

import pytest


pytestmark = pytest.mark.fast


def _load_benchmark_module():
    module_path = Path(__file__).resolve().parents[2] / "scripts" / "benchmarks" / "run_native_benchmarks.py"
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
