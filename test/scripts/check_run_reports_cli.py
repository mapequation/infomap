#!/usr/bin/env python3

import json
import subprocess
import sys
import tempfile
from pathlib import Path

from schema_validation import validate_json_schema


def run(infomap_bin: str, *args: str, cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [infomap_bin, *args],
        check=False,
        text=True,
        capture_output=True,
        cwd=str(cwd),
    )


def make_workdir(path: Path) -> Path:
    path.mkdir(parents=True, exist_ok=True)
    (path / "network.net").write_text(
        "*Vertices 6\n"
        '1 "one"\n'
        '2 "two"\n'
        '3 "three"\n'
        '4 "four"\n'
        '5 "five"\n'
        '6 "six"\n'
        "*Edges\n"
        "1 2 1\n"
        "2 3 1\n"
        "3 1 1\n"
        "4 5 1\n"
        "5 6 1\n"
        "6 4 1\n",
        encoding="utf-8",
    )
    return path


def load_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def test_summary_and_timing_reports(infomap_bin: str, work: Path) -> None:
    make_workdir(work)

    result = run(
        infomap_bin,
        "network.net",
        "out",
        "--silent",
        "--seed",
        "7",
        "--num-trials",
        "2",
        "--summary-json",
        "summary.json",
        "--timing-json",
        "timing.json",
        cwd=work,
    )

    assert result.returncode == 0, result.stderr
    summary = load_json(work / "summary.json")
    timing = load_json(work / "timing.json")
    validate_json_schema(summary, "summary-report.schema.json")
    validate_json_schema(timing, "timing-report.schema.json")

    assert summary["trials"] == 2
    assert len(summary["trial_codelengths"]) == 2
    assert len(summary["trial_top_modules"]) == 2
    assert "memory" not in timing
    assert len(timing["trials"]) == 2


def test_timing_memory_report(infomap_bin: str, work: Path) -> None:
    make_workdir(work)

    result = run(
        infomap_bin,
        "network.net",
        "out",
        "--silent",
        "--timing-json",
        "timing-memory.json",
        "--memory-report",
        cwd=work,
    )

    assert result.returncode == 0, result.stderr
    timing = load_json(work / "timing-memory.json")
    validate_json_schema(timing, "timing-report.schema.json")

    assert "memory" in timing
    assert "rss_peak_mb" in timing["memory"]


def main(argv: list[str]) -> int:
    infomap_bin = argv[1]
    tests = [
        test_summary_and_timing_reports,
        test_timing_memory_report,
    ]

    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        for test in tests:
            try:
                test(infomap_bin, tmp_path / test.__name__)
            except Exception as exc:
                failures += 1
                print(f"{test.__name__}: {exc}", file=sys.stderr)

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
