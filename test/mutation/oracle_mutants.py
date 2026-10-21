#!/usr/bin/env python3
"""Count how many hand-made faults in the map equation and flow code the oracle tests kill.

A one-off measure of oracle strength (#1261). Each mutant is a small textual
change to MapEquation, MemMapEquation, MetaMapEquation or FlowCalculator: a sign
flip, an off-by-one, a dropped term or a wrong log base. For each one the script
applies it, rebuilds the Python extension in place with the flow invariants on
(INFOMAP_CHECK_INVARIANTS, as in debug builds), runs the oracle tests, and
restores the source. A mutant is killed when a test fails or the build fails.

The oracle tests are the reference codelength and flow oracles (#1177, #1181),
the exhaustive-optimum and metamorphic checks (#1200) and the pairwise sweep
(#1225), with the invariants (checkInvariants) on inside every run.

Run from the repository root in a virtual environment with the test extras:

    python test/mutation/oracle_mutants.py              # every mutant
    python test/mutation/oracle_mutants.py M3 F2        # some of them
    python test/mutation/oracle_mutants.py --slow       # with the slow sweeps
    python test/mutation/oracle_mutants.py --no-invariants  # as release builds

It builds with ccache when it is on PATH, so a mutant recompiles only the files
that include the changed one. The source is restored after each mutant, and at
the end the extension is rebuilt without the checks; it stays serial (OPENMP=0)
until you rebuild it as usual. SIGINT and SIGTERM restore the source before
exiting, SIGKILL does not: check ``git status src`` then. A mutant whose text
is no longer in the source is reported as stale; update or drop it.
"""

from __future__ import annotations

import argparse
import os
import shutil
import signal
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

ORACLE_TESTS = [
    "test/python/test_reference_codelength.py",
    "test/python/test_reference_flow.py",
    "test/python/test_exhaustive_optimum.py",
    "test/python/test_metamorphic.py",
    "test/python/test_pairwise_options.py",
]

# Messages of the C++ invariant checks, to tell their kills from the oracles'.
INVARIANT_MARKS = ("Flow invariant violated", "is a bug in")


@dataclass(frozen=True)
class Mutant:
    key: str
    path: str
    old: str
    new: str
    what: str


MAPEQ = "src/core/MapEquation.h"
MEM = "src/core/MemMapEquation.cpp"
META = "src/core/MetaMapEquation.cpp"
FLOW = "src/utils/FlowCalculator.cpp"

