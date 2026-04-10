from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

RELATIVE_REGRESSION_THRESHOLD = 0.15
ABSOLUTE_REGRESSION_THRESHOLD_SEC = 0.05
SEVERE_RELATIVE_REGRESSION_THRESHOLD = 0.25
SEVERE_ABSOLUTE_REGRESSION_THRESHOLD_SEC = 0.10
CV_INCONCLUSIVE_THRESHOLD = 0.10
MIN_BASELINE_RUNTIME_SEC = 0.20
CODELENGTH_REL_TOL = 1e-9
CODELENGTH_ABS_TOL = 1e-9


def load_report(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def index_benchmarks(report: dict[str, object]) -> dict[str, dict[str, object]]:
    return {str(benchmark["name"]): benchmark for benchmark in report.get("benchmarks", [])}


def outputs_match(base_case: dict[str, object], head_case: dict[str, object]) -> bool:
    if int(base_case["num_top_modules"]) != int(head_case["num_top_modules"]):
        return False
    if int(base_case["num_levels"]) != int(head_case["num_levels"]):
        return False
    return math.isclose(
        float(base_case["codelength"]),
        float(head_case["codelength"]),
        rel_tol=CODELENGTH_REL_TOL,
        abs_tol=CODELENGTH_ABS_TOL,
    )


def classify_case(name: str, base_case: dict[str, object], head_case: dict[str, object]) -> dict[str, object]:
    base_runtime = float(base_case["median_run_sec"])
    head_runtime = float(head_case["median_run_sec"])
    delta_sec = head_runtime - base_runtime
    delta_pct = (delta_sec / base_runtime) if base_runtime else 0.0
    base_cv = float(base_case.get("cv_run_sec", 0.0))
    head_cv = float(head_case.get("cv_run_sec", 0.0))
    status = "ok"
    reason = "within_threshold"
    severe = False

    if not outputs_match(base_case, head_case):
        status = "manual_review"
        reason = "output_changed"
    elif base_runtime < MIN_BASELINE_RUNTIME_SEC:
        status = "inconclusive"
        reason = "baseline_too_small"
    elif max(base_cv, head_cv) > CV_INCONCLUSIVE_THRESHOLD:
        status = "inconclusive"
        reason = "high_variance"
    elif delta_sec >= SEVERE_ABSOLUTE_REGRESSION_THRESHOLD_SEC and delta_pct >= SEVERE_RELATIVE_REGRESSION_THRESHOLD:
        status = "regression"
        reason = "severe_regression"
        severe = True
    elif delta_sec >= ABSOLUTE_REGRESSION_THRESHOLD_SEC and delta_pct >= RELATIVE_REGRESSION_THRESHOLD:
        status = "regression"
        reason = "regression"
    elif delta_sec <= -ABSOLUTE_REGRESSION_THRESHOLD_SEC and delta_pct <= -RELATIVE_REGRESSION_THRESHOLD:
        status = "improvement"
        reason = "improvement"

    return {
        "name": name,
        "status": status,
        "reason": reason,
        "severe": severe,
        "base_median_run_sec": base_runtime,
        "head_median_run_sec": head_runtime,
        "delta_sec": delta_sec,
        "delta_pct": delta_pct,
        "base_cv_run_sec": base_cv,
        "head_cv_run_sec": head_cv,
        "base_num_top_modules": int(base_case["num_top_modules"]),
        "head_num_top_modules": int(head_case["num_top_modules"]),
        "base_num_levels": int(base_case["num_levels"]),
        "head_num_levels": int(head_case["num_levels"]),
        "base_codelength": float(base_case["codelength"]),
        "head_codelength": float(head_case["codelength"]),
    }


def compare_reports(base_report: dict[str, object], head_report: dict[str, object]) -> dict[str, object]:
    base_benchmarks = index_benchmarks(base_report)
    head_benchmarks = index_benchmarks(head_report)
    all_names = sorted(set(base_benchmarks) | set(head_benchmarks))

    cases: list[dict[str, object]] = []
    for name in all_names:
        if name not in base_benchmarks or name not in head_benchmarks:
            cases.append(
                {
                    "name": name,
                    "status": "manual_review",
                    "reason": "case_missing",
                    "severe": False,
                }
            )
            continue
        cases.append(classify_case(name, base_benchmarks[name], head_benchmarks[name]))

    manual_review = [case for case in cases if case["status"] == "manual_review"]
    regressions = [case for case in cases if case["status"] == "regression"]
    severe_regressions = [case for case in regressions if case["severe"]]
    inconclusive = [case for case in cases if case["status"] == "inconclusive"]
    improvements = [case for case in cases if case["status"] == "improvement"]

    if manual_review:
        overall_status = "manual_review"
        summary = "manual review needed"
    elif len(regressions) >= 2 or severe_regressions:
        overall_status = "possible_regression"
        summary = "possible regression"
    else:
        overall_status = "no_regression"
        summary = "no regression"

    return {
        "overall_status": overall_status,
        "summary": summary,
        "thresholds": {
            "relative_regression_threshold": RELATIVE_REGRESSION_THRESHOLD,
            "absolute_regression_threshold_sec": ABSOLUTE_REGRESSION_THRESHOLD_SEC,
            "severe_relative_regression_threshold": SEVERE_RELATIVE_REGRESSION_THRESHOLD,
            "severe_absolute_regression_threshold_sec": SEVERE_ABSOLUTE_REGRESSION_THRESHOLD_SEC,
            "cv_inconclusive_threshold": CV_INCONCLUSIVE_THRESHOLD,
            "min_baseline_runtime_sec": MIN_BASELINE_RUNTIME_SEC,
        },
        "counts": {
            "regression": len(regressions),
            "inconclusive": len(inconclusive),
            "improvement": len(improvements),
            "manual_review": len(manual_review),
        },
        "cases": cases,
    }


def render_markdown(result: dict[str, object]) -> str:
    lines = [
        "## PR Native Benchmark Comparison",
        "",
        f"Overall result: **{result['summary']}**",
        "",
        "| Case | Status | Base median run (s) | Head median run (s) | Delta (s) | Delta (%) | Reason |",
        "| --- | --- | ---: | ---: | ---: | ---: | --- |",
    ]

    for case in result["cases"]:
        base_runtime = case.get("base_median_run_sec")
        head_runtime = case.get("head_median_run_sec")
        delta_sec = case.get("delta_sec")
        delta_pct = case.get("delta_pct")

        if base_runtime is None or head_runtime is None or delta_sec is None or delta_pct is None:
            base_runtime_text = "n/a"
            head_runtime_text = "n/a"
            delta_sec_text = "n/a"
            delta_pct_text = "n/a"
        else:
            base_runtime_text = f"{float(base_runtime):.6f}"
            head_runtime_text = f"{float(head_runtime):.6f}"
            delta_sec_text = f"{float(delta_sec):.6f}"
            delta_pct_text = f"{float(delta_pct) * 100.0:.1f}"

        lines.append(
            "| {name} | {status} | {base_runtime} | {head_runtime} | {delta_sec} | {delta_pct} | {reason} |".format(
                name=case["name"],
                status=case["status"],
                base_runtime=base_runtime_text,
                head_runtime=head_runtime_text,
                delta_sec=delta_sec_text,
                delta_pct=delta_pct_text,
                reason=case["reason"],
            )
        )

    counts = result["counts"]
    lines.extend(
        [
            "",
            "Thresholds: "
            f"regression >= {RELATIVE_REGRESSION_THRESHOLD:.0%} and >= {ABSOLUTE_REGRESSION_THRESHOLD_SEC:.2f}s, "
            f"severe >= {SEVERE_RELATIVE_REGRESSION_THRESHOLD:.0%} and >= {SEVERE_ABSOLUTE_REGRESSION_THRESHOLD_SEC:.2f}s, "
            f"inconclusive when baseline < {MIN_BASELINE_RUNTIME_SEC:.2f}s or CV > {CV_INCONCLUSIVE_THRESHOLD:.2f}.",
            "",
            f"Counts: {counts['regression']} regression, {counts['manual_review']} manual review, {counts['inconclusive']} inconclusive, {counts['improvement']} improvement.",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Compare native benchmark reports for PR perf regression detection.")
    parser.add_argument("--base", type=Path, required=True, help="Path to the base benchmark JSON report.")
    parser.add_argument("--head", type=Path, required=True, help="Path to the head benchmark JSON report.")
    parser.add_argument("--markdown-out", type=Path, default=None, help="Optional Markdown summary output path.")
    parser.add_argument("--json-out", type=Path, default=None, help="Optional JSON comparison output path.")
    args = parser.parse_args()

    result = compare_reports(load_report(args.base), load_report(args.head))
    markdown = render_markdown(result)

    if args.markdown_out is not None:
        args.markdown_out.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_out.write_text(markdown, encoding="utf-8")

    if args.json_out is not None:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    print(markdown)


if __name__ == "__main__":
    main()
