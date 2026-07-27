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

    from infomap.merge import merge_trial_results
    summary = merge_trial_results(["results_*.json"], out_name="final")
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import shutil
import sys
from collections.abc import Iterable, Sequence
from typing import TypedDict

__all__ = ["merge_trial_results", "MergeError", "MergeSummary"]

SUPPORTED_FORMATS = ("tree", "clu")


class MergeSummary(TypedDict):
    """Summary returned by :func:`merge_trial_results`."""

    #: Global index of the winning trial (lowest codelength, ties broken by
    #: lowest index).
    trial: int
    #: Codelength of the winning trial.
    codelength: float
    #: Resolved path to the winning trial's ``.tree`` file.
    winner_tree: str
    #: Number of shard result files merged.
    num_shards: int
    #: Number of distinct global trial indices covered by the shards.
    num_trials: int
    #: Global trial indices missing from ``[0, max_index]``, if any.
    missing: list[int]
    #: Paths of the output files written.
    outputs: list[str]


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


def _expand_patterns(patterns: Iterable[str | os.PathLike[str]]) -> list[str]:
    """Expand glob patterns.

    A ``str`` item may be a comma-separated list of patterns (CLI parity);
    an ``os.PathLike`` item names exactly one pattern -- a comma in it is
    part of the path.
    """
    paths: list[str] = []
    seen = set()
    for raw in patterns:
        if isinstance(raw, str):
            items = raw.split(",")
        else:
            items = [os.fsdecode(raw)]
        for pattern in items:
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
        with open(path, encoding="utf-8") as fh:
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
        # Without this check a string entry would pass the `key not in trial`
        # test below via substring containment and crash much later in
        # _select_winner instead of raising a MergeError here.
        if not isinstance(trial, dict):
            raise MergeError(f"shard file '{path}' has a non-object entry in 'trials'")
        for key in _REQUIRED_TRIAL_KEYS:
            if key not in trial:
                raise MergeError(
                    f"shard file '{path}' has a trial missing required key '{key}'"
                )
    return data


def _validate_consistent(shards: Sequence[tuple[str, dict]]) -> None:
    """All shards must share the same network, configuration and Infomap build.

    ``infomap_version`` is checked alongside the fingerprints because a different
    build can partition the same network differently -- a fixed bug changes results
    exactly as much as a changed option does -- and the field was handed to us and
    then ignored.
    """
    first_path, first = shards[0]
    for field in ("network_fingerprint", "config_fingerprint", "infomap_version"):
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


def _select_winner(shards: Sequence[tuple[str, dict]]):
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


def _covered_indices(shards: Sequence[tuple[str, dict]]) -> set:
    return {int(t["trial"]) for _, s in shards for t in s["trials"]}


def _duplicate_indices(shards: Sequence[tuple[str, dict]]) -> dict[int, list[str]]:
    """Global trial indices claimed more than once, mapped to the shards claiming them.

    Shards that overlap -- or that were all launched with the same --trial-offset --
    contribute fewer independent trials than the executions suggest, while the gap
    check below sees a contiguous range and reports the budget as complete. Four
    shards of two trials each at offset 0 merged as "2 trials from 4 shard(s)".
    """
    claims: dict[int, list[str]] = {}
    for path, shard in shards:
        for trial in shard["trials"]:
            claims.setdefault(int(trial["trial"]), []).append(path)
    return {index: paths for index, paths in claims.items() if len(paths) > 1}


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


def _parse_tree_row(line: str) -> tuple[str, str, list[str]] | None:
    """Split a .tree leaf row into (path, flow, trailing ids), or None if it is not one.

    Rows are ``path flow "name" id...``, and the name is taken from the first quote to
    the last, the same rule Infomap's own tree parser uses. Splitting on whitespace and
    indexing from the left breaks on a name containing a space; taking the last token
    breaks on a state row, where the trailing ids are ``state_id node_id [layer_id]``.
    """
    head, quote, rest = line.partition('"')
    if not quote:
        return None
    name_end = rest.rfind('"')
    if name_end < 0:
        return None
    head_tokens = head.split()
    if len(head_tokens) < 2:
        return None
    trailing = rest[name_end + 1 :].split()
    if not trailing:
        return None
    return head_tokens[0], head_tokens[1], trailing


