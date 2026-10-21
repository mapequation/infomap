"""Correctness sweep over the pinned benchmark networks (#1256).

    python scripts/benchmarks/run_correctness_sweep.py --binary build/bench-correctness/Infomap

Runs a debug build of Infomap (compiled with ``checkInvariants()``) on every
network of the columnar benchmark set with its benchmark flags, multilevel and
``-2``, for a few seeds, each under a time limit. Every returned tree is then

1. re-scored by the same binary with ``--no-infomap --cluster-data``, and
2. where the network has at most ``--reference-max-links`` links and the
   ``infomap`` Python package is importable, scored by
   ``test/python/reference_codelength.py`` with the flow the package reports
   for that tree.

A finding is an ``InternalError`` (exit code 7), any other failed exit, or a
codelength that differs from the reported one. Timeouts are listed but are not
findings. The exit status is 1 if there is a finding.

The networks are not in this repository. Fetch them into ``--networks`` (default
``networks/columnar-benchmark``) with ``columnar_wip/fetch-benchmark-networks.py``
from the ``columnar-hierarchical-core`` branch; it needs the private
``mapequation-networks`` package and checks every file against its pinned sha256.
Pass ``--pins`` with that branch's ``benchmark-networks.sha256`` to check them here.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import shlex
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OM = [
    f"om/overlapping-memory-n256-om{om}-nc64-E{e}-mu0.1-seed1"
    for om in range(2, 9)
    for e in (50000, 100000)
]
# Same tolerance as the codelength invariants: far above rounding, far below a real error.
REL_TOL = 1e-9
ABS_TOL = 1e-10


@dataclass(frozen=True)
class Row:
    network: str
    flags: str = ""
    # A scoring run of a given partition: no search, so one cell instead of modes x seeds.
    score_only: bool = False
    # The largest networks run one seed only; a debug run there takes minutes.
    max_seeds: int | None = None

    @property
    def label(self) -> str:
        name = Path(self.network).stem.removeprefix("overlapping-memory-n256-")
        flags = re.sub(r"-c \S+", "-c planted.clu", self.flags).replace("{N}/", "")
        return f"{name} {flags}".strip()


def rows() -> list[Row]:
    """The run flags of ``columnar_wip/benchmark-networks.md``."""
    big = {"max_seeds": 1}
    base = [
        Row("ninetriangles.net"),
        Row("jazz.txt"),
        Row("netscience.txt"),
        Row("powergrid.txt"),
        Row("polblogs.txt", "-d"),
        Row("word_assoc.txt", "-d"),
        Row("web-NotreDame.txt", "-d", **big),
        Row("lazega.net", "--meta-data {N}/lazega.meta"),
        Row("air30k.net"),
    ]
    out = list(base)
    out += [
        Row("multilayer.net"),
        Row("malaria.net"),
        Row("air30k.net", "-d --regularized"),
        Row("air30k.net", "--meta-data {N}/air30k_usstate.meta"),
        Row("word_assoc.txt", "-d --preferred-number-of-modules 25"),
        Row("wikispeedia_states.net", "-d"),
    ]
    # Recorded teleportation (#1042): every network above but the multilayer ones.
    out += [
        Row(r.network, f"{r.flags} --regularized".strip(), max_seeds=r.max_seeds)
        for r in base
    ]
    out += [
        Row(r.network, f"{r.flags} --recorded-teleportation", max_seeds=r.max_seeds)
        for r in base
        if "-d" in r.flags.split()
    ]
    for om in OM:
        for extra in ("", " --regularized"):
            out.append(Row(f"{om}.net", f"-d{extra}"))
            out.append(
                Row(f"{om}.net", f"-2d --no-infomap -c {{N}}/{om}.clu{extra}", True)
            )
    return out


@dataclass
class Cell:
    row: Row
    two_level: bool
    seed: int

    @property
    def name(self) -> str:
        mode = "two-level" if self.two_level else "multilevel"
        if self.row.score_only:
            mode = "planted"
        return f"{self.row.label} | {mode} | seed {self.seed}"

    @property
    def slug(self) -> str:
        return hashlib.sha1(self.name.encode()).hexdigest()[:12]


def flags_of(cell: Cell, networks: Path, trials: int) -> list[str]:
    flags = shlex.split(cell.row.flags.replace("{N}", str(networks)))
    if cell.row.score_only:
        return flags
    if cell.two_level:
        flags.append("-2")
    return [*flags, "--seed", str(cell.seed), "-N", str(trials)]


def scoring_flags(flags: list[str]) -> list[str]:
    """The run's flags without the search and the partition it was given."""
    out, skip = [], False
    for flag in flags:
        if skip:
            skip = False
        elif flag in ("--seed", "-N", "-c", "--cluster-data"):
            skip = True
        elif flag != "--no-infomap":
            out.append(flag)
    return out


