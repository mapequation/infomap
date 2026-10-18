from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest

pytestmark = pytest.mark.fast


def _load_count_module():
    current = Path(__file__).resolve()
    relative_target = Path("scripts") / "benchmarks" / "count_native_benchmarks.py"
    for parent in current.parents:
        candidate = parent / relative_target
        if candidate.exists():
            spec = importlib.util.spec_from_file_location(
                "count_native_benchmarks", candidate
            )
            assert spec is not None
            assert spec.loader is not None
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            return module
    raise FileNotFoundError(f"Could not locate {relative_target} from {current}")


def test_parse_cachegrind_reads_instructions_from_the_summary_line():
    module = _load_count_module()
    text = (
        "# cachegrind output\n"
        "cmd: ./infomap_native_benchmark --input x.net\n"
        "events: Ir\n"
        "fl=src/core/InfomapBase.cpp\n"
        "fn=run\n"
        "12 3\n"
        "summary: 123456789\n"
    )
    assert module.parse_cachegrind(text) == 123456789


def test_parse_cachegrind_picks_ir_among_several_events():
    module = _load_count_module()
    assert module.parse_cachegrind("events: Ir I1mr ILmr\nsummary: 1000 7 3\n") == 1000


def test_parse_cachegrind_rejects_output_without_a_summary():
    module = _load_count_module()
    with pytest.raises(ValueError):
        module.parse_cachegrind("events: Ir\n")


def test_parse_heap_counter_reads_what_heap_counter_c_writes():
    module = _load_count_module()
    text = "peak_heap_bytes 52428800\nallocations 1234\n"
    assert module.parse_heap_counter(text) == {
        "peak_heap_bytes": 52428800,
        "allocations": 1234,
    }


def _metric(delta_pct: float, delta: float = 0.0) -> dict[str, float]:
    return {"delta_pct": delta_pct, "delta": delta}


@pytest.mark.parametrize(
    ("instructions_pct", "expected"),
    [
        (0.0, "ok"),
        (0.0099, "ok"),
        (0.01, "more instructions"),
        (-0.0099, "ok"),
        (-0.01, "fewer instructions"),
    ],
)
def test_classify_marks_instruction_changes_at_the_threshold(
    instructions_pct, expected
):
    module = _load_count_module()
    assert module.classify({"instructions": _metric(instructions_pct)}) == expected


def test_classify_needs_both_heap_thresholds():
    module = _load_count_module()
    kib = 1024

    def heap(delta_pct: float, delta: float) -> str:
        return module.classify({"peak_heap_bytes": _metric(delta_pct, delta)})

    # A large relative change of a few bytes, and a small relative change of a lot.
    assert heap(0.5, 1 * kib) == "ok"
    assert heap(0.005, 1024 * kib) == "ok"
    assert heap(0.02, 128 * kib) == "more heap"
    assert heap(-0.02, -128 * kib) == "less heap"


def test_summarize_compares_medians_and_reports_the_spread():
    module = _load_count_module()

    def samples(metric: str, *values: int) -> list[dict[str, object]]:
        return [{metric: value, "elapsed_sec": 2.0} for value in values]

    mib = 1024 * 1024
    results = {
        "sparse": {
            "base": {
                "instructions": samples("instructions", 1000, 1000, 1002),
                "peak_heap_bytes": samples(
                    "peak_heap_bytes", 10 * mib, 10 * mib, 10 * mib
                ),
            },
            "head": {
                "instructions": samples("instructions", 1030, 1030, 1030),
                "peak_heap_bytes": samples(
                    "peak_heap_bytes", 10 * mib, 10 * mib, 10 * mib
                ),
            },
        }
    }
    (row,) = module.summarize(results)
    assert row["instructions"]["base"] == 1000
    assert row["instructions"]["head"] == 1030
    assert row["instructions"]["delta_pct"] == pytest.approx(0.03)
    assert row["instructions"]["base_relative_range"] == pytest.approx(2 / (3002 / 3))
    assert row["instructions"]["head_relative_range"] == 0.0
    assert row["peak_heap_bytes"]["delta"] == 0
    assert row["status"] == "more instructions"


def test_cachegrind_command_counts_instructions_only(tmp_path: Path):
    module = _load_count_module()
    out_file = tmp_path / "out"
    command = ["./bench", "--input", "x.net"]
    full = module.cachegrind_command("valgrind", out_file, command)
    assert full[:2] == ["valgrind", "--tool=cachegrind"]
    assert "--cache-sim=no" in full
    assert f"--cachegrind-out-file={out_file}" in full
    assert full[-3:] == command
