from __future__ import annotations

import argparse
import json
import statistics
import time
from pathlib import Path

from infomap import Infomap


def benchmark_case(name: str, network_path: Path, repeats: int) -> dict[str, object]:
    durations = []
    final_modules = None
    final_levels = None
    final_codelength = None

    for _ in range(repeats):
        im = Infomap(seed=123, num_trials=1, silent=True)
        start = time.perf_counter()
        im.read_file(str(network_path))
        im.run()
        durations.append(time.perf_counter() - start)
        final_modules = im.num_top_modules
        final_levels = im.num_levels
        final_codelength = im.codelength

    return {
        "name": name,
        "path": str(network_path),
        "repeats": repeats,
        "durations_sec": durations,
        "median_sec": statistics.median(durations),
        "min_sec": min(durations),
        "max_sec": max(durations),
        "num_top_modules": final_modules,
        "num_levels": final_levels,
        "codelength": final_codelength,
    }


def render_markdown(results: list[dict[str, object]]) -> str:
    lines = [
        "## Python Benchmark Summary",
        "",
        "| Case | Median (s) | Min (s) | Max (s) | Top Modules | Levels |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ]

    for result in results:
        lines.append(
            "| {name} | {median_sec:.6f} | {min_sec:.6f} | {max_sec:.6f} | {num_top_modules} | {num_levels} |".format(
                **result
            )
        )

    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Run a small fixed Python benchmark suite for Infomap.")
    parser.add_argument("--output", type=Path, required=True, help="Path to write the JSON benchmark report.")
    parser.add_argument(
        "--summary",
        type=Path,
        default=None,
        help="Optional path to write a Markdown summary suitable for a GitHub job summary.",
    )
    parser.add_argument("--repeats", type=int, default=5, help="Number of runs per benchmark case.")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    cases = [
        ("twotriangles", repo_root / "examples" / "networks" / "twotriangles.net"),
        ("ninetriangles", repo_root / "examples" / "networks" / "ninetriangles.net"),
        ("states", repo_root / "examples" / "networks" / "states.net"),
    ]

    results = [benchmark_case(name, path, args.repeats) for name, path in cases]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"benchmarks": results}, indent=2) + "\n")

    markdown = render_markdown(results)
    if args.summary is not None:
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(markdown)

    print(markdown)


if __name__ == "__main__":
    main()
