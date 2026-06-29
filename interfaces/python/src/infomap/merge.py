"""Deterministic merge of distributed Infomap trial-results shards.

Infomap can run a *shard* of trials in a separate process via ``--trial-offset``
and emit per-shard results with ``--trial-results <file.json>`` (see the Infomap
CLI). Each shard runs global trial indices ``[offset, offset + num_trials)`` with
``seed = base_seed + global_index``, so any partition of ``[0, N)`` reproduces the
trials of a single ``--num-trials N`` run.

This module merges those shard files: it verifies every shard came from the same
network and algorithm configuration, selects the global best trial (lowest
codelength, ties broken by lowest global trial index), and writes the final
``tree`` / ``clu`` output from the winning trial's ``.tree`` — without re-running
Infomap and without needing the original network.

Example (after a SLURM job array wrote ``results_0.json`` … ``results_3.json``)::

    python -m infomap.merge results_*.json --out-name final --output tree,clu

or programmatically::

    from infomap import merge_trial_results
    summary = merge_trial_results(["results_*.json"], out_name="final")
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import shutil
import sys
from typing import Iterable, List, Sequence, Tuple, TypedDict

__all__ = ["merge_trial_results", "MergeError"]

SUPPORTED_FORMATS = ("tree", "clu")


class MergeSummary(TypedDict):
    """Summary returned by :func:`merge_trial_results`."""

    trial: int
    codelength: float
    winner_tree: str
    num_shards: int
    num_trials: int
    missing: List[int]
    outputs: List[str]

# Top-level keys every shard results file must contain.
_REQUIRED_FILE_KEYS = (
    "network_fingerprint",
    "config_fingerprint",
    "best_tree_file",
    "trials",
)
# Per-trial keys required so a malformed/partial shard cannot win selection.
_REQUIRED_TRIAL_KEYS = ("trial", "codelength")


class MergeError(Exception):
    """Raised when shard files are missing, inconsistent, or unmergeable."""


def _expand_patterns(patterns: Iterable[str]) -> List[str]:
    """Expand glob patterns (each item may itself be a comma-separated list)."""
    paths: List[str] = []
    seen = set()
    for raw in patterns:
        for pattern in str(raw).split(","):
            pattern = pattern.strip()
            if not pattern:
                continue
            matches = sorted(glob.glob(pattern))
            if not matches:
                # A literal (non-glob) path that does not exist is an error;
                # a wildcard that matches nothing is also an error.
                raise MergeError(f"no shard files matched '{pattern}'")
            for m in matches:
                if m not in seen:
                    seen.add(m)
                    paths.append(m)
    if not paths:
        raise MergeError("no shard result files given")
    return paths


def _load_shard(path: str) -> dict:
    try:
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, ValueError) as exc:
        raise MergeError(f"cannot read shard file '{path}': {exc}") from exc
    if not isinstance(data, dict):
        raise MergeError(f"shard file '{path}' is not a JSON object")
    for key in _REQUIRED_FILE_KEYS:
        if key not in data:
            raise MergeError(f"shard file '{path}' is missing required key '{key}'")
    if not isinstance(data["trials"], list):
        raise MergeError(f"shard file '{path}' has a non-list 'trials' field")
    for trial in data["trials"]:
        for key in _REQUIRED_TRIAL_KEYS:
            if key not in trial:
                raise MergeError(
                    f"shard file '{path}' has a trial missing required key '{key}'"
                )
    return data


def _validate_consistent(shards: Sequence[Tuple[str, dict]]) -> None:
    """All shards must share the same non-empty network + config fingerprint."""
    first_path, first = shards[0]
    for field in ("network_fingerprint", "config_fingerprint"):
        value = first.get(field, "")
        if not value:
            raise MergeError(
                f"shard file '{first_path}' has an empty {field}; cannot verify "
                "shards describe the same run"
            )
        for path, shard in shards[1:]:
            if shard.get(field, "") != value:
                raise MergeError(
                    f"{field} mismatch: '{first_path}' and '{path}' describe "
                    "different runs and cannot be merged"
                )


def _select_winner(shards: Sequence[Tuple[str, dict]]):
    """Return (shard_path, shard, winning_trial) with the global best codelength.

    Tie-break: lowest global trial index (so the result is independent of how
    trials were partitioned across shards).
    """
    best = None  # (codelength, trial_index, shard_path, shard, trial)
    for path, shard in shards:
        for trial in shard["trials"]:
            key = (float(trial["codelength"]), int(trial["trial"]))
            if best is None or key < best[0]:
                best = (key, path, shard, trial)
    if best is None:
        raise MergeError("no trials found across the shard files")
    _, path, shard, trial = best
    return path, shard, trial


def _covered_indices(shards: Sequence[Tuple[str, dict]]) -> set:
    return {int(t["trial"]) for _, s in shards for t in s["trials"]}


def _resolve_best_tree(shard_path: str, best_tree_file: str) -> str:
    """Resolve best_tree_file relative to the shard JSON's directory."""
    if not best_tree_file:
        raise MergeError(
            f"winning shard '{shard_path}' recorded no best tree file; re-run "
            "shards with tree output enabled"
        )
    if os.path.isabs(best_tree_file):
        return best_tree_file
    return os.path.join(os.path.dirname(shard_path), best_tree_file)


