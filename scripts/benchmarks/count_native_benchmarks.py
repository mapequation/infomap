"""Count instructions and peak live heap of two native benchmark binaries.

Wall time on a shared runner varies by a few percent between runs of one binary, so
the wall-time gate in compare_native_benchmarks.py can only call changes of about
15% and up. Both quantities here are close to deterministic for a fixed input and
seed: Valgrind runs the program on one simulated CPU, so neither machine load nor
the runner's hardware reaches them. That makes changes of a percent or two visible.

Instructions miss memory-bound effects (a change in data layout can cost wall time
at the same instruction count), so this complements the wall-time gate rather than
replacing it.

Each binary runs once per case under each tool. Runs are independent of machine
load, so they run in parallel, one per core.
"""

from __future__ import annotations

import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from run_native_benchmarks import (
    benchmark_command,
    generate_multilayer_block_network,
    generate_ring_of_cliques,
    generate_sparse_graph,
    generate_state_block_network,
)

# Reported, not gated: see the perf-pr workflow for why. Above these the summary
# marks a case and the job prints a warning.
INSTRUCTIONS_RELATIVE_THRESHOLD = 0.01
HEAP_RELATIVE_THRESHOLD = 0.01
# A few allocations more or less are not worth a warning on a case whose peak is
# tens of megabytes.
HEAP_ABSOLUTE_THRESHOLD_BYTES = 64 * 1024

TOOLS = ("cachegrind", "dhat")


def build_count_cases(generated_dir: Path) -> list[dict[str, str | Path]]:
    """Cases of 10k to 50k nodes: big enough that start-up is a small part of the
    count, small enough for Valgrind. They cover the two-level and multilevel
    search, directed flow, and memory and multilayer input."""
    sparse_20k = generated_dir / "sparse_20k.net"
    ring_20k = generated_dir / "ring_of_cliques_20k.net"
    state_block_40k = generated_dir / "state_block_40k_states.net"
    multilayer_10k_x4 = generated_dir / "multilayer_10k_x4.net"
    generate_sparse_graph(sparse_20k, num_nodes=20_000, avg_degree=10, seed=123)
    generate_ring_of_cliques(ring_20k, clique_count=2_000, clique_size=10)
    generate_state_block_network(state_block_40k, 10_000, 4, 50)
    generate_multilayer_block_network(multilayer_10k_x4, 10_000, 4, 50)
    return [
        {"name": "sparse_20k", "path": sparse_20k, "flags": ""},
        {
            "name": "sparse_20k_directed",
            "path": sparse_20k,
            "flags": "--flow-model directed",
        },
        {"name": "ring_of_cliques_20k", "path": ring_20k, "flags": ""},
        {"name": "state_block_40k_states", "path": state_block_40k, "flags": ""},
        {"name": "multilayer_10k_x4", "path": multilayer_10k_x4, "flags": ""},
    ]


def valgrind_command(
    valgrind: str, tool: str, out_file: Path, command: list[str]
) -> list[str]:
    if tool == "cachegrind":
        # Without the cache simulation, cachegrind only counts instructions, which is
        # all that is used and several times faster.
        options = ["--cache-sim=no", f"--cachegrind-out-file={out_file}"]
    elif tool == "dhat":
        options = [f"--dhat-out-file={out_file}"]
    else:
        raise ValueError(f"unknown tool: {tool}")
    return [valgrind, f"--tool={tool}", "--quiet", *options, *command]


def parse_cachegrind(text: str) -> dict[str, int]:
    """Total instructions from a cachegrind output file's `summary:` line."""
    events: list[str] = []
    for line in text.splitlines():
        if line.startswith("events:"):
            events = line.split()[1:]
        elif line.startswith("summary:"):
            totals = dict(
                zip(events, (int(value) for value in line.split()[1:]), strict=True)
            )
            return {"instructions": totals["Ir"]}
    raise ValueError("no summary line in cachegrind output")


def parse_dhat(text: str) -> dict[str, int]:
    """Instructions and peak live heap from a DHAT JSON output file.

    DHAT measures time in instructions executed (`tu` is "instrs"), so `te`, the
    time at exit, is the instruction count. Each program point's `gb` is the bytes
    it held live at the global peak; their sum is the peak live heap.
    """
    data = json.loads(text)
    if data.get("tu") != "instrs":
        raise ValueError(f"DHAT time unit is {data.get('tu')!r}, not instructions")
    return {
        "instructions": int(data["te"]),
        "peak_heap_bytes": sum(int(point["gb"]) for point in data["pps"]),
        "peak_heap_blocks": sum(int(point["gbk"]) for point in data["pps"]),
        "total_heap_bytes": sum(int(point["tb"]) for point in data["pps"]),
        "total_heap_blocks": sum(int(point["tbk"]) for point in data["pps"]),
    }


