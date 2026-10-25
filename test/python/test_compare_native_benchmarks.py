from __future__ import annotations

import importlib.util
import json
from pathlib import Path

import pytest

pytestmark = pytest.mark.fast

MIB = 1024 * 1024


def _find_compare_script_path() -> Path:
    current = Path(__file__).resolve()
    relative_target = Path("scripts") / "benchmarks" / "compare_native_benchmarks.py"
    for parent in current.parents:
        candidate = parent / relative_target
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"Could not locate {relative_target} from {current}")


def _load_compare_module():
    module_path = _find_compare_script_path()
    spec = importlib.util.spec_from_file_location(
        "compare_native_benchmarks", module_path
    )
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
    median_peak_rss_bytes: int | None = 200 * MIB,
) -> dict[str, object]:
    case = {
        "name": name,
        "median_run_sec": median_run_sec,
        "cv_run_sec": cv_run_sec,
        "num_top_modules": num_top_modules,
        "num_levels": num_levels,
        "codelength": codelength,
    }
    if median_peak_rss_bytes is not None:
        case["median_peak_rss_bytes"] = median_peak_rss_bytes
    return case


def _timed_out(name: str) -> dict[str, object]:
    return {"name": name, "timed_out": True, "sample_timeout_sec": 120.0}


def _run_main(compare_module, argv: list[str]) -> int:
    import sys

    old_argv = sys.argv
    sys.argv = ["compare_native_benchmarks.py", *argv]
    try:
        return compare_module.main()
    finally:
        sys.argv = old_argv


def test_compare_reports_marks_stable_regression():
    compare_module = _load_compare_module()
    result = compare_module.compare_reports(
        {"benchmarks": [_case("ring", median_run_sec=0.50)]},
        {"benchmarks": [_case("ring", median_run_sec=0.60)]},
    )

    assert result["cases"][0]["status"] == "regression"
    assert result["cases"][0]["reason"] == "regression"
    assert result["overall_status"] == "possible_regression"


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


def test_compare_reports_detects_an_interleaved_pair():
    """Each report has to name the other's binary, so a half-filled pair is not enough."""
    compare_module = _load_compare_module()
    base = {
        "binary": "/tmp/base_bin",
        "interleaved_with": "/tmp/head_bin",
        "benchmarks": [_case("ring", median_run_sec=1.00)],
    }
    head = {
        "binary": "/tmp/head_bin",
        "interleaved_with": "/tmp/base_bin",
        "benchmarks": [_case("ring", median_run_sec=1.00)],
    }

    assert compare_module.compare_reports(base, head)["interleaved"] is True
    assert "alternating the two binaries" in compare_module.render_markdown(
        compare_module.compare_reports(base, head)
    )

    one_sided = dict(head, interleaved_with=None)
    assert compare_module.compare_reports(base, one_sided)["interleaved"] is False


def test_compare_reports_warns_when_the_runs_were_not_interleaved():
    """Reports from two separate invocations -- and older reports with no such field.

    A verdict from sequential blocks is not worth the same as one from an alternating
    run, and #943 happened because nothing said which kind was on screen.
    """
    compare_module = _load_compare_module()
    result = compare_module.compare_reports(
        {"benchmarks": [_case("ring", median_run_sec=1.00)]},
        {"benchmarks": [_case("ring", median_run_sec=1.30)]},
    )

    assert result["interleaved"] is False
    assert "measured separately" in compare_module.render_markdown(result)


def test_render_markdown_uses_na_for_missing_case_metrics():
    compare_module = _load_compare_module()
    result = compare_module.compare_reports(
        {"benchmarks": [_case("ring", median_run_sec=1.00)]},
        {"benchmarks": []},
    )

    markdown = compare_module.render_markdown(result)

    assert (
        "| ring | manual_review | n/a | n/a | n/a | n/a | n/a | n/a | n/a | case_missing |"
        in markdown
    )