def _write_clu_from_tree(tree_path: str, clu_path: str) -> None:
    """Derive a .clu (node -> top-level module + flow) from a .tree file.

    .tree node lines are ``path flow "name" node_id``; the top-level module is
    the first component of the colon-separated path. We read node_id (last
    token), flow (second token) and path (first token); the name is ignored.
    """
    # node_id may be a non-numeric label, so it stays str when not all-digits.
    rows: List[Tuple[int | str, int, str]] = []
    with open(tree_path, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            tokens = line.split()
            if len(tokens) < 4:
                continue  # not a leaf node line
            path, flow, node_id = tokens[0], tokens[1], tokens[-1]
            try:
                top_module = int(path.split(":")[0])
            except ValueError:
                continue
            rows.append((int(node_id) if node_id.isdigit() else node_id, top_module, flow))
    tmp_path = clu_path + ".tmp"
    with open(tmp_path, "w", encoding="utf-8") as out:
        out.write("# produced by infomap.merge\n")
        out.write("# node_id module flow\n")
        for node_id, module, flow in rows:
            out.write(f"{node_id} {module} {flow}\n")
    os.replace(tmp_path, clu_path)  # atomic on POSIX/Windows


def merge_trial_results(
    patterns: Sequence[str],
    out_name: str,
    formats: Sequence[str] = SUPPORTED_FORMATS,
    require_complete: bool = False,
) -> MergeSummary:
    """Merge distributed Infomap trial-results shards into final output.

    Parameters
    ----------
    patterns:
        Shard result file paths or glob patterns (each may be comma-separated).
    out_name:
        Output basename; ``<out_name>.tree`` / ``<out_name>.clu`` are written.
    formats:
        Which output formats to write. Only ``tree`` and ``clu`` are supported
        (the merge has no network, so link-bearing formats cannot be produced).
    require_complete:
        If True, raise when any global trial index in ``[0, max]`` is missing.

    Returns
    -------
    dict
        Summary with the winning ``trial`` index, ``codelength``, the resolved
        winner tree path, the number of shards/trials, any ``missing`` indices,
        and the list of ``outputs`` written.
    """
    bad = [f for f in formats if f not in SUPPORTED_FORMATS]
    if bad:
        raise MergeError(
            f"unsupported output format(s) {bad}; merge supports only "
            f"{list(SUPPORTED_FORMATS)} (no network is available to derive others)"
        )

    paths = _expand_patterns(patterns)
    shards = [(p, _load_shard(p)) for p in paths]
    _validate_consistent(shards)

    shard_path, winner_shard, winner = _select_winner(shards)
    winner_tree = _resolve_best_tree(shard_path, winner_shard["best_tree_file"])
    if not os.path.isfile(winner_tree):
        raise MergeError(
            f"winning trial's tree file does not exist or is not a file: "
            f"'{winner_tree}'"
        )

    # Completeness check over the global trial range.
    covered = _covered_indices(shards)
    max_index = max(covered)
    missing = sorted(set(range(max_index + 1)) - covered)
    if missing:
        msg = (
            f"{len(missing)} global trial index(es) missing in [0, {max_index}]"
            f"{'' if len(missing) > 20 else ': ' + ' '.join(map(str, missing))}"
        )
        if require_complete:
            raise MergeError(msg)
        print(f"Warning: {msg}", file=sys.stderr)

    outputs: List[str] = []
    if "tree" in formats:
        tree_out = out_name + ".tree"
        os.makedirs(os.path.dirname(tree_out) or ".", exist_ok=True)
        shutil.copyfile(winner_tree, tree_out)
        outputs.append(tree_out)
    if "clu" in formats:
        clu_out = out_name + ".clu"
        os.makedirs(os.path.dirname(clu_out) or ".", exist_ok=True)
        _write_clu_from_tree(winner_tree, clu_out)
        outputs.append(clu_out)

    return {
        "trial": int(winner["trial"]),
        "codelength": float(winner["codelength"]),
        "winner_tree": winner_tree,
        "num_shards": len(shards),
        "num_trials": len(covered),
        "missing": missing,
        "outputs": outputs,
    }


def _parse_formats(value: str) -> List[str]:
    return [f.strip() for f in value.split(",") if f.strip()]


def _main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="python -m infomap.merge",
        description="Merge distributed Infomap trial-results shards into final output.",
    )
    parser.add_argument(
        "patterns", nargs="+",
        help="Shard result JSON files or glob patterns (comma lists allowed).",
    )
    parser.add_argument(
        "--out-name", required=True,
        help="Output basename; writes <out-name>.tree / .clu.",
    )
    parser.add_argument(
        "--output", default="tree,clu", type=_parse_formats,
        help="Comma-separated output formats (tree, clu). Default: tree,clu.",
    )
    parser.add_argument(
        "--require-complete-trials", action="store_true",
        help="Fail if any global trial index in [0, max] is missing.",
    )
    args = parser.parse_args(argv)

    try:
        summary = merge_trial_results(
            args.patterns,
            out_name=args.out_name,
            formats=args.output,
            require_complete=args.require_complete_trials,
        )
    except MergeError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2

    print(
        f"Merged {summary['num_trials']} trials from {summary['num_shards']} shard(s). "
        f"Winner: global trial {summary['trial']} with codelength "
        f"{summary['codelength']:.6g}. Wrote: {', '.join(summary['outputs'])}."
    )
    return 0


if __name__ == "__main__":
    sys.exit(_main())