def _write_clu_from_tree(tree_path: str, clu_path: str, states: bool = False) -> None:
    """Derive a .clu from a .tree, mirroring the columns Infomap writes itself.

    Physical: ``# node_id module flow``. State-level: ``# state_id module flow node_id``
    plus ``layer_id`` when the rows carry one, keyed on the state id -- a higher-order network's physical rows repeat
    the same node id under several modules, so a .clu keyed on it cannot express the
    partition at all (#906).
    """
    rows: list[tuple[str, int, str, list[str]]] = []
    with open(tree_path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parsed = _parse_tree_row(line)
            if parsed is None:
                continue
            path, flow, trailing = parsed
            try:
                top_module = int(path.split(":")[0])
            except ValueError:
                continue
            rows.append((trailing[0], top_module, flow, trailing[1:]))

    if states:
        # Infomap's own _states.clu adapts its columns: a memory network's state rows are
        # "state_id node_id" while multilayer adds the layer. Advertising layer_id
        # unconditionally would name a column the rows do not have.
        trailing_columns = max((len(extra) for _, _, _, extra in rows), default=0)
        names = ["node_id", "layer_id"][:trailing_columns]
        header = " ".join(["# state_id", "module", "flow", *names])
    else:
        header = "# node_id module flow"
    tmp_path = clu_path + ".tmp"
    with open(tmp_path, "w", encoding="utf-8") as out:
        out.write("# produced by infomap.merge\n")
        out.write(header + "\n")
        for key, module, flow, extra in rows:
            trailing = (" " + " ".join(extra)) if states and extra else ""
            out.write(f"{key} {module} {flow}{trailing}\n")
    os.replace(tmp_path, clu_path)  # atomic on POSIX/Windows


def merge_trial_results(
    patterns: Sequence[str | os.PathLike[str]],
    out_name: str | os.PathLike[str],
    formats: Sequence[str] = SUPPORTED_FORMATS,
    require_complete: bool = False,
) -> MergeSummary:
    """Merge distributed Infomap trial-results shards into final output.

    Parameters
    ----------
    patterns : sequence of str or os.PathLike
        Shard result file paths or glob patterns. A ``str`` may be a
        comma-separated list of patterns; an ``os.PathLike`` names exactly
        one pattern.
    out_name : str or os.PathLike
        Output basename; ``<out_name>.tree`` / ``<out_name>.clu`` are written.
    formats : sequence of str, optional
        Which output formats to write. Only ``tree`` and ``clu`` are supported
        (the merge has no network, so link-bearing formats cannot be produced).
    require_complete : bool, optional
        If True, raise when any global trial index in ``[0, max]`` is missing.
        Default ``False``.

    Returns
    -------
    MergeSummary
        Summary with the winning ``trial`` index, ``codelength``, the resolved
        winner tree path, the number of shards/trials, any ``missing`` indices,
        and the list of ``outputs`` written.
    """
    out_name = os.fsdecode(out_name)
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

    # Independence check: the same global trial index in two shards is the same trial
    # run twice, not two trials, so the effective budget is smaller than it looks.
    duplicates = _duplicate_indices(shards)
    if duplicates:
        shown = sorted(duplicates)[:10]
        detail = ", ".join(
            f"{index} in {len(duplicates[index])} shards" for index in shown
        )
        msg = (
            f"{len(duplicates)} global trial index(es) claimed by more than one shard "
            f"({detail}{', …' if len(duplicates) > len(shown) else ''}); the shards "
            "overlap, so they contribute fewer independent trials than they ran"
        )
        if require_complete:
            raise MergeError(msg)
        print(f"Warning: {msg}", file=sys.stderr)

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

    outputs: list[str] = []
    if "tree" in formats:
        tree_out = out_name + ".tree"
        os.makedirs(os.path.dirname(tree_out) or ".", exist_ok=True)
        # Copy via a sibling temp file + os.replace so the final output
        # appears atomically, mirroring _write_clu_from_tree.
        tree_tmp = tree_out + ".tmp"
        shutil.copyfile(winner_tree, tree_tmp)
        os.replace(tree_tmp, tree_out)
        outputs.append(tree_out)
    if "clu" in formats:
        clu_out = out_name + ".clu"
        os.makedirs(os.path.dirname(clu_out) or ".", exist_ok=True)
        _write_clu_from_tree(winner_tree, clu_out)
        outputs.append(clu_out)

    # Higher-order shards also record the state-level tree, which is the partition the
    # run actually found: the physical one is a projection that repeats a node id across
    # modules. Published alongside so a distributed higher-order analysis is recoverable
    # at all (#906).
    winner_states_tree = winner_shard.get("best_states_tree_file", "")
    if winner_states_tree:
        states_tree = _resolve_best_tree(shard_path, winner_states_tree)
        if not os.path.isfile(states_tree):
            raise MergeError(
                f"winning trial's state-level tree does not exist or is not a file: "
                f"'{states_tree}'"
            )
        if "tree" in formats:
            states_tree_out = out_name + "_states.tree"
            states_tmp = states_tree_out + ".tmp"
            shutil.copyfile(states_tree, states_tmp)
            os.replace(states_tmp, states_tree_out)
            outputs.append(states_tree_out)
        if "clu" in formats:
            states_clu_out = out_name + "_states.clu"
            _write_clu_from_tree(states_tree, states_clu_out, states=True)
            outputs.append(states_clu_out)

    return {
        "trial": int(winner["trial"]),
        "codelength": float(winner["codelength"]),
        "winner_tree": winner_tree,
        "num_shards": len(shards),
        "num_trials": len(covered),
        "missing": missing,
        "outputs": outputs,
    }


def _parse_formats(value: str) -> list[str]:
    return [f.strip() for f in value.split(",") if f.strip()]


def _main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="python -m infomap.merge",
        description="Merge distributed Infomap trial-results shards into final output.",
    )
    parser.add_argument(
        "patterns",
        nargs="+",
        help="Shard result JSON files or glob patterns (comma lists allowed).",
    )
    parser.add_argument(
        "--out-name",
        required=True,
        help="Output basename; writes <out-name>.tree / .clu.",
    )
    parser.add_argument(
        "--output",
        default="tree,clu",
        type=_parse_formats,
        help="Comma-separated output formats (tree, clu). Default: tree,clu.",
    )
    parser.add_argument(
        "--require-complete-trials",
        action="store_true",
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