def test_main_writes_markdown_and_json(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
):
    compare_module = _load_compare_module()
    base_path = tmp_path / "base.json"
    head_path = tmp_path / "head.json"
    markdown_path = tmp_path / "summary.md"
    json_path = tmp_path / "comparison.json"
    base_path.write_text(
        json.dumps({"benchmarks": [_case("ring", median_run_sec=1.00)]}) + "\n",
        encoding="utf-8",
    )
    head_path.write_text(
        json.dumps({"benchmarks": [_case("ring", median_run_sec=1.30)]}) + "\n",
        encoding="utf-8",
    )

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
        assert compare_module.main() == 0
    finally:
        sys.argv = old_argv

    captured = capsys.readouterr()
    assert "PR Native Benchmark Comparison" in captured.out
    assert "possible regression" in markdown_path.read_text(encoding="utf-8")
    assert (
        json.loads(json_path.read_text(encoding="utf-8"))["overall_status"]
        == "possible_regression"
    )


def test_main_can_fail_after_writing_reports_for_regression(tmp_path: Path):
    compare_module = _load_compare_module()
    base_path = tmp_path / "base.json"
    head_path = tmp_path / "head.json"
    markdown_path = tmp_path / "summary.md"
    json_path = tmp_path / "comparison.json"
    base_path.write_text(
        json.dumps({"benchmarks": [_case("ring", median_run_sec=1.00)]}) + "\n",
        encoding="utf-8",
    )
    head_path.write_text(
        json.dumps({"benchmarks": [_case("ring", median_run_sec=1.20)]}) + "\n",
        encoding="utf-8",
    )

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
        "--fail-on-regression",
    ]
    try:
        assert compare_module.main() == 1
    finally:
        sys.argv = old_argv

    assert "possible regression" in markdown_path.read_text(encoding="utf-8")
    assert (
        json.loads(json_path.read_text(encoding="utf-8"))["counts"]["regression"] == 1
    )


def test_output_change_acknowledgement_reads_its_own_line():
    """The reason after `Output-change:`, on a line of its own, in any letter case."""
    compare_module = _load_compare_module()
    ack = compare_module.output_change_acknowledgement

    assert ack(
        "## Summary\r\nText.\r\n\r\nOutput-change: numLevels counts the root\r\n"
    ) == ("numLevels counts the root")
    assert ack("  output-CHANGE:   new tie-break  ") == "new tie-break"
    assert ack("Output-change:") is None
    assert ack("Output-change:   \nnext line") is None
    assert ack("This PR has no Output-change: here") is None
    assert ack("") is None


def test_main_fails_on_unacknowledged_output_change(tmp_path: Path):
    """An output change used to pass green: main() only ever failed on regressions."""
    compare_module = _load_compare_module()
    base_path = tmp_path / "base.json"
    head_path = tmp_path / "head.json"
    markdown_path = tmp_path / "summary.md"
    base_path.write_text(
        json.dumps({"benchmarks": [_case("ring", median_run_sec=1.00)]}),
        encoding="utf-8",
    )
    head_path.write_text(
        json.dumps(
            {"benchmarks": [_case("ring", median_run_sec=1.00, codelength=1.5)]}
        ),
        encoding="utf-8",
    )
    argv = [
        "--base",
        str(base_path),
        "--head",
        str(head_path),
        "--markdown-out",
        str(markdown_path),
        "--fail-on-regression",
        "--fail-on-output-change",
    ]

    assert _run_main(compare_module, argv) == 1
    assert "`Output-change: <reason>`" in markdown_path.read_text(encoding="utf-8")

    description = tmp_path / "pr.md"
    # The line has to give a reason.
    description.write_text("Summary\n\nOutput-change:\n", encoding="utf-8")
    assert _run_main(compare_module, [*argv, "--pr-description", str(description)]) == 1

    description.write_text(
        "Summary\n\nOutput-change: codelength now includes the teleportation term\n",
        encoding="utf-8",
    )
    assert _run_main(compare_module, [*argv, "--pr-description", str(description)]) == 0
    assert (
        "Output change acknowledged in the PR description: codelength now includes "
        "the teleportation term" in markdown_path.read_text(encoding="utf-8")
    )

    # Without the flag an output change is reported but does not fail, as before.
    assert _run_main(compare_module, argv[:-1]) == 0


def test_acknowledged_output_change_does_not_hide_a_regression():
    """The headline names what fails the gate, and an acknowledged change does not."""
    compare_module = _load_compare_module()
    base = {
        "benchmarks": [
            _case("ring", median_run_sec=1.00),
            _case("sparse", median_run_sec=1.00),
        ]
    }
    head = {
        "benchmarks": [
            _case("ring", median_run_sec=1.00, num_levels=3),
            _case("sparse", median_run_sec=1.30),
        ]
    }

    acknowledged = compare_module.compare_reports(
        base, head, "levels now count the root"
    )
    assert acknowledged["overall_status"] == "possible_regression"

    unacknowledged = compare_module.compare_reports(base, head)
    assert unacknowledged["overall_status"] == "manual_review"
    assert unacknowledged["summary"] == "output changed, not acknowledged"


