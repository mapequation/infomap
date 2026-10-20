#!/usr/bin/env python3
"""Differential checks: run the corpus in cases.tsv through a front end, and
compare the results of two or more runs.

  differential.py run-cli <infomap-binary> <out-dir>
  differential.py run-python <out-dir>
  differential.py compare <dir> <dir> [<dir> ...]
  differential.py compare-release <reference-dir> <current-dir>
  differential.py release-reference

A run writes, for each case, ``<id>.tree`` and ``<id>.codelength`` into the
output directory, or ``<id>.error`` when the front end refused the case. The
.tree file is the state-level tree for a higher-order network and the physical
tree otherwise. run_r.R and run_js.mjs write the same layout for R and
JavaScript.

``compare`` requires every run to agree with the first on every case, apart
from the disagreements listed in frontend_differences.tsv. ``compare-release``
compares a released CLI with the current one and requires each difference to be
listed in release_differences.tsv with the RESULT_CHANGES.csv rows that explain
it. ``release-reference`` prints the release tag that file compares with. See
README.md.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[1]
CASES = HERE / "cases.tsv"
RELEASE_DIFFERENCES = HERE / "release_differences.tsv"
FRONTEND_DIFFERENCES = HERE / "frontend_differences.tsv"
RESULT_CHANGES = REPO_ROOT / "RESULT_CHANGES.csv"

DEFAULT_SEED = "123"
# Far above rounding differences between compilers, far below a real change.
CODELENGTH_TOLERANCE = 1e-9
# Tree files print flow with six significant digits.
FLOW_RELATIVE_TOLERANCE = 1e-5
# Shown, but not checked: the order of modules and nodes with equal flow depends
# on the last bit of their flow, which differs between builds (#1325). Drop
# "order" from here, and list the order differences, once #1325 is fixed.
UNCHECKED = frozenset({"order"})


@dataclass(frozen=True)
class Case:
    id: str
    network: str
    options: dict[str, str | None]  # a flag maps to None

    def cli_args(self) -> list[str]:
        args = []
        for name, value in self.options.items():
            args.append("--" + name.replace("_", "-"))
            if value is not None:
                args.append(value)
        return args


def load_cases(path: Path = CASES) -> list[Case]:
    cases = []
    lines = [
        line
        for line in path.read_text().splitlines()
        if line and not line.startswith("#")
    ]
    header, *rows = lines
    if header.split("\t") != ["id", "network", "options"]:
        raise SystemExit(f"{path}: unexpected header {header!r}")
    for row in rows:
        case_id, network, options = (row.split("\t") + [""])[:3]
        parsed: dict[str, str | None] = {"seed": DEFAULT_SEED}
        for token in options.split():
            name, sep, value = token.partition("=")
            parsed[name] = value if sep else None
        cases.append(Case(case_id, network, parsed))
    ids = [case.id for case in cases]
    if len(ids) != len(set(ids)):
        raise SystemExit(f"{path}: duplicate case ids")
    return cases


def _write_error(out_dir: Path, case: Case, message: str) -> None:
    (out_dir / f"{case.id}.error").write_text(message.strip() + "\n")


def run_cli(binary: Path, out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    for case in load_cases():
        with tempfile.TemporaryDirectory() as tmp:
            command = [
                str(binary),
                str(REPO_ROOT / case.network),
                tmp,
                "--silent",
                "--out-name",
                "result",
                "--output",
                "tree,json",
                *case.cli_args(),
            ]
            proc = subprocess.run(
                command, cwd=REPO_ROOT, capture_output=True, text=True, check=False
            )
            tmp_path = Path(tmp)
            if proc.returncode != 0:
                _write_error(
                    out_dir,
                    case,
                    f"exit {proc.returncode}: {proc.stderr or proc.stdout}",
                )
                continue
            tree = tmp_path / "result_states.tree"
            if not tree.exists():
                tree = tmp_path / "result.tree"
            codelength = json.loads((tmp_path / "result.json").read_text())[
                "codelength"
            ]
            (out_dir / f"{case.id}.tree").write_text(tree.read_text())
            (out_dir / f"{case.id}.codelength").write_text(f"{codelength!r}\n")


def _python_value(value: str | None):
    if value is None:
        return True
    for convert in (int, float):
        try:
            return convert(value)
        except ValueError:
            pass
    return value


def run_python(out_dir: Path) -> None:
    import infomap

    out_dir.mkdir(parents=True, exist_ok=True)
    for case in load_cases():
        options = {name: _python_value(value) for name, value in case.options.items()}
        try:
            result = infomap.run(str(REPO_ROOT / case.network), **options)
            result.write_tree(
                str(out_dir / f"{case.id}.tree"), states=result.have_memory
            )
        except Exception as error:  # noqa: BLE001 - any refusal is a result to compare
            _write_error(out_dir, case, f"{type(error).__name__}: {error}")
            continue
        (out_dir / f"{case.id}.codelength").write_text(f"{result.codelength!r}\n")


# path flow "name" id [id ...]; the name runs from the first quote to the last.
_TREE_LINE = re.compile(r'^(\S+) (\S+) ".*" ([\d ]+)$')


@dataclass(frozen=True)
class Outcome:
    error: str | None = None
    skipped: str | None = None
    # The run wrote nothing for the case: a broken run, never a result.
    missing: bool = False
    codelength: float = math.nan
    # (path, (state_id, node_id, layer_id)) -> flow
    nodes: tuple = ()


def _parse_tree(text: str) -> tuple:
    nodes = []
    for line in text.splitlines():
        if not line or line.startswith("#"):
            continue
        match = _TREE_LINE.match(line)
        if match is None:
            raise ValueError(f"unparsed tree line {line!r}")
        path, flow, ids = match.groups()
        ids = tuple(int(i) for i in ids.split())
        if len(ids) == 1:  # first-order: the state id is the node id
            ids = (ids[0], ids[0])
        if len(ids) == 2:
            ids = (*ids, None)
        nodes.append((path, ids, float(flow)))
    return tuple(sorted(nodes, key=lambda node: (node[0], node[1])))


def load_outcome(run_dir: Path, case_id: str) -> Outcome:
    skip = run_dir / f"{case_id}.skip"
    if skip.exists():
        return Outcome(skipped=skip.read_text().strip())
    error = run_dir / f"{case_id}.error"
    if error.exists():
        return Outcome(error=error.read_text().strip())
    tree = run_dir / f"{case_id}.tree"
    codelength = run_dir / f"{case_id}.codelength"
    if not tree.exists() or not codelength.exists():
        return Outcome(missing=True)
    codelength = float(codelength.read_text())
    return Outcome(codelength=codelength, nodes=_parse_tree(tree.read_text()))


def _hierarchy(nodes: tuple) -> frozenset:
    """The modules at every level as (depth, set of nodes), without their ids or
    order. The depth tells a module from its only child module."""
    members: dict[str, set] = {}
    for path, ids, _ in nodes:
        parts = path.split(":")
        for depth in range(1, len(parts)):
            members.setdefault(":".join(parts[:depth]), set()).add(ids)
    return frozenset(
        (prefix.count(":") + 1, frozenset(module)) for prefix, module in members.items()
    )


def _flows(nodes: tuple) -> list:
    return sorted(((ids, flow) for _, ids, flow in nodes), key=lambda n: str(n[0]))


def differences(a: Outcome, b: Outcome) -> list[str]:
    """Which of missing, error, codelength, tree, order, flow differ between
    two outcomes.

    ``tree`` means the modules differ; ``order`` means the same modules with
    other module ids or another order of their children. ``flow`` compares the
    flow of each node, wherever it is in the tree.
    """
    if a.missing or b.missing:
        return ["missing"]
    if a.error is not None or b.error is not None:
        return [] if (a.error is None) == (b.error is None) else ["error"]
    found = []
    if not math.isclose(
        a.codelength, b.codelength, rel_tol=0, abs_tol=CODELENGTH_TOLERANCE
    ):
        found.append("codelength")
    if [(path, ids) for path, ids, _ in a.nodes] != [
        (path, ids) for path, ids, _ in b.nodes
    ]:
        found.append("tree" if _hierarchy(a.nodes) != _hierarchy(b.nodes) else "order")
    flows_a, flows_b = _flows(a.nodes), _flows(b.nodes)
    if [ids for ids, _ in flows_a] != [ids for ids, _ in flows_b] or not all(
        math.isclose(fa, fb, rel_tol=FLOW_RELATIVE_TOLERANCE, abs_tol=1e-12)
        for (_, fa), (_, fb) in zip(flows_a, flows_b, strict=True)
    ):
        found.append("flow")
    return found


def _describe(outcome: Outcome) -> str:
    if outcome.missing:
        return "no result written"
    if outcome.error is not None:
        return "error: " + outcome.error.splitlines()[0][:120]
    top = len({path.split(":")[0] for path, _, _ in outcome.nodes})
    depth = max((path.count(":") + 1 for path, _, _ in outcome.nodes), default=0)
    return f"codelength {outcome.codelength:.10f}, {top} top modules, depth {depth}"


def _name_changes(changes: list[str]) -> str:
    return ", ".join(
        f"{change} (not checked)" if change in UNCHECKED else change
        for change in changes
    )


@dataclass(frozen=True)
class ExpectedDifference:
    changes: frozenset[str]
    explained_by: tuple[str, ...]


def _read_table(
    path: Path, columns: list[str]
) -> tuple[dict[str, str], list[list[str]]]:
    """``# key: value`` lines and the rows of a tab-separated table with a header."""
    settings, rows = {}, []
    for line in path.read_text().splitlines():
        if line.startswith("#"):
            key, sep, value = line[1:].partition(":")
            if sep and " " not in key.strip():
                settings[key.strip()] = value.strip()
        elif line:
            rows.append(line.split("\t"))
    header, *rows = rows
    if header != columns:
        raise SystemExit(f"{path}: expected the header {chr(9).join(columns)!r}")
    return settings, rows