MUTANTS = [
    # MapEquation
    Mutant(
        "M1",
        MAPEQ,
        "  }\n  indexLength -= infomath::plogp(parentExit / totalParentFlow);\n",
        "  }\n",
        "leaf module codebook: drop the exit codeword",
    ),
    Mutant(
        "M2",
        MAPEQ,
        "return infomath::plogp(totalCodewordUse) - sumEnterLogEnter - infomath::plogp(parentExit);",
        "return infomath::plogp(totalCodewordUse) - sumEnterLogEnter + infomath::plogp(parentExit);",
        "module of modules codebook: flip the sign of the exit term",
    ),
    Mutant(
        "M3",
        MAPEQ,
        "  enterFlow += exitNetworkFlow;\n",
        "",
        "index codebook: drop the network exit flow from its use rate",
    ),
    Mutant(
        "M4",
        MAPEQ,
        "  enterFlow_log_enterFlow = infomath::plogp(enterFlow);\n}",
        "  enterFlow_log_enterFlow = enterFlow * std::log(enterFlow);\n}",
        "index codebook use rate: natural log instead of log2",
    ),
    Mutant(
        "M5",
        MAPEQ,
        "double deltaL = delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;",
        "double deltaL = delta_enter - delta_enter_log_enter + delta_exit_log_exit + delta_flow_log_flow;",
        "move delta: flip the sign of the exit term",
    ),
    Mutant(
        "M6",
        MAPEQ,
        "      nodeFlow_log_nodeFlow += infomath::plogp(node.data.flow);\n",
        "      nodeFlow_log_nodeFlow += infomath::plogp(node.data.enterFlow);\n",
        "leaf entropy term: enter flow instead of node flow",
    ),
    Mutant(
        "M7",
        MAPEQ,
        "  moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;\n",
        "  moduleFlowData[newModule].exitFlow += deltaEnterExitNewModule;\n",
        "move update: flip the sign of the new module's exit flow change",
    ),
    # MemMapEquation
    Mutant(
        "Mem1",
        MEM,
        "  }\n  indexLength -= infomath::plogp(parentExit / totalParentFlow);\n",
        "  }\n",
        "leaf module codebook: drop the exit codeword",
    ),
    Mutant(
        "Mem2",
        MEM,
        "  codelength -= delta_nodeFlow_log_nodeFlow;\n",
        "  codelength += delta_nodeFlow_log_nodeFlow;\n",
        "move update: flip the sign of the physical-node entropy change",
    ),
    Mutant(
        "Mem3",
        MEM,
        "for (unsigned int i = 0; i < m_numPhysicalNodes; ++i) {\n    const ModuleToMemNodes&",
        "for (unsigned int i = 0; i + 1 < m_numPhysicalNodes; ++i) {\n    const ModuleToMemNodes&",
        "physical-node entropy: skip the last physical node (off by one)",
    ),
    Mutant(
        "Mem4",
        MEM,
        "  return deltaL - delta_nodeFlow_log_nodeFlow;\n",
        "  return deltaL;\n",
        "move delta: drop the physical-node entropy change",
    ),
    # MetaMapEquation
    Mutant(
        "Meta1",
        META,
        "return codelength + metaCodelength * metaDataRate;",
        "return codelength + metaCodelength;",
        "total codelength: drop the meta data rate",
    ),
    Mutant(
        "Meta2",
        META,
        "metaCollection.add(node.metaData()[0], weightByFlow ? node.data.flow : m_unweightedNodeFlow);",
        "metaCollection.add(node.metaData()[0], m_unweightedNodeFlow);",
        "leaf module meta entropy: always unweighted",
    ),
    Mutant(
        "Meta3",
        META,
        "  deltaMetaL += getCurrentModuleMetaCodelength(newModuleIndex, current, 1);\n",
        "  deltaMetaL += getCurrentModuleMetaCodelength(newModuleIndex, current, 0);\n",
        "move delta: new module's meta entropy without the moved node",
    ),
    # FlowCalculator
    Mutant(
        "F1",
        FLOW,
        "    if (link.source != link.target) {\n      link.flow *= 2;\n",
        "    if (link.source != link.target) {\n      link.flow *= 1;\n",
        "undirected link flow: count one direction only",
    ),
    Mutant(
        "F2",
        FLOW,
        "auto toNode = config.recordedTeleportation ? link.target : link.source;\n      nodeTeleportWeights[toNode] += link.flow / sumLinkWeight;\n    }\n  }\n\n  // Normalize link weights with respect to its source nodes total out-link weight;\n  for (auto& link : flowLinks) {\n    if (sumLinkOutWeight[link.source] > 0) {\n      link.flow /= sumLinkOutWeight[link.source];\n    }\n  }\n\n  std::vector<double> nodeFlowTmp(numNodes, 0.0);\n  double danglingRank;\n\n  // Calculate PageRank",
        "auto toNode = config.recordedTeleportation ? link.source : link.target;\n      nodeTeleportWeights[toNode] += link.flow / sumLinkWeight;\n    }\n  }\n\n  // Normalize link weights with respect to its source nodes total out-link weight;\n  for (auto& link : flowLinks) {\n    if (sumLinkOutWeight[link.source] > 0) {\n      link.flow /= sumLinkOutWeight[link.source];\n    }\n  }\n\n  std::vector<double> nodeFlowTmp(numNodes, 0.0);\n  double danglingRank;\n\n  // Calculate PageRank",
        "directed teleportation to links: swap link source and target",
    ),
    Mutant(
        "F3",
        FLOW,
        "    const auto teleportationFlow = alpha + beta * danglingRank;\n    for (unsigned int i = 0; i < numNodes; ++i) {",
        "    const auto teleportationFlow = alpha + danglingRank;\n    for (unsigned int i = 0; i < numNodes; ++i) {",
        "directed PageRank: dangling flow teleports without beta",
    ),
    Mutant(
        "F4",
        FLOW,
        "    sumNodeRank = 1.0 - danglingRank;\n    nodeFlow.assign(numNodes, 0.0);",
        "    sumNodeRank = 1.0;\n    nodeFlow.assign(numNodes, 0.0);",
        "unrecorded teleportation: do not renormalize the last step",
    ),
    Mutant(
        "F5",
        FLOW,
        "link.flow *= nodeFlowSteadyState[link.source] / sumLinkOutWeight[link.source] / sumNodeFlow;",
        "link.flow *= nodeFlowSteadyState[link.source] / sumLinkOutWeight[link.source];",
        "undirdir/outdirdir link flow: drop the normalization",
    ),
    Mutant(
        "F6",
        FLOW,
        "exitFlow[sourceIndex] += alpha * node.flow * (1.0 - node.weight);",
        "exitFlow[sourceIndex] += alpha * node.flow;",
        "recorded teleportation: count self-teleportation as exit flow",
    ),
    Mutant(
        "F7",
        FLOW,
        "unsigned int norm = config.isUndirectedFlow() ? 2 : 1;",
        "unsigned int norm = 1;",
        "leaf enter/exit flow: remove an undirected self-link twice",
    ),
    Mutant(
        "F8",
        FLOW,
        "enterFlow[sourceIndex] += (sumTeleFlow - node.teleFlow) * node.weight;",
        "enterFlow[sourceIndex] += sumTeleFlow * node.weight;",
        "regularized leaf enter flow: count self-teleportation",
    ),
]