def test_head_timeout_fails_and_base_timeout_does_not(tmp_path: Path):
    """Only a head timeout is the PR's doing; a PR that fixes a base hang must pass."""
    compare_module = _load_compare_module()

    head_hung = compare_module.compare_reports(
        {"benchmarks": [_case("ring", median_run_sec=1.00)]},
        {"benchmarks": [_timed_out("ring")]},
    )
    assert head_hung["cases"][0]["status"] == "timeout"
    assert head_hung["cases"][0]["reason"] == "head_timed_out"
    assert head_hung["overall_status"] == "timeout"
    assert "killed on ring (120s)" in compare_module.render_markdown(head_hung)

    base_hung = compare_module.compare_reports(
        {"benchmarks": [_timed_out("ring")]},
        {"benchmarks": [_case("ring", median_run_sec=1.00)]},
    )
    assert base_hung["cases"][0]["status"] == "inconclusive"
    assert base_hung["cases"][0]["reason"] == "base_timed_out"
    assert base_hung["counts"]["timeout"] == 0

    base_path = tmp_path / "base.json"
    head_path = tmp_path / "head.json"
    base_path.write_text(
        json.dumps({"benchmarks": [_case("ring", median_run_sec=1.00)]}),
        encoding="utf-8",
    )
    head_path.write_text(
        json.dumps({"benchmarks": [_timed_out("ring")]}), encoding="utf-8"
    )
    argv = ["--base", str(base_path), "--head", str(head_path), "--fail-on-regression"]
    assert _run_main(compare_module, argv) == 1


def test_compare_reports_flags_a_memory_regression_regardless_of_timing():
    """Peak RSS was recorded but never compared, so memory could not fail the gate.

    The case's runtime is below the timing floor, which makes the wall-time verdict
    inconclusive; that says nothing about memory.
    """
    compare_module = _load_compare_module()
    result = compare_module.compare_reports(
        {
            "benchmarks": [
                _case("ring", median_run_sec=0.01, median_peak_rss_bytes=200 * MIB)
            ]
        },
        {
            "benchmarks": [
                _case("ring", median_run_sec=0.01, median_peak_rss_bytes=230 * MIB)
            ]
        },
    )

    case = result["cases"][0]
    assert case["status"] == "regression"
    assert case["reason"] == "memory_regression"
    assert case["memory_regression"] is True
    assert case["rss_delta_pct"] == pytest.approx(0.15)
    assert "| 200.00 | 230.00 | 15.0 | memory_regression |" in (
        compare_module.render_markdown(result)
    )


def test_compare_reports_ignores_small_or_unrecorded_rss_changes():
    compare_module = _load_compare_module()

    # 50% more, but only 2 MiB: the small cases' peak is mostly process start-up.
    small = compare_module.compare_reports(
        {
            "benchmarks": [
                _case("ring", median_run_sec=1.00, median_peak_rss_bytes=4 * MIB)
            ]
        },
        {
            "benchmarks": [
                _case("ring", median_run_sec=1.00, median_peak_rss_bytes=6 * MIB)
            ]
        },
    )
    assert small["cases"][0]["status"] == "ok"

    # 8 MiB more, but under 10% of a large peak.
    relative = compare_module.compare_reports(
        {
            "benchmarks": [
                _case("ring", median_run_sec=1.00, median_peak_rss_bytes=200 * MIB)
            ]
        },
        {
            "benchmarks": [
                _case("ring", median_run_sec=1.00, median_peak_rss_bytes=210 * MIB)
            ]
        },
    )
    assert relative["cases"][0]["status"] == "ok"

    # Reports from before the harness recorded a median peak.
    unrecorded = compare_module.compare_reports(
        {
            "benchmarks": [
                _case("ring", median_run_sec=1.00, median_peak_rss_bytes=None)
            ]
        },
        {
            "benchmarks": [
                _case("ring", median_run_sec=1.00, median_peak_rss_bytes=None)
            ]
        },
    )
    assert unrecorded["cases"][0]["status"] == "ok"
    assert unrecorded["cases"][0]["rss_delta_pct"] is None
