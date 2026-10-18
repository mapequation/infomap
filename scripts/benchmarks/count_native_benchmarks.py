"""Count instructions and peak live heap of two native benchmark binaries.

Wall time on a shared runner varies by a few percent between runs of one binary, so
the wall-time gate in compare_native_benchmarks.py can only call changes of about
15% and up. The two counts here are close to deterministic for a fixed input and
seed, so changes of a percent or two show:

- instructions, from Valgrind's cachegrind, which runs the program on a simulated
  CPU that neither machine load nor the runner's hardware reaches;
- peak live heap, from heap_counter.c preloaded into a normal run, which counts
  what the allocator hands out rather than what the kernel maps.

Instructions miss memory-bound effects (a change in data layout can cost wall time
at the same instruction count), so this complements the wall-time gate rather than
replacing it.

Neither count depends on machine load, so the runs go in parallel, one per core.
"""

from __future__ import annotations

import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from run_native_benchmarks import (  # noqa: E402
    benchmark_command,
    generate_multilayer_block_network,
    generate_ring_of_cliques,
    generate_sparse_graph,
    generate_state_block_network,
)

# Reported, not gated: see the perf-pr workflow for why. At or above these the
# summary marks a case and the job prints a warning.
INSTRUCTIONS_RELATIVE_THRESHOLD = 0.01
HEAP_RELATIVE_THRESHOLD = 0.01
# A few allocations more or less are not worth a warning on a case whose peak is
# tens of megabytes.
HEAP_ABSOLUTE_THRESHOLD_BYTES = 64 * 1024

METRICS = ("instructions", "peak_heap_bytes")


def build_count_cases(generated_dir: Path) -> list[dict[str, str | Path]]:
    """Cases of 10k to 20k nodes: big enough that start-up is a small part of the
    count, small enough that each runs in under a minute under cachegrind. They
    cover the two-level and multilevel search, directed flow, and memory and
    multilayer input."""
    sparse_10k = generated_dir / "sparse_10k.net"
    sparse_20k = generated_dir / "sparse_20k.net"
    ring_20k = generated_dir / "ring_of_cliques_20k.net"
    state_block_20k = generated_dir / "state_block_20k_states.net"
    multilayer_5k_x4 = generated_dir / "multilayer_5k_x4.net"
    generate_sparse_graph(sparse_10k, num_nodes=10_000, avg_degree=10, seed=123)
    generate_sparse_graph(sparse_20k, num_nodes=20_000, avg_degree=10, seed=123)
    generate_ring_of_cliques(ring_20k, clique_count=2_000, clique_size=10)
    generate_state_block_network(state_block_20k, 5_000, 4, 50)
    generate_multilayer_block_network(multilayer_5k_x4, 5_000, 4, 50)
    return [
        {"name": "sparse_20k", "path": sparse_20k, "flags": ""},
        {
            "name": "sparse_10k_directed",
            "path": sparse_10k,
            "flags": "--flow-model directed",
        },
        {"name": "ring_of_cliques_20k", "path": ring_20k, "flags": ""},
        {"name": "state_block_20k_states", "path": state_block_20k, "flags": ""},
        {"name": "multilayer_5k_x4", "path": multilayer_5k_x4, "flags": ""},
    ]


def compile_heap_counter(out_dir: Path, compiler: str) -> Path:
    library = out_dir / "heap_counter.so"
    subprocess.run(
        [
            compiler,
            "-O2",
            "-shared",
            "-fPIC",
            "-o",
            str(library),
            str(SCRIPT_DIR / "heap_counter.c"),
        ],
        check=True,
    )
    return library


def cachegrind_command(valgrind: str, out_file: Path, command: list[str]) -> list[str]:
    # Without the cache simulation, cachegrind only counts instructions, which is
    # all that is used and several times faster.
    return [
        valgrind,
        "--tool=cachegrind",
        "--quiet",
        "--cache-sim=no",
        f"--cachegrind-out-file={out_file}",
        *command,
    ]


def parse_cachegrind(text: str) -> int:
    """Total instructions from a cachegrind output file's `summary:` line."""
    events: list[str] = []
    for line in text.splitlines():
        if line.startswith("events:"):
            events = line.split()[1:]
        elif line.startswith("summary:"):
            values = (int(value) for value in line.split()[1:])
            return dict(zip(events, values, strict=True))["Ir"]
    raise ValueError("no summary line in cachegrind output")


def parse_heap_counter(text: str) -> dict[str, int]:
    values = dict(line.split() for line in text.splitlines() if line.strip())
    return {
        "peak_heap_bytes": int(values["peak_heap_bytes"]),
        "allocations": int(values["allocations"]),
    }


