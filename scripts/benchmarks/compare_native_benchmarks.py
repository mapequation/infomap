from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path

RELATIVE_REGRESSION_THRESHOLD = 0.15
ABSOLUTE_REGRESSION_THRESHOLD_SEC = 0.05
SEVERE_RELATIVE_REGRESSION_THRESHOLD = 0.25
SEVERE_ABSOLUTE_REGRESSION_THRESHOLD_SEC = 0.10
CV_INCONCLUSIVE_THRESHOLD = 0.10
MIN_BASELINE_RUNTIME_SEC = 0.20
CODELENGTH_REL_TOL = 1e-9
CODELENGTH_ABS_TOL = 1e-9
# Peak RSS barely moves with machine load: serial repeats of one binary stayed within
# about 1% of each other, so a memory regression can be judged on a tighter relative
# bound than wall time. The absolute floor keeps the small cases, whose peak is
# mostly process start-up, from tripping on a few pages.
RSS_RELATIVE_REGRESSION_THRESHOLD = 0.10
RSS_ABSOLUTE_REGRESSION_THRESHOLD_BYTES = 8 * 1024 * 1024
# A changed partition on a reference input is a correctness event for an optimizer,
# so it fails the gate unless the PR description says it is intended, on a line of
# its own: `Output-change: <reason>`.
OUTPUT_CHANGE_ACK_PATTERN = re.compile(
    r"^[ \t]*Output-change:[ \t]*(\S.*?)\s*$", re.IGNORECASE | re.MULTILINE
)