def _check(found: dict, expected: dict, what: str) -> tuple[int, list[str]]:
    """Every difference found must be listed, with the same changes, and every
    listed difference must still be found. Returns (listed count, problems)."""
    problems = []
    for key in sorted(found.keys() | expected.keys()):
        changes = found.get(key, frozenset())
        entry = expected.get(key)
        name = " in ".join(key) if isinstance(key, tuple) else key
        if entry is None:
            problems.append(
                f"{name}: {', '.join(sorted(changes))} differ from {what}, not listed"
            )
        elif not changes:
            problems.append(
                f"{name}: listed, but agrees with {what} now; remove the line"
            )
        elif changes != entry.changes:
            problems.append(
                f"{name}: {', '.join(sorted(changes))} differ from {what}, "
                f"listed as {', '.join(sorted(entry.changes))}"
            )
    listed = sum(
        1 for key in found if key in expected and found[key] == expected[key].changes
    )
    return listed, problems


def compare(
    run_dirs: list[Path],
    expected_path: Path = FRONTEND_DIFFERENCES,
    case_ids: list[str] | None = None,
) -> int:
    """Every run must agree with the first on every case, apart from the
    disagreements listed in ``expected_path``."""
    _, rows = _read_table(expected_path, ["id", "run", "changes", "explained_by"])
    expected = {
        (row[0], row[1]): ExpectedDifference(
            frozenset(row[2].split(",")), tuple(row[3].split())
        )
        for row in rows
    }
    reference, *others = run_dirs
    # Lines for a front end that is not compared here do not apply.
    names = {other.name for other in others}
    expected = {key: entry for key, entry in expected.items() if key[1] in names}
    cases = case_ids or [case.id for case in load_cases()]
    found = {}
    skipped = 0
    for case_id in cases:
        base = load_outcome(reference, case_id)
        for other in others:
            outcome = load_outcome(other, case_id)
            if base.skipped or outcome.skipped:
                skipped += 1
                print(
                    f"{case_id}: skipped in {other.name}: {base.skipped or outcome.skipped}"
                )
                continue
            changes = differences(base, outcome)
            if changes:
                checked = frozenset(changes) - UNCHECKED
                if checked:
                    found[(case_id, other.name)] = checked
                entry = expected.get((case_id, other.name))
                print(f"{case_id}: {_name_changes(changes)} differ")
                print(f"  {reference.name}: {_describe(base)}")
                print(f"  {other.name}: {_describe(outcome)}")
                if entry is not None:
                    print(f"  listed, {' '.join(entry.explained_by)}")
    listed, problems = _check(found, expected, reference.name)
    print(
        f"\n{len(cases)} cases, {len(run_dirs)} runs, {listed} listed disagreements, {skipped} skipped"
    )
    for problem in problems:
        print(f"FAIL {problem}")
    return 1 if problems else 0