def run_sample(
    metric: str,
    command: list[str],
    out_file: Path,
    valgrind: str,
    heap_counter: Path,
    timeout_sec: float | None,
) -> dict[str, object]:
    env = None
    if metric == "instructions":
        command = cachegrind_command(valgrind, out_file, command)
    else:
        env = {
            **os.environ,
            "LD_PRELOAD": str(heap_counter),
            "HEAP_COUNTER_OUT": str(out_file),
        }
    start = time.monotonic()
    completed = subprocess.run(
        command,
        check=True,
        capture_output=True,
        text=True,
        timeout=timeout_sec,
        env=env,
    )
    elapsed_sec = time.monotonic() - start
    output = json.loads(completed.stdout)
    text = out_file.read_text(encoding="utf-8")
    counts = (
        {"instructions": parse_cachegrind(text)}
        if metric == "instructions"
        else parse_heap_counter(text)
    )
    return {
        **counts,
        "elapsed_sec": elapsed_sec,
        "num_top_modules": output["num_top_modules"],
        "num_levels": output["num_levels"],
        "codelength": output["codelength"],
    }


def run_counts(
    binaries: dict[str, Path],
    cases: list[dict[str, str | Path]],
    repeats: int,
    flags: str,
    valgrind: str,
    heap_counter: Path,
    work_dir: Path,
    jobs: int,
    timeout_sec: float | None,
) -> dict[str, dict[str, dict[str, list[dict[str, object]]]]]:
    """Samples as results[case][label][metric], one dict per repeat."""
    tasks = []
    for case in cases:
        case_flags = " ".join(part for part in (flags, str(case["flags"])) if part)
        for label, binary in binaries.items():
            command = benchmark_command(
                binary, str(case["name"]), Path(case["path"]), 1, case_flags
            )
            for metric in METRICS:
                for repeat in range(repeats):
                    out_file = work_dir / f"{label}.{case['name']}.{metric}.{repeat}"
                    tasks.append((str(case["name"]), label, metric, command, out_file))

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [
            pool.submit(
                run_sample,
                metric,
                command,
                out_file,
                valgrind,
                heap_counter,
                timeout_sec,
            )
            for (_, _, metric, command, out_file) in tasks
        ]
        samples = [future.result() for future in futures]

    results: dict[str, dict[str, dict[str, list[dict[str, object]]]]] = {}
    for (name, label, metric, _, _), sample in zip(tasks, samples, strict=True):
        per_label = results.setdefault(name, {}).setdefault(label, {})
        per_label.setdefault(metric, []).append(sample)
    return results


def relative_range(values: list[int]) -> float:
    mean_value = statistics.mean(values)
    return (max(values) - min(values)) / mean_value if mean_value else 0.0


def summarize(
    results: dict[str, dict[str, dict[str, list[dict[str, object]]]]],
) -> list[dict[str, object]]:
    """One row per case: base and head medians, the delta, and the spread of
    repeats, which is zero when a count is deterministic."""
    rows = []
    for name, per_label in results.items():
        row: dict[str, object] = {"name": name}
        for metric in METRICS:
            values = {
                label: [int(sample[metric]) for sample in per_label[label][metric]]
                for label in ("base", "head")
            }
            base_value = statistics.median(values["base"])
            head_value = statistics.median(values["head"])
            row[metric] = {
                "base": base_value,
                "head": head_value,
                "delta": head_value - base_value,
                "delta_pct": (head_value - base_value) / base_value
                if base_value
                else 0.0,
                "base_relative_range": relative_range(values["base"]),
                "head_relative_range": relative_range(values["head"]),
                "max_elapsed_sec": max(
                    float(sample["elapsed_sec"])
                    for label in ("base", "head")
                    for sample in per_label[label][metric]
                ),
            }
        row["status"] = classify(row)
        rows.append(row)
    return rows


def classify(row: dict[str, object]) -> str:
    marks = []
    instructions = row.get("instructions")
    if instructions is not None:
        if instructions["delta_pct"] >= INSTRUCTIONS_RELATIVE_THRESHOLD:
            marks.append("more instructions")
        elif instructions["delta_pct"] <= -INSTRUCTIONS_RELATIVE_THRESHOLD:
            marks.append("fewer instructions")
    heap = row.get("peak_heap_bytes")
    if heap is not None and abs(heap["delta"]) >= HEAP_ABSOLUTE_THRESHOLD_BYTES:
        if heap["delta_pct"] >= HEAP_RELATIVE_THRESHOLD:
            marks.append("more heap")
        elif heap["delta_pct"] <= -HEAP_RELATIVE_THRESHOLD:
            marks.append("less heap")
    return ", ".join(marks) or "ok"