PARSERS = {"cachegrind": parse_cachegrind, "dhat": parse_dhat}


def measure(
    valgrind: str,
    tool: str,
    command: list[str],
    out_file: Path,
    timeout_sec: float | None,
) -> dict[str, object]:
    full_command = valgrind_command(valgrind, tool, out_file, command)
    completed = subprocess.run(
        full_command, check=True, capture_output=True, text=True, timeout=timeout_sec
    )
    output = json.loads(completed.stdout)
    counts = PARSERS[tool](out_file.read_text(encoding="utf-8"))
    return {
        **counts,
        # The benchmark's own timer, here only to show what Valgrind costs.
        "valgrind_total_sec": float(output["total_sec"]),
        "num_top_modules": output["num_top_modules"],
        "codelength": output["codelength"],
    }


def spread(values: list[int]) -> dict[str, float]:
    mean_value = statistics.mean(values)
    return {
        "min": min(values),
        "max": max(values),
        "relative_range": (max(values) - min(values)) / mean_value
        if mean_value
        else 0.0,
    }


def run_counts(
    binaries: dict[str, Path],
    cases: list[dict[str, str | Path]],
    tools: list[str],
    repeats: int,
    flags: str,
    valgrind: str,
    work_dir: Path,
    jobs: int,
    timeout_sec: float | None,
) -> dict[str, object]:
    tasks = []
    for case in cases:
        case_flags = " ".join(part for part in (flags, str(case["flags"])) if part)
        for label, binary in binaries.items():
            command = benchmark_command(
                binary, str(case["name"]), Path(case["path"]), 1, case_flags
            )
            for tool in tools:
                for repeat in range(repeats):
                    out_file = work_dir / f"{label}.{case['name']}.{tool}.{repeat}.out"
                    tasks.append((str(case["name"]), label, tool, command, out_file))

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [
            pool.submit(measure, valgrind, tool, command, out_file, timeout_sec)
            for (_, _, tool, command, out_file) in tasks
        ]
        samples = [future.result() for future in futures]

    results: dict[str, dict[str, dict[str, list[dict[str, object]]]]] = {}
    for (name, label, tool, _, _), sample in zip(tasks, samples, strict=True):
        results.setdefault(name, {}).setdefault(label, {}).setdefault(tool, []).append(
            sample
        )
    return results


def summarize(
    results: dict[str, dict[str, dict[str, list[dict[str, object]]]]],
) -> list[dict[str, object]]:
    """One row per case: base and head medians, deltas, and the spread of repeats."""
    rows = []
    for name, per_label in results.items():
        row: dict[str, object] = {"name": name}
        for metric, tool in (
            ("instructions", "cachegrind"),
            ("dhat_instructions", "dhat"),
            ("peak_heap_bytes", "dhat"),
        ):
            source_key = "instructions" if metric == "dhat_instructions" else metric
            if any(tool not in per_label[label] for label in ("base", "head")):
                continue
            values = {
                label: [int(s[source_key]) for s in per_label[label][tool]]
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
                "base_spread": spread(values["base"]),
                "head_spread": spread(values["head"]),
            }
        row["valgrind_sec"] = {
            tool: statistics.median(
                float(s["valgrind_total_sec"])
                for label in per_label
                for s in per_label[label].get(tool, [])
            )
            for tool in TOOLS
            if all(tool in per_label[label] for label in per_label)
        }
        row["status"] = classify(row)
        rows.append(row)
    return rows


def classify(row: dict[str, object]) -> str:
    flags = []
    instructions = row.get("instructions") or row.get("dhat_instructions")
    if instructions is not None:
        if instructions["delta_pct"] >= INSTRUCTIONS_RELATIVE_THRESHOLD:
            flags.append("more instructions")
        elif instructions["delta_pct"] <= -INSTRUCTIONS_RELATIVE_THRESHOLD:
            flags.append("fewer instructions")
    heap = row.get("peak_heap_bytes")
    if heap is not None and abs(heap["delta"]) >= HEAP_ABSOLUTE_THRESHOLD_BYTES:
        if heap["delta_pct"] >= HEAP_RELATIVE_THRESHOLD:
            flags.append("more heap")
        elif heap["delta_pct"] <= -HEAP_RELATIVE_THRESHOLD:
            flags.append("less heap")
    return ", ".join(flags) or "ok"