def _unreleased_change_refs(reference: str) -> set[str]:
    """#issue and #pr numbers of the RESULT_CHANGES.csv rows newer than ``reference``."""
    released = tuple(int(part) for part in reference.lstrip("v").split("."))
    refs = set()
    with RESULT_CHANGES.open(newline="") as handle:
        for row in csv.DictReader(handle):
            # A row waiting for the next release is newer than every release.
            if row["version"] == "unreleased" or (
                tuple(int(part) for part in row["version"].split(".")) > released
            ):
                refs.update(
                    f"#{number}" for number in (row["issue"], row["pr"]) if number
                )
    return refs


def release_reference(path: Path = RELEASE_DIFFERENCES) -> str:
    settings, _ = _read_table(path, ["id", "changes", "explained_by"])
    if "reference" not in settings:
        raise SystemExit(f"{path}: no '# reference: <tag>' line")
    return settings["reference"]


def compare_release(
    reference_dir: Path,
    current_dir: Path,
    expected_path: Path = RELEASE_DIFFERENCES,
    case_ids: list[str] | None = None,
) -> int:
    """The current CLI must match the released one, apart from the differences
    listed in ``expected_path``, each explained by RESULT_CHANGES.csv rows."""
    reference = release_reference(expected_path)
    _, rows = _read_table(expected_path, ["id", "changes", "explained_by"])
    expected = {
        row[0]: ExpectedDifference(frozenset(row[1].split(",")), tuple(row[2].split()))
        for row in rows
    }
    cases = case_ids or [case.id for case in load_cases()]
    found = {}
    for case_id in cases:
        old = load_outcome(reference_dir, case_id)
        new = load_outcome(current_dir, case_id)
        changes = differences(old, new)
        if changes:
            checked = frozenset(changes) - UNCHECKED
            if checked:
                found[case_id] = checked
            print(f"{case_id}: {_name_changes(changes)}")
            print(f"  {reference}: {_describe(old)}")
            print(f"  current: {_describe(new)}")
            if case_id in expected:
                print(f"  explained by {' '.join(expected[case_id].explained_by)}")
    listed, problems = _check(found, expected, reference)
    known_refs = _unreleased_change_refs(reference)
    for case_id, entry in expected.items():
        if case_id not in cases:
            problems.append(f"{case_id}: listed, but not in cases.tsv")
        unknown = [ref for ref in entry.explained_by if ref not in known_refs]
        if unknown or not entry.explained_by:
            problems.append(
                f"{case_id}: explained_by needs the issue or PR of a RESULT_CHANGES.csv row "
                f"after {reference}; {' '.join(unknown) or 'none'} given"
            )
    print(f"\n{len(cases)} cases, {listed} listed differences from {reference}")
    for problem in problems:
        print(f"FAIL {problem}")
    return 1 if problems else 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = parser.add_subparsers(dest="command", required=True)
    cli = sub.add_parser("run-cli")
    cli.add_argument("binary", type=Path)
    cli.add_argument("out_dir", type=Path)
    python = sub.add_parser("run-python")
    python.add_argument("out_dir", type=Path)
    cmp = sub.add_parser("compare")
    cmp.add_argument("run_dirs", type=Path, nargs="+")
    release = sub.add_parser("compare-release")
    release.add_argument("reference_dir", type=Path)
    release.add_argument("current_dir", type=Path)
    sub.add_parser("release-reference")
    args = parser.parse_args(argv)

    if args.command == "run-cli":
        run_cli(args.binary.resolve(), args.out_dir)
    elif args.command == "run-python":
        run_python(args.out_dir)
    elif args.command == "release-reference":
        print(release_reference())
    elif args.command == "compare":
        if len(args.run_dirs) < 2:
            parser.error("compare needs at least two run directories")
        return compare(args.run_dirs)
    else:
        return compare_release(args.reference_dir, args.current_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
