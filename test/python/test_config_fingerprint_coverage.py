"""The config fingerprint has to cover every option that can change the partition.

It is the only thing standing between a distributed run and merging shards that were run
with different algorithms, and it is enumerated by hand in ``canonicalConfigJson``. Four
options had been missed, so shards that disagreed on them fingerprinted identically and
merged without complaint (#906). This test walks the generated parameter catalog, so a
new option fails it until it is either covered or listed below with a reason.
"""

from __future__ import annotations

import json
from pathlib import Path

import infomap
import pytest

pytestmark = pytest.mark.fast

REPO_ROOT = Path(__file__).resolve().parents[2]
CATALOG = REPO_ROOT / "interfaces" / "js" / "generated" / "parameters.json"

# Groups whose options change what the algorithm computes. Input options are covered by
# the network fingerprint instead, except where they change interpretation rather than
# content -- those are asserted individually further down.
RESULT_AFFECTING_GROUPS = ("Algorithm", "Accuracy")

# Options whose fingerprint key is not the flag name with dashes swapped for underscores.
KEY_OVERRIDES = {
    "--to-nodes": "teleport_to_nodes",
    "--converge": "converge_trials",
    "--markov-time": "markov_time",
}

# Deliberately outside the fingerprint, with the reason. Anything else must be covered.
INTENTIONALLY_EXCLUDED = {
    "--num-trials": "names which slice of one budget a shard ran, not what it ran",
    "--trial-offset": "same: shards differ here by construction",
}


def _fingerprint_config(tmp_path: Path) -> dict:
    """Run Infomap once and return the canonical config object from its manifest."""
    manifest = tmp_path / "manifest.json"
    im = infomap.Infomap(
        silent=True,
        options=infomap.Options(manifest_json=str(manifest)),
    )
    im.add_links(((1, 2), (2, 3), (3, 1)))
    im.run()
    return json.loads(manifest.read_text())["config"]


def _flag_to_key(flag: str) -> str:
    return KEY_OVERRIDES.get(flag, flag.lstrip("-").replace("-", "_"))


def test_every_result_affecting_option_is_in_the_fingerprint(tmp_path):
    config = _fingerprint_config(tmp_path)
    catalog = json.loads(CATALOG.read_text())

    uncovered = []
    for parameter in catalog:
        flag = parameter["long"]
        if parameter["group"] not in RESULT_AFFECTING_GROUPS:
            continue
        if flag in INTENTIONALLY_EXCLUDED:
            continue
        if _flag_to_key(flag) not in config:
            uncovered.append(f"{flag} (expected key {_flag_to_key(flag)!r})")

    assert not uncovered, (
        "these options can change the partition but are missing from the config "
        "fingerprint, so two shards that disagree on them would merge:\n  "
        + "\n  ".join(sorted(uncovered))
        + "\n\nAdd them to canonicalConfigJson, or list them in "
        "INTENTIONALLY_EXCLUDED with the reason."
    )


def test_interpretation_changing_input_options_are_in_the_fingerprint(tmp_path):
    """Input options that change how the same file is read, not which file is read.

    The network fingerprint covers the file's content, so most Input options need no
    entry -- but these decide what the engine does with it, and both were missing.
    """
    config = _fingerprint_config(tmp_path)

    for key in (
        "cluster_data",
        "cluster_data_is_hard",
        "assign_to_neighbouring_module",
    ):
        assert key in config, f"{key} is missing from the config fingerprint"