def build_env(invariants: bool) -> dict[str, str]:
    env = dict(os.environ)
    if shutil.which("ccache"):
        env.setdefault(
            "CC", "ccache clang" if sys.platform == "darwin" else "ccache gcc"
        )
        env.setdefault(
            "CXX", "ccache clang++" if sys.platform == "darwin" else "ccache g++"
        )
    env["OPENMP"] = "0"
    if invariants:
        flags = env.get("CXXFLAGS", "") + " -DINFOMAP_CHECK_INVARIANTS=1"
        env["CXXFLAGS"] = flags.strip()
    env.setdefault("INFOMAP_BUILD_JOBS", "4")
    return env


def build(invariants: bool = True) -> bool:
    proc = subprocess.run(
        [sys.executable, "setup.py", "build_ext", "--inplace", "--force"],
        cwd=REPO,
        env=build_env(invariants),
        capture_output=True,
        text=True,
        check=False,
    )
    return proc.returncode == 0


def run_tests(slow: bool, timeout: int) -> tuple[str, list[str]]:
    """'pass', 'fail', 'invariant', 'crash' or 'timeout', and the failing test ids."""
    env = dict(os.environ)
    env["PYTHONPATH"] = str(REPO / "interfaces" / "python" / "src")
    # test_metamorphic.py runs Infomap at import time, so a fault can fail its
    # collection; without the flag that would stop the whole session.
    cmd = [sys.executable, "-m", "pytest", "-q", "-p", "no:cacheprovider", "-rfE"]
    cmd += ["--continue-on-collection-errors"]
    cmd += ORACLE_TESTS + (["--run-slow"] if slow else ["-m", "not slow"])
    try:
        proc = subprocess.run(
            cmd,
            cwd=REPO,
            env=env,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired:
        return "timeout", []
    if proc.returncode == 0:
        return "pass", []
    failed = [
        line.split()[1]
        for line in proc.stdout.splitlines()
        if line.startswith(("FAILED ", "ERROR "))
    ]
    if not failed:
        # No test summary: pytest itself died, e.g. on an abort in the engine.
        return f"crash ({proc.returncode})", []
    kind = "invariant" if any(m in proc.stdout for m in INVARIANT_MARKS) else "fail"
    return kind, failed


def summarize(failed: list[str]) -> str:
    files: dict[str, int] = {}
    for test in failed:
        name = Path(test.split("::")[0]).stem.removeprefix("test_")
        files[name] = files.get(name, 0) + 1
    return ", ".join(f"{name} {n}" for name, n in sorted(files.items()))


def _interrupt(signum, frame):
    raise KeyboardInterrupt


def main() -> int:
    signal.signal(signal.SIGTERM, _interrupt)
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("keys", nargs="*", help="mutants to run (default: all)")
    parser.add_argument("--slow", action="store_true", help="include the slow tests")
    parser.add_argument(
        "--no-invariants",
        action="store_true",
        help="build without the invariant checks, like the release wheels",
    )
    parser.add_argument("--timeout", type=int, default=1800)
    args = parser.parse_args()
    chosen = [m for m in MUTANTS if not args.keys or m.key in args.keys]

    rows = []
    for mutant in chosen:
        path = REPO / mutant.path
        original = path.read_text()
        count = original.count(mutant.old)
        if count == 0:
            rows.append((mutant, "stale", "", 0.0))
            continue
        start = time.monotonic()
        try:
            path.write_text(original.replace(mutant.old, mutant.new))
            if not build(not args.no_invariants):
                status, failed = "build", []
            else:
                status, failed = run_tests(args.slow, args.timeout)
        finally:
            path.write_text(original)
        rows.append((mutant, status, summarize(failed), time.monotonic() - start))
        print(
            f"{mutant.key}: {status} {summarize(failed)}", file=sys.stderr, flush=True
        )

    build(invariants=False)

    print("| Mutant | File | Change | Result | Killed by |")
    print("|---|---|---|---|---|")
    for mutant, status, by, _ in rows:
        result = "survived" if status == "pass" else status
        print(
            f"| {mutant.key} | `{Path(mutant.path).name}` | {mutant.what} | {result} | {by} |"
        )
    survived = sum(status == "pass" for _, status, _, _ in rows)
    stale = sum(status == "stale" for _, status, _, _ in rows)
    run = len(rows) - stale
    print(f"\n{run - survived} of {run} killed, {survived} survived, {stale} stale.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