def load_report(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def output_change_acknowledgement(pr_description: str) -> str | None:
    """The reason on the PR description's `Output-change:` line, or None."""
    match = OUTPUT_CHANGE_ACK_PATTERN.search(pr_description)
    return match.group(1) if match else None


def index_benchmarks(report: dict[str, object]) -> dict[str, dict[str, object]]:
    return {
        str(benchmark["name"]): benchmark for benchmark in report.get("benchmarks", [])
    }


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


def classify_timeout(
    name: str, base_case: dict[str, object], head_case: dict[str, object]
) -> dict[str, object] | None:
    """Verdict for a case where a binary was killed at the per-sample timeout.

    Only a head timeout fails: a base that hangs is master's problem, and the PR
    that fixes the hang would otherwise be the one the gate blocks.
    """
    base_timed_out = bool(base_case.get("timed_out"))
    head_timed_out = bool(head_case.get("timed_out"))
    if not (base_timed_out or head_timed_out):
        return None
    if head_timed_out:
        status = "timeout"
        reason = "both_timed_out" if base_timed_out else "head_timed_out"
    else:
        status = "inconclusive"
        reason = "base_timed_out"
    timed_out_case = head_case if head_timed_out else base_case
    return {
        "name": name,
        "status": status,
        "reason": reason,
        "severe": False,
        "sample_timeout_sec": timed_out_case.get("sample_timeout_sec"),
    }


def classify_case(
    name: str, base_case: dict[str, object], head_case: dict[str, object]
) -> dict[str, object]:
    base_runtime = float(base_case["median_run_sec"])
    head_runtime = float(head_case["median_run_sec"])
    delta_sec = head_runtime - base_runtime
    delta_pct = (delta_sec / base_runtime) if base_runtime else 0.0
    base_cv = float(base_case.get("cv_run_sec", 0.0))
    head_cv = float(head_case.get("cv_run_sec", 0.0))
    # The fastest of the repeats, reported so a borderline verdict can be read by a
    # human, but deliberately not part of the verdict. It does not separate noise
    # from a regression: a disturbance lasting a whole measurement block slows every
    # repeat, so the minimum moves with the median (measured on this runner: a
    # sequential A/A comparison under load moved the median 2.339 -> 2.709 s and the
    # minimum 2.332 -> 2.707 s). Interleaving is what handles that; see
    # run_native_benchmarks.benchmark_case_pair.
    base_min = float(base_case.get("min_run_sec", base_runtime))
    head_min = float(head_case.get("min_run_sec", head_runtime))
    min_delta_sec = head_min - base_min
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
    elif (
        delta_sec >= SEVERE_ABSOLUTE_REGRESSION_THRESHOLD_SEC
        and delta_pct >= SEVERE_RELATIVE_REGRESSION_THRESHOLD
    ):
        status = "regression"
        reason = "severe_regression"
        severe = True
    elif (
        delta_sec >= ABSOLUTE_REGRESSION_THRESHOLD_SEC
        and delta_pct >= RELATIVE_REGRESSION_THRESHOLD
    ):
        status = "regression"
        reason = "regression"
    elif (
        delta_sec <= -ABSOLUTE_REGRESSION_THRESHOLD_SEC
        and delta_pct <= -RELATIVE_REGRESSION_THRESHOLD
    ):
        status = "improvement"
        reason = "improvement"

    # Reports from before the harness recorded a median peak have nothing to compare.
    base_rss = base_case.get("median_peak_rss_bytes")
    head_rss = head_case.get("median_peak_rss_bytes")
    rss_delta_bytes = None
    rss_delta_pct = None
    memory_regression = False
    if base_rss and head_rss is not None:
        rss_delta_bytes = float(head_rss) - float(base_rss)
        rss_delta_pct = rss_delta_bytes / float(base_rss)
        memory_regression = (
            rss_delta_bytes >= RSS_ABSOLUTE_REGRESSION_THRESHOLD_BYTES
            and rss_delta_pct >= RSS_RELATIVE_REGRESSION_THRESHOLD
        )
    # Judged apart from wall time, because a short baseline or noisy timings say
    # nothing about memory. A changed output still takes precedence: the two runs
    # then did different work.
    if memory_regression and status in {"ok", "improvement", "inconclusive"}:
        status = "regression"
        reason = "memory_regression"

    return {
        "name": name,
        "status": status,
        "reason": reason,
        "severe": severe,
        "base_median_run_sec": base_runtime,
        "head_median_run_sec": head_runtime,
        "base_min_run_sec": base_min,
        "head_min_run_sec": head_min,
        "min_delta_sec": min_delta_sec,
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
        "base_median_peak_rss_bytes": base_rss,
        "head_median_peak_rss_bytes": head_rss,
        "rss_delta_bytes": rss_delta_bytes,
        "rss_delta_pct": rss_delta_pct,
        "memory_regression": memory_regression,
    }


def reports_are_interleaved(
    base_report: dict[str, object], head_report: dict[str, object]
) -> bool:
    """True when each report names the other's binary as the one it alternated with.

    ``run_native_benchmarks --compare-binary`` records this; two independent
    invocations leave it unset, and older reports predate the field entirely.
    """
    base_binary = base_report.get("binary")
    head_binary = head_report.get("binary")
    base_partner = base_report.get("interleaved_with")
    head_partner = head_report.get("interleaved_with")
    # All four have to be present: a report that predates the field has None on both
    # sides, and comparing None to None would otherwise read as a match.
    if None in (base_binary, head_binary, base_partner, head_partner):
        return False
    return base_partner == head_binary and head_partner == base_binary


def compare_reports(
    base_report: dict[str, object],
    head_report: dict[str, object],
    output_change_ack: str | None = None,
) -> dict[str, object]:
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
        base_case = base_benchmarks[name]
        head_case = head_benchmarks[name]
        cases.append(
            classify_timeout(name, base_case, head_case)
            or classify_case(name, base_case, head_case)
        )

    timeouts = [case for case in cases if case["status"] == "timeout"]
    manual_review = [case for case in cases if case["status"] == "manual_review"]
    regressions = [case for case in cases if case["status"] == "regression"]
    inconclusive = [case for case in cases if case["status"] == "inconclusive"]
    improvements = [case for case in cases if case["status"] == "improvement"]

    # Ordered so the headline names what fails the gate: an acknowledged output
    # change does not, and must not hide a regression in another case.
    if timeouts:
        overall_status = "timeout"
        summary = "benchmark timed out"
    elif manual_review and not output_change_ack:
        overall_status = "manual_review"
        summary = "output changed, not acknowledged"
    elif regressions:
        overall_status = "possible_regression"
        summary = "possible regression"
    elif manual_review:
        overall_status = "manual_review"
        summary = "output changed (acknowledged)"
    else:
        overall_status = "no_regression"
        summary = "no regression"

    return {
        "overall_status": overall_status,
        "summary": summary,
        # Whether the two runs were measured against each other, alternating, rather
        # than as two separate blocks. Surfaced because it decides how much a verdict
        # is worth: two sequential blocks on a shared runner reported the same commit
        # as both a 17.3% regression and a 1.9% pass (#943).
        "interleaved": reports_are_interleaved(base_report, head_report),
        "output_change_ack": output_change_ack,
        "thresholds": {
            "relative_regression_threshold": RELATIVE_REGRESSION_THRESHOLD,
            "absolute_regression_threshold_sec": ABSOLUTE_REGRESSION_THRESHOLD_SEC,
            "severe_relative_regression_threshold": SEVERE_RELATIVE_REGRESSION_THRESHOLD,
            "severe_absolute_regression_threshold_sec": SEVERE_ABSOLUTE_REGRESSION_THRESHOLD_SEC,
            "cv_inconclusive_threshold": CV_INCONCLUSIVE_THRESHOLD,
            "min_baseline_runtime_sec": MIN_BASELINE_RUNTIME_SEC,
            "rss_relative_regression_threshold": RSS_RELATIVE_REGRESSION_THRESHOLD,
            "rss_absolute_regression_threshold_bytes": RSS_ABSOLUTE_REGRESSION_THRESHOLD_BYTES,
        },
        "counts": {
            "timeout": len(timeouts),
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
        (
            "Measured by alternating the two binaries on each case."
            if result.get("interleaved")
            else "Note: the two runs were measured separately rather than alternating, "
            "so a difference here can be machine noise as easily as a code change."
        ),
        "",
        "| Case | Status | Base median run (s) | Head median run (s) | Delta (s) | Delta (%) | Base peak RSS (MiB) | Head peak RSS (MiB) | RSS delta (%) | Reason |",
        "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    for case in result["cases"]:
        base_runtime = case.get("base_median_run_sec")
        head_runtime = case.get("head_median_run_sec")
        delta_sec = case.get("delta_sec")
        delta_pct = case.get("delta_pct")

        if (
            base_runtime is None
            or head_runtime is None
            or delta_sec is None
            or delta_pct is None
        ):
            base_runtime_text = "n/a"
            head_runtime_text = "n/a"
            delta_sec_text = "n/a"
            delta_pct_text = "n/a"
        else:
            base_runtime_text = f"{float(base_runtime):.6f}"
            head_runtime_text = f"{float(head_runtime):.6f}"
            delta_sec_text = f"{float(delta_sec):.6f}"
            delta_pct_text = f"{float(delta_pct) * 100.0:.1f}"

        base_rss = case.get("base_median_peak_rss_bytes")
        head_rss = case.get("head_median_peak_rss_bytes")
        rss_delta_pct = case.get("rss_delta_pct")
        if base_rss is None or head_rss is None or rss_delta_pct is None:
            base_rss_text = "n/a"
            head_rss_text = "n/a"
            rss_delta_pct_text = "n/a"
        else:
            base_rss_text = f"{float(base_rss) / (1024.0 * 1024.0):.2f}"
            head_rss_text = f"{float(head_rss) / (1024.0 * 1024.0):.2f}"
            rss_delta_pct_text = f"{float(rss_delta_pct) * 100.0:.1f}"

        lines.append(
            "| {name} | {status} | {base_runtime} | {head_runtime} | {delta_sec} | {delta_pct} | {base_rss} | {head_rss} | {rss_delta_pct} | {reason} |".format(
                name=case["name"],
                status=case["status"],
                base_runtime=base_runtime_text,
                head_runtime=head_runtime_text,
                delta_sec=delta_sec_text,
                delta_pct=delta_pct_text,
                base_rss=base_rss_text,
                head_rss=head_rss_text,
                rss_delta_pct=rss_delta_pct_text,
                reason=case["reason"],
            )
        )

    timed_out = [case for case in result["cases"] if case["status"] == "timeout"]
    if timed_out:
        lines.extend(
            [
                "",
                "**Timed out:** the head binary ran past the per-sample limit and was "
                "killed on "
                + ", ".join(
                    f"{case['name']} ({float(case['sample_timeout_sec']):g}s)"
                    if case.get("sample_timeout_sec") is not None
                    else str(case["name"])
                    for case in timed_out
                )
                + ".",
            ]
        )

    changed = [
        case["name"] for case in result["cases"] if case["status"] == "manual_review"
    ]
    if changed:
        ack = result.get("output_change_ack")
        lines.append("")
        if ack:
            lines.append(f"Output change acknowledged in the PR description: {ack}")
        else:
            lines.append(
                f"**Output changed** on {', '.join(changed)}: the module count, level "
                "count or codelength differs from base, or a case is missing. If that "
                "is intended, add a line `Output-change: <reason>` to the PR "
                "description and re-run this job."
            )

    counts = result["counts"]
    lines.extend(
        [
            "",
            (
                "Thresholds: "
                f"regression >= {RELATIVE_REGRESSION_THRESHOLD:.0%} and >= {ABSOLUTE_REGRESSION_THRESHOLD_SEC:.2f}s, "
                f"severe >= {SEVERE_RELATIVE_REGRESSION_THRESHOLD:.0%} and >= {SEVERE_ABSOLUTE_REGRESSION_THRESHOLD_SEC:.2f}s, "
                f"inconclusive when baseline < {MIN_BASELINE_RUNTIME_SEC:.2f}s or CV > {CV_INCONCLUSIVE_THRESHOLD:.2f}; "
                f"memory regression when peak RSS grows >= {RSS_RELATIVE_REGRESSION_THRESHOLD:.0%} and >= "
                f"{RSS_ABSOLUTE_REGRESSION_THRESHOLD_BYTES / (1024 * 1024):g} MiB."
            ),
            "",
            f"Counts: {counts['timeout']} timeout, {counts['regression']} regression, {counts['manual_review']} manual review, {counts['inconclusive']} inconclusive, {counts['improvement']} improvement.",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare native benchmark reports for PR perf regression detection."
    )
    parser.add_argument(
        "--base",
        type=Path,
        required=True,
        help="Path to the base benchmark JSON report.",
    )
    parser.add_argument(
        "--head",
        type=Path,
        required=True,
        help="Path to the head benchmark JSON report.",
    )
    parser.add_argument(
        "--markdown-out",
        type=Path,
        default=None,
        help="Optional Markdown summary output path.",
    )
    parser.add_argument(
        "--json-out",
        type=Path,
        default=None,
        help="Optional JSON comparison output path.",
    )
    parser.add_argument(
        "--fail-on-regression",
        action="store_true",
        help=(
            "Exit with status 1 when any benchmark case is classified as a regression, "
            "in wall time or peak RSS, or when the head binary timed out on a case."
        ),
    )
    parser.add_argument(
        "--fail-on-output-change",
        action="store_true",
        help=(
            "Exit with status 1 when a case's output differs from base, unless "
            "--pr-description acknowledges it."
        ),
    )
    parser.add_argument(
        "--pr-description",
        type=Path,
        default=None,
        help=(
            "File holding the PR description. A line 'Output-change: <reason>' in it "
            "acknowledges an output change."
        ),
    )
    args = parser.parse_args()

    output_change_ack = (
        output_change_acknowledgement(args.pr_description.read_text(encoding="utf-8"))
        if args.pr_description is not None
        else None
    )
    result = compare_reports(
        load_report(args.base), load_report(args.head), output_change_ack
    )
    markdown = render_markdown(result)

    if args.markdown_out is not None:
        args.markdown_out.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_out.write_text(markdown, encoding="utf-8")

    if args.json_out is not None:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    print(markdown)
    counts = result["counts"]
    if args.fail_on_regression and (counts["regression"] or counts["timeout"]):
        return 1
    if args.fail_on_output_change and counts["manual_review"] and not output_change_ack:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