def render_markdown(rows: list[dict[str, object]], repeats: int) -> str:
    lines = [
        "## PR Instruction and Heap Counts",
        "",
        (
            "Instructions counted under cachegrind and peak live heap by a preloaded "
            "allocator counter, both single-threaded. Reported only; this does not "
            "fail the job."
        ),
        "",
        "| Case | Status | Base instructions | Head instructions | Delta (%) | Base peak heap (MiB) | Head peak heap (MiB) | Delta (%) |",
        "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for row in rows:
        instructions = row["instructions"]
        heap = row["peak_heap_bytes"]
        cells = [
            str(row["name"]),
            str(row["status"]),
            f"{instructions['base']:,.0f}",
            f"{instructions['head']:,.0f}",
            f"{instructions['delta_pct'] * 100:+.3f}",
            f"{heap['base'] / (1024 * 1024):.3f}",
            f"{heap['head'] / (1024 * 1024):.3f}",
            f"{heap['delta_pct'] * 100:+.3f}",
        ]
        lines.append("| " + " | ".join(cells) + " |")
    lines += [
        "",
        (
            f"Marked when instructions change by >= {INSTRUCTIONS_RELATIVE_THRESHOLD:.0%}, "
            f"or peak live heap by >= {HEAP_RELATIVE_THRESHOLD:.0%} and >= "
            f"{HEAP_ABSOLUTE_THRESHOLD_BYTES // 1024} KiB."
        ),
        "",
    ]
    if repeats > 1:
        lines += [
            f"Spread over {repeats} runs of each binary, (max - min) / mean:",
            "",
            "| Case | Base instructions | Head instructions | Base heap | Head heap | Slowest cachegrind run (s) |",
            "| --- | ---: | ---: | ---: | ---: | ---: |",
        ]
        for row in rows:
            instructions = row["instructions"]
            heap = row["peak_heap_bytes"]
            lines.append(
                f"| {row['name']} "
                f"| {instructions['base_relative_range']:.2e} "
                f"| {instructions['head_relative_range']:.2e} "
                f"| {heap['base_relative_range']:.2e} "
                f"| {heap['head_relative_range']:.2e} "
                f"| {instructions['max_elapsed_sec']:.1f} |"
            )
        lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Count instructions and peak live heap of a base and a head native "
            "benchmark binary."
        )
    )
    parser.add_argument("--base-binary", type=Path, required=True)
    parser.add_argument("--head-binary", type=Path, required=True)
    parser.add_argument("--json-out", type=Path, default=None)
    parser.add_argument("--markdown-out", type=Path, default=None)
    parser.add_argument(
        "--flags",
        default="--silent --no-file-output --num-trials 1 --seed 123 --num-threads 1",
        help="Infomap flags forwarded to the native benchmark executable.",
    )
    parser.add_argument(
        "--repeats",
        type=int,
        default=1,
        help="Runs per binary, case and count. More than one measures the spread.",
    )
    parser.add_argument("--valgrind", default="valgrind")
    parser.add_argument(
        "--compiler",
        default=os.environ.get("CC", "cc"),
        help="C compiler for heap_counter.c.",
    )
    parser.add_argument(
        "--jobs", type=int, default=os.cpu_count() or 1, help="Parallel runs."
    )
    parser.add_argument("--generated-dir", type=Path, default=None)
    parser.add_argument(
        "--timeout-sec",
        type=float,
        default=None,
        help="Kill a single run that takes longer than this.",
    )
    args = parser.parse_args()

    if args.repeats < 1:
        parser.error("--repeats must be at least 1")
    for binary in (args.base_binary, args.head_binary):
        if not binary.is_file():
            parser.error(f"benchmark binary not found: {binary}")

    # Resolved, since Path("./bench") prints as "bench", which subprocess would look
    # up on PATH instead of in the current directory.
    binaries = {"base": args.base_binary.resolve(), "head": args.head_binary.resolve()}
    with tempfile.TemporaryDirectory() as temp_dir:
        work_dir = Path(temp_dir)
        generated_dir = args.generated_dir or work_dir
        generated_dir.mkdir(parents=True, exist_ok=True)
        heap_counter = compile_heap_counter(work_dir, args.compiler)
        results = run_counts(
            binaries,
            build_count_cases(generated_dir),
            args.repeats,
            args.flags,
            args.valgrind,
            heap_counter,
            work_dir,
            max(1, args.jobs),
            args.timeout_sec,
        )

    rows = summarize(results)
    markdown = render_markdown(rows, args.repeats)
    if args.markdown_out is not None:
        args.markdown_out.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_out.write_text(markdown, encoding="utf-8")
    if args.json_out is not None:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        report = {
            "flags": args.flags,
            "repeats": args.repeats,
            "thresholds": {
                "instructions_relative": INSTRUCTIONS_RELATIVE_THRESHOLD,
                "heap_relative": HEAP_RELATIVE_THRESHOLD,
                "heap_absolute_bytes": HEAP_ABSOLUTE_THRESHOLD_BYTES,
            },
            "cases": rows,
            "samples": results,
        }
        args.json_out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(markdown)
    for row in rows:
        if row["status"] != "ok":
            print(f"::warning::{row['name']}: {row['status']} than base")
    return 0


if __name__ == "__main__":
    sys.exit(main())