def render_markdown(rows: list[dict[str, object]], repeats: int) -> str:
    lines = [
        "## PR Instruction and Heap Counts",
        "",
        (
            "Counted under Valgrind, single-threaded. Reported only; this does not "
            "fail the job."
        ),
        "",
        "| Case | Status | Base instructions | Head instructions | Delta (%) | Base peak heap (MiB) | Head peak heap (MiB) | Delta (%) |",
        "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for row in rows:
        instructions = row.get("instructions") or row.get("dhat_instructions")
        heap = row.get("peak_heap_bytes")
        cells = [str(row["name"]), str(row["status"])]
        if instructions is None:
            cells += ["n/a"] * 3
        else:
            cells += [
                f"{instructions['base']:,.0f}",
                f"{instructions['head']:,.0f}",
                f"{instructions['delta_pct'] * 100:+.3f}",
            ]
        if heap is None:
            cells += ["n/a"] * 3
        else:
            cells += [
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
            f"Spread over {repeats} repeats of each binary (max - min, relative):",
            "",
            "| Case | Metric | Base | Head | Valgrind time (s) |",
            "| --- | --- | ---: | ---: | --- |",
        ]
        for row in rows:
            for metric in ("instructions", "dhat_instructions", "peak_heap_bytes"):
                if metric not in row:
                    continue
                lines.append(
                    f"| {row['name']} | {metric} | "
                    f"{row[metric]['base_spread']['relative_range'] * 100:.4f}% | "
                    f"{row[metric]['head_spread']['relative_range'] * 100:.4f}% | "
                    + ", ".join(
                        f"{tool} {sec:.1f}" for tool, sec in row["valgrind_sec"].items()
                    )
                    + " |"
                )
        lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Count instructions and peak live heap of a base and a head native "
            "benchmark binary under Valgrind."
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
        "--tools",
        default="dhat",
        help=f"Comma-separated Valgrind tools to run, from {', '.join(TOOLS)}.",
    )
    parser.add_argument(
        "--repeats",
        type=int,
        default=1,
        help="Runs per binary, case and tool. More than one measures the spread.",
    )
    parser.add_argument("--valgrind", default="valgrind")
    parser.add_argument(
        "--jobs", type=int, default=os.cpu_count() or 1, help="Parallel runs."
    )
    parser.add_argument("--generated-dir", type=Path, default=None)
    parser.add_argument(
        "--timeout-sec",
        type=float,
        default=None,
        help="Kill a single Valgrind run that takes longer than this.",
    )
    args = parser.parse_args()

    tools = [tool.strip() for tool in args.tools.split(",") if tool.strip()]
    unknown = sorted(set(tools) - set(TOOLS))
    if unknown or not tools:
        parser.error(f"--tools must name some of {', '.join(TOOLS)}")
    if args.repeats < 1:
        parser.error("--repeats must be at least 1")
    for binary in (args.base_binary, args.head_binary):
        if not binary.is_file():
            parser.error(f"benchmark binary not found: {binary}")

    binaries = {"base": args.base_binary, "head": args.head_binary}
    with tempfile.TemporaryDirectory() as temp_dir:
        work_dir = Path(temp_dir)
        generated_dir = args.generated_dir or work_dir
        generated_dir.mkdir(parents=True, exist_ok=True)
        cases = build_count_cases(generated_dir)
        results = run_counts(
            binaries,
            cases,
            tools,
            args.repeats,
            args.flags,
            args.valgrind,
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
        args.json_out.write_text(
            json.dumps(
                {
                    "flags": args.flags,
                    "tools": tools,
                    "repeats": args.repeats,
                    "thresholds": {
                        "instructions_relative": INSTRUCTIONS_RELATIVE_THRESHOLD,
                        "heap_relative": HEAP_RELATIVE_THRESHOLD,
                        "heap_absolute_bytes": HEAP_ABSOLUTE_THRESHOLD_BYTES,
                    },
                    "cases": rows,
                    "samples": results,
                },
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
    print(markdown)
    for row in rows:
        if row["status"] != "ok":
            print(f"::warning::{row['name']}: {row['status']} than base")
    return 0


if __name__ == "__main__":
    sys.exit(main())