def summary(binary: Path, network: Path, flags: list[str], out: Path, timeout: float):
    out.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(binary),
        str(network),
        str(out),
        *flags,
        "--out-name",
        "run",
        "-o",
        "tree,states",
        "--silent",
        "--summary-json",
        str(out / "summary.json"),
    ]
    start = time.monotonic()
    try:
        proc = subprocess.run(
            cmd, check=False, capture_output=True, text=True, timeout=timeout
        )
    except subprocess.TimeoutExpired:
        return {"status": "timeout", "seconds": timeout, "cmd": shlex.join(cmd)}
    result = {
        "status": "ok" if proc.returncode == 0 else f"exit {proc.returncode}",
        "seconds": round(time.monotonic() - start, 2),
        "cmd": shlex.join(cmd),
    }
    if proc.returncode != 0:
        result["stderr"] = proc.stderr.strip()[-2000:]
        return result
    data = json.loads((out / "summary.json").read_text())
    result["codelength"] = data["codelength"]
    states = out / "run_states.tree"
    result["tree"] = str(states if states.exists() else out / "run.tree")
    return result


def close(a: float, b: float) -> bool:
    return math.isclose(a, b, rel_tol=REL_TOL, abs_tol=ABS_TOL)


_reference_lock = threading.Lock()


def reference(network: Path, flags: list[str], tree: str, max_links: int):
    """Score ``tree`` with reference_codelength.py and the package's flow."""
    try:
        import infomap
        from reference_codelength import Leaf, Partition, codelength
    except ImportError as e:
        return {"status": f"skipped ({e})"}
    args = shlex.join([*scoring_flags(flags), "--no-infomap", "-c", tree, "--silent"])
    with _reference_lock:  # one engine at a time in this process
        result = infomap.run(str(network), args=args)
        links = list(result.links("flow"))
        if len(links) > max_links:
            return {"status": f"skipped ({len(links)} links)"}
        leaves = {
            node.state_id: Leaf(
                path=tuple(node.path),
                flow=node.flow,
                physical_id=node.node_id,
                teleport_flow=node.data.teleportFlow,
                teleport_weight=node.data.teleportWeight,
            )
            for node in result.tree(states=True)
            if node.is_leaf
        }
        engine = result.codelength
    options = {}
    if "--meta-data" in flags:
        meta_file = Path(flags[flags.index("--meta-data") + 1])
        meta = {}
        for line in meta_file.read_text().splitlines():
            parts = line.split("#", 1)[0].split()
            if len(parts) >= 2:
                meta[int(parts[0])] = int(parts[1])
        for node_id, leaf in leaves.items():
            leaf.meta = meta.get(node_id, -1)
        options["meta_rate"] = 1.0
    if "--preferred-number-of-modules" in flags:
        options["preferred_num_modules"] = int(
            flags[flags.index("--preferred-number-of-modules") + 1]
        )
    # The flow model the run used, from the tree header: multilayer input with
    # intra-layer links only is run directed whatever the flags say.
    directed = True
    with open(tree) as f:
        for line in f:
            if not line.startswith("#"):
                break
            if line.startswith("# flow model "):
                directed = line.split()[-1] != "undirected"
    partition = Partition(
        leaves=leaves,
        links=links,
        undirected_links=not directed,
        # With metadata the engine scores the state nodes without the physical-node
        # codebook, and warns about it: the two objectives do not compose (#1012).
        memory=result.have_memory and "--meta-data" not in flags,
        recorded_teleportation=any(
            f in ("--regularized", "--recorded-teleportation", "-e") for f in flags
        ),
        **options,
    )
    return {
        "status": "ok",
        "codelength": codelength(partition).total,
        "engine": engine,
    }


