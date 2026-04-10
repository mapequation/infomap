from __future__ import annotations

import importlib.util
import json
from pathlib import Path

import pytest


pytestmark = pytest.mark.fast


def _load_compare_module():
    module_path = Path(__file__).resolve().parents[2] / "scripts" / "benchmarks" / "compare_native_benchmarks.py"
    spec = importlib.util.spec_from_file_location("compare_native_benchmarks", module_path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _case(
    name: str,
    *,
    median_run_sec: float,
    cv_run_sec: float = 0.02,
    num_top_modules: int = 3,
    num_levels: int = 2,
    codelength: float = 1.234,
) -> dict[str, object]:
    return {
        "name": name,
        "median_run_sec": median_run_sec,
        "cv_run_sec": cv_run_sec,
        "num_top_modules": num_top_modules,
        "num_levels": num_levels,
        "codelength": codelength,
    }


def test_compare_reports_marks_stable_regression():
    compare_module = _load_compare_module()
    result = compare_module.compare_reports(
        {"benchmarks": [_case("ring", median_run_sec=0.50)]},
        {"benchmarks": [_case("ring", median_run_sec=0.60)]},
    )

    assert result["cases"][0]["status"] == "regression"
    assert result["cases"][0]["reason"] == "regression"
    assert result["overall_status"] == "no_regression"


def test_compare_reports_ignores_small_delta():
    compare_module = _load_compare_module()
    result = compare_module.compare_reports(
        {"benchmarks": [_case("ring", median_run_sec=1.00)]},
        {"benchmarks": [_case("ring", median_run_sec=1.10)]},
    )

    assert result["cases"][0]["status"] == "ok"
    assert result["overall_status"] == "no_regression"


def test_compare_reports_marks_high_variance_inconclusive():
    compare_module = _load_compare_module()
    result = compare_module.compare_reports(
        {"benchmarks": [_case("ring", median_run_sec=1.00, cv_run_sec=0.11)]},
        {"benchmarks": [_case("ring", median_run_sec=1.30, cv_run_sec=0.04)]},
    )

    assert result["cases"][0]["status"] == "inconclusive"
    assert result["cases"][0]["reason"] == "high_variance"


def test_compare_reports_promotes_single_severe_regression():
    compare_module = _load_compare_module()
    result = compare_module.compare_reports(
        {"benchmarks": [_case("ring", median_run_sec=0.50)]},
        {"benchmarks": [_case("ring", median_run_sec=0.70)]},
    )

    assert result["cases"][0]["status"] == "regression"
    assert result["cases"][0]["severe"] is True
    assert result["overall_status"] == "possible_regression"


def test_compare_reports_requires_manual_review_when_outputs_change():
    compare_module = _load_compare_module()
    result = compare_module.compare_reports(
        {"benchmarks": [_case("ring", median_run_sec=1.00, num_top_modules=3)]},
        {"benchmarks": [_case("ring", median_run_sec=1.20, num_top_modules=4)]},
    )

    assert result["cases"][0]["status"] == "manual_review"
    assert result["overall_status"] == "manual_review"


def test_main_writes_markdown_and_json(tmp_path: Path, capsys: pytest.CaptureFixture[str]):
    compare_module = _load_compare_module()
    base_path = tmp_path / "base.json"
    head_path = tmp_path / "head.json"
    markdown_path = tmp_path / "summary.md"
    json_path = tmp_path / "comparison.json"
    base_path.write_text(json.dumps({"benchmarks": [_case("ring", median_run_sec=1.00)]}) + "\n", encoding="utf-8")
    head_path.write_text(json.dumps({"benchmarks": [_case("ring", median_run_sec=1.30)]}) + "\n", encoding="utf-8")

    import sys

    old_argv = sys.argv
    sys.argv = [
        "compare_native_benchmarks.py",
        "--base",
        str(base_path),
        "--head",
        str(head_path),
        "--markdown-out",
        str(markdown_path),
        "--json-out",
        str(json_path),
    ]
    try:
        compare_module.main()
    finally:
        sys.argv = old_argv

    captured = capsys.readouterr()
    assert "PR Native Benchmark Comparison" in captured.out
    assert "possible regression" in markdown_path.read_text(encoding="utf-8")
    assert json.loads(json_path.read_text(encoding="utf-8"))["overall_status"] == "possible_regression"