def run_cell(cell: Cell, args) -> dict:
    network = args.networks / cell.row.network
    flags = flags_of(cell, args.networks, args.trials)
    out = args.out / cell.slug
    record = {"cell": cell.name, "dir": str(out)}
    record["run"] = run = summary(
        args.binary, network, flags, out / "run", args.timeout
    )
    if run["status"] != "ok":
        return record
    scoring = [*scoring_flags(flags), "--no-infomap", "-c", run["tree"]]
    record["rescore"] = rescore = summary(
        args.binary, network, scoring, out / "rescore", args.timeout
    )
    findings = []
    if rescore["status"] != "ok":
        findings.append(f"re-score {rescore['status']}")
    elif not close(rescore["codelength"], run["codelength"]):
        findings.append(
            f"re-score {rescore['codelength']!r} != reported {run['codelength']!r}"
        )
    if args.reference_max_links > 0:
        record["reference"] = ref = reference(
            network, flags, run["tree"], args.reference_max_links
        )
        if ref["status"] == "ok" and not close(ref["codelength"], run["codelength"]):
            findings.append(
                f"reference {ref['codelength']!r} != reported {run['codelength']!r}"
                f" (package re-score {ref['engine']!r})"
            )
    record["findings"] = findings
    return record


def check_pins(networks: Path, pins: Path) -> None:
    bad = []
    for line in pins.read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        path = networks / name
        if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            bad.append(name)
    if bad:
        sys.exit(f"{len(bad)} network file(s) missing or not matching {pins}: {bad}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument(
        "--networks", type=Path, default=REPO / "networks" / "columnar-benchmark"
    )
    parser.add_argument("--pins", type=Path, help="benchmark-networks.sha256")
    parser.add_argument(
        "--out", type=Path, default=REPO / "build" / "bench-correctness" / "runs"
    )
    parser.add_argument("--seeds", type=int, nargs="+", default=[1, 2, 3])
    parser.add_argument("--trials", type=int, default=2, help="-N per run")
    parser.add_argument("--timeout", type=float, default=1800, help="seconds per run")
    parser.add_argument("--jobs", type=int, default=3)
    parser.add_argument("--reference-max-links", type=int, default=2_000_000)
    parser.add_argument("--filter", default="", help="regex on the cell name")
    args = parser.parse_args()
    args.networks = args.networks.resolve()
    args.binary = args.binary.resolve()
    if args.pins:
        check_pins(args.networks, args.pins)
    sys.path.insert(0, str(REPO / "test" / "python"))

    cells = []
    for row in rows():
        if row.score_only:
            cells.append(Cell(row, True, 0))
            continue
        for two_level in (False, True):
            for seed in args.seeds[: row.max_seeds]:
                cells.append(Cell(row, two_level, seed))
    cells = [c for c in cells if re.search(args.filter, c.name)]
    # Longest first, so the big networks do not end the sweep alone.
    cells.sort(key=lambda c: -(args.networks / c.row.network).stat().st_size)

    args.out.mkdir(parents=True, exist_ok=True)
    log = (args.out / "results.jsonl").open("w")
    start = time.monotonic()
    records = []
    print(f"{len(cells)} cells, {args.jobs} jobs, {args.timeout:g} s limit", flush=True)
    with ThreadPoolExecutor(args.jobs) as pool:
        futures = [pool.submit(run_cell, cell, args) for cell in cells]
        for future in as_completed(futures):
            record = future.result()
            records.append(record)
            log.write(json.dumps(record) + "\n")
            log.flush()
            run = record["run"]
            status = "; ".join(record.get("findings", [])) or run["status"]
            print(f"{run['seconds']:8.1f} s  {record['cell']}: {status}", flush=True)

    failed = [r for r in records if r["run"]["status"] not in ("ok", "timeout")]
    mismatched = [r for r in records if r.get("findings")]
    timeouts = [r for r in records if r["run"]["status"] == "timeout"]
    references = sum(r.get("reference", {}).get("status") == "ok" for r in records)
    print(
        f"\n{len(records)} cells in {time.monotonic() - start:.0f} s: "
        f"{len(failed)} failed runs, {len(mismatched)} with codelength findings, "
        f"{len(timeouts)} timeouts, {references} scored by the reference"
    )
    for r in failed:
        print(f"FAILED {r['cell']}: {r['run']['status']}\n  {r['run']['cmd']}")
        print("  " + r["run"].get("stderr", "").replace("\n", "\n  "))
    for r in mismatched:
        print(f"MISMATCH {r['cell']}: {'; '.join(r['findings'])}")
        print(f"  {r['run']['cmd']}")
    for r in timeouts:
        print(f"TIMEOUT {r['cell']}")
    print(f"results: {args.out / 'results.jsonl'}")
    return 1 if failed or mismatched else 0


if __name__ == "__main__":
    sys.exit(main())
