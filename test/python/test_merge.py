"""Unit tests for the distributed-trial merge tool (infomap.merge).

These use synthetic shard JSON + .tree fixtures, so they exercise the full merge
logic (selection, validation, clu derivation) without needing the compiled
extension or running Infomap.
"""

import json
from pathlib import Path

import pytest
from infomap.merge import MergeError, MergeSummary, merge_trial_results


def _write_tree(path, modules):
    """Write a minimal .tree file. `modules` maps node_id -> top module."""
    lines = ["# v-test", "# path flow name node_id"]
    flow = round(1.0 / max(len(modules), 1), 6)
    # Track a within-module leaf counter for the path's second component.
    leaf_in_module = {}
    for node_id, module in sorted(modules.items()):
        leaf_in_module[module] = leaf_in_module.get(module, 0) + 1
        path_str = f"{module}:{leaf_in_module[module]}"
        lines.append(f'{path_str} {flow} "n{node_id}" {node_id}')
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _shard(
    tmp_path, name, *, offset, trials, best_tree_modules, net_fp="NET", cfg_fp="CFG"
):
    """Write a shard results JSON + its best-tree file; return the json path.

    `trials` is a list of (global_index, codelength) tuples. The best tree is
    written for the lowest-codelength trial in this shard.
    """
    best_trial = min(trials, key=lambda t: (t[1], t[0]))
    tree_name = f"{name}_trial_{best_trial[0]}.tree"
    _write_tree(tmp_path / tree_name, best_tree_modules)
    data = {
        "network_fingerprint": net_fp,
        "config_fingerprint": cfg_fp,
        "infomap_version": "test",
        "base_seed": 1,
        "trial_offset": offset,
        "num_trials": len(trials),
        "best_tree_file": tree_name,  # relative to the shard json dir
        "trials": [
            {
                "trial": idx,
                "seed": 1 + idx,
                "codelength": cl,
                "num_top_modules": 2,
                "num_levels": 2,
                "thread": 0,
                "time_s": 0.1,
            }
            for idx, cl in trials
        ],
    }
    json_path = tmp_path / f"{name}.json"
    json_path.write_text(json.dumps(data), encoding="utf-8")
    return str(json_path)


def test_merge_selects_global_best_and_writes_outputs(tmp_path):
    # Shard A covers [0,2) with best codelength 6.5; shard B covers [2,4) with 6.1.
    _shard(
        tmp_path,
        "a",
        offset=0,
        trials=[(0, 6.5), (1, 6.7)],
        best_tree_modules={1: 1, 2: 1, 3: 2, 4: 2},
    )
    _shard(
        tmp_path,
        "b",
        offset=2,
        trials=[(2, 6.1), (3, 6.3)],
        best_tree_modules={1: 1, 2: 2, 3: 2, 4: 2},
    )  # the winning tree

    out = str(tmp_path / "final")
    summary = merge_trial_results([str(tmp_path / "*.json")], out_name=out)

    assert summary["trial"] == 2  # global best (codelength 6.1)
    assert summary["codelength"] == pytest.approx(6.1)
    assert summary["num_shards"] == 2
    assert summary["num_trials"] == 4
    assert summary["missing"] == []

    # tree is a verbatim copy of the winner; clu derives node -> top module.
    tree_text = (tmp_path / "final.tree").read_text()
    assert '"n1" 1' in tree_text  # winner tree content present
    clu_lines = [
        line
        for line in (tmp_path / "final.clu").read_text().splitlines()
        if line and not line.startswith("#")
    ]
    clu = {int(line.split()[0]): int(line.split()[1]) for line in clu_lines}
    assert clu == {1: 1, 2: 2, 3: 2, 4: 2}  # winner's modules


def test_merge_ties_break_on_lowest_global_index(tmp_path):
    _shard(tmp_path, "a", offset=0, trials=[(0, 6.0)], best_tree_modules={1: 1, 2: 1})
    _shard(tmp_path, "b", offset=1, trials=[(1, 6.0)], best_tree_modules={1: 2, 2: 2})
    summary = merge_trial_results(
        [str(tmp_path / "*.json")], out_name=str(tmp_path / "out")
    )
    assert summary["trial"] == 0  # equal codelength -> lowest global index wins


def test_merge_refuses_config_fingerprint_mismatch(tmp_path):
    _shard(
        tmp_path,
        "a",
        offset=0,
        trials=[(0, 6.0)],
        best_tree_modules={1: 1},
        cfg_fp="CFG1",
    )
    _shard(
        tmp_path,
        "b",
        offset=1,
        trials=[(1, 6.0)],
        best_tree_modules={1: 1},
        cfg_fp="CFG2",
    )
    with pytest.raises(MergeError, match="config_fingerprint mismatch"):
        merge_trial_results([str(tmp_path / "*.json")], out_name=str(tmp_path / "o"))


def test_merge_publishes_the_state_level_artifacts(tmp_path):
    """A higher-order shard records both trees; both have to reach the merged output.

    The physical tree is a projection that repeats a node id across modules, so the .clu
    derived from it cannot express the partition — and it was the only thing merge wrote
    (#906).
    """
    shard = _shard(
        tmp_path, "s", offset=0, trials=[(0, 2.5)], best_tree_modules={1: 1, 2: 2}
    )
    data = json.loads(Path(shard).read_text())
    states_name = "s_trial_0_states.tree"
    (tmp_path / states_name).write_text(
        "# path flow name state_id node_id layer_id\n"
        '1:1 0.25 "a" 0 1 1\n'
        '1:2 0.25 "b" 1 2 1\n'
        '2:1 0.25 "a" 2 1 2\n'
        '2:2 0.25 "b" 3 2 2\n'
    )
    data["best_states_tree_file"] = states_name
    Path(shard).write_text(json.dumps(data))

    summary = merge_trial_results([shard], out_name=str(tmp_path / "out"))

    states_tree = tmp_path / "out_states.tree"
    states_clu = tmp_path / "out_states.clu"
    assert states_tree.exists()
    assert states_clu.exists()
    assert str(states_tree) in summary["outputs"]
    assert str(states_clu) in summary["outputs"]

    # Keyed on the state id, with the physical id and layer kept, the way Infomap writes
    # its own _states.clu. Node 1 appears twice, under different modules — which is
    # exactly what the physical clu cannot say.
    rows = [
        line.split()
        for line in states_clu.read_text().splitlines()
        if line and not line.startswith("#")
    ]
    assert rows[0] == ["0", "1", "0.25", "1", "1"]
    assert rows[2] == ["2", "2", "0.25", "1", "2"]


def test_clu_derivation_survives_a_space_in_a_node_name(tmp_path):
    """Rows are ``path flow "name" id``, and the name may contain spaces."""
    shard = _shard(
        tmp_path, "s", offset=0, trials=[(0, 2.5)], best_tree_modules={1: 1, 2: 2}
    )
    tree = tmp_path / json.loads(Path(shard).read_text())["best_tree_file"]
    tree.write_text(
        '# path flow name node_id\n1:1 0.5 "gene X, alias" 1\n2:1 0.5 "plain" 2\n'
    )

    merge_trial_results([shard], out_name=str(tmp_path / "out"))

    rows = [
        line.split()
        for line in (tmp_path / "out.clu").read_text().splitlines()
        if line and not line.startswith("#")
    ]
    assert rows == [["1", "1", "0.5"], ["2", "2", "0.5"]]


def test_merge_refuses_overlapping_shard_ranges(tmp_path):
    """Four shards that all ran trials 0-1 are eight executions, not eight trials.

    The completeness check looks for gaps in [0, max], which overlapping shards do not
    have, so --require-complete-trials reported success while the effective budget was a
    quarter of the intended one (#906).
    """
    shards = [
        _shard(
            tmp_path,
            f"s{i}",
            offset=0,
            trials=[(0, 2.5), (1, 2.4)],
            best_tree_modules={1: 1, 2: 1, 3: 2},
        )
        for i in range(4)
    ]

    with pytest.raises(MergeError, match="claimed by more than one shard"):
        merge_trial_results(
            shards, out_name=str(tmp_path / "out"), require_complete=True
        )


def test_overlapping_shard_ranges_warn_without_require_complete(tmp_path, capsys):
    """Without the flag the merge still runs, but says the shards overlap."""
    shards = [
        _shard(
            tmp_path,
            f"s{i}",
            offset=0,
            trials=[(0, 2.5), (1, 2.4)],
            best_tree_modules={1: 1, 2: 1, 3: 2},
        )
        for i in range(2)
    ]

    summary = merge_trial_results(shards, out_name=str(tmp_path / "out"))

    assert summary["num_trials"] == 2
    assert "claimed by more than one shard" in capsys.readouterr().err


def test_merge_refuses_shards_from_different_builds(tmp_path):
    """A different Infomap build can partition the same network differently.

    The field was recorded in every shard and never read, so shards from two builds
    merged silently -- a fixed bug changes results exactly as much as a changed option
    does (#906).
    """
    first = _shard(
        tmp_path, "a", offset=0, trials=[(0, 2.5)], best_tree_modules={1: 1, 2: 1, 3: 2}
    )
    second = _shard(
        tmp_path, "b", offset=1, trials=[(1, 2.4)], best_tree_modules={1: 1, 2: 2, 3: 2}
    )
    data = json.loads(Path(second).read_text())
    data["infomap_version"] = "some-other-build"
    Path(second).write_text(json.dumps(data))

    with pytest.raises(MergeError, match="infomap_version mismatch"):
        merge_trial_results([first, second], out_name=str(tmp_path / "out"))


def test_merge_refuses_network_fingerprint_mismatch(tmp_path):
    _shard(
        tmp_path,
        "a",
        offset=0,
        trials=[(0, 6.0)],
        best_tree_modules={1: 1},
        net_fp="NETA",
    )
    _shard(
        tmp_path,
        "b",
        offset=1,
        trials=[(1, 6.0)],
        best_tree_modules={1: 1},
        net_fp="NETB",
    )
    with pytest.raises(MergeError, match="network_fingerprint mismatch"):
        merge_trial_results([str(tmp_path / "*.json")], out_name=str(tmp_path / "o"))


def test_merge_refuses_empty_fingerprint(tmp_path):
    _shard(
        tmp_path, "a", offset=0, trials=[(0, 6.0)], best_tree_modules={1: 1}, net_fp=""
    )
    with pytest.raises(MergeError, match="empty network_fingerprint"):
        merge_trial_results([str(tmp_path / "a.json")], out_name=str(tmp_path / "o"))


def test_merge_missing_trials_warns_or_errors(tmp_path):
    # Covers [0,1) and [2,3) — global index 1 is missing.
    _shard(tmp_path, "a", offset=0, trials=[(0, 6.0)], best_tree_modules={1: 1})
    _shard(tmp_path, "b", offset=2, trials=[(2, 6.5)], best_tree_modules={1: 1})

    # Default: warn but succeed.
    summary = merge_trial_results(
        [str(tmp_path / "*.json")], out_name=str(tmp_path / "out")
    )
    assert summary["missing"] == [1]

    # Strict: raise.
    with pytest.raises(MergeError, match="missing"):
        merge_trial_results(
            [str(tmp_path / "*.json")],
            out_name=str(tmp_path / "out2"),
            require_complete=True,
        )


def test_merge_rejects_empty_best_tree_file(tmp_path):
    data = {
        "network_fingerprint": "N",
        "config_fingerprint": "C",
        "infomap_version": "t",
        "base_seed": 1,
        "trial_offset": 0,
        "num_trials": 1,
        "best_tree_file": "",
        "trials": [{"trial": 0, "codelength": 6.0}],
    }
    p = tmp_path / "a.json"
    p.write_text(json.dumps(data), encoding="utf-8")
    with pytest.raises(MergeError, match="no best tree file"):
        merge_trial_results([str(p)], out_name=str(tmp_path / "o"))


def test_merge_rejects_unsupported_format(tmp_path):
    _shard(tmp_path, "a", offset=0, trials=[(0, 6.0)], best_tree_modules={1: 1})
    with pytest.raises(MergeError, match="unsupported output format"):
        merge_trial_results(
            [str(tmp_path / "a.json")], out_name=str(tmp_path / "o"), formats=("ftree",)
        )


def test_merge_no_matching_files(tmp_path):
    with pytest.raises(MergeError, match="no shard files matched"):
        merge_trial_results(
            [str(tmp_path / "nope_*.json")], out_name=str(tmp_path / "o")
        )


def test_merge_summary_is_public():
    from infomap import merge

    assert "MergeSummary" in merge.__all__
    assert merge.MergeSummary is MergeSummary


def test_merge_rejects_non_object_trial_entry(tmp_path):
    json_path = _shard(
        tmp_path, "a", offset=0, trials=[(0, 6.0)], best_tree_modules={1: 1}
    )
    data = json.loads((tmp_path / "a.json").read_text(encoding="utf-8"))
    # A string entry contains the substring "trial", so it would pass a plain
    # `key not in trial` containment check.
    data["trials"].append("trial=1 codelength=6.5")
    (tmp_path / "a.json").write_text(json.dumps(data), encoding="utf-8")

    with pytest.raises(MergeError, match="non-object entry in 'trials'"):
        merge_trial_results([json_path], out_name=str(tmp_path / "o"))


def test_merge_accepts_pathlike_out_name(tmp_path):
    _shard(tmp_path, "a", offset=0, trials=[(0, 6.0)], best_tree_modules={1: 1})
    summary = merge_trial_results(
        [str(tmp_path / "a.json")], out_name=tmp_path / "final"
    )
    assert (tmp_path / "final.tree").is_file()
    assert (tmp_path / "final.clu").is_file()
    assert summary["outputs"] == [
        str(tmp_path / "final.tree"),
        str(tmp_path / "final.clu"),
    ]


def test_merge_accepts_generic_pathlike_patterns(tmp_path, fspath_only):
    shard_json = _shard(
        tmp_path, "a", offset=0, trials=[(0, 6.0)], best_tree_modules={1: 1}
    )

    summary = merge_trial_results(
        [fspath_only(shard_json)], out_name=fspath_only(tmp_path / "final")
    )

    assert summary["num_shards"] == 1
    assert (tmp_path / "final.tree").is_file()


def test_merge_pathlike_pattern_is_a_literal_path_not_a_comma_list(tmp_path):
    # A plain-string pattern keeps its CLI-parity comma splitting, but a
    # PathLike names exactly one path -- a comma in it is part of the path.
    shard_dir = tmp_path / "shards,batch-1"
    shard_dir.mkdir()
    shard_json = _shard(
        shard_dir, "a", offset=0, trials=[(0, 6.0)], best_tree_modules={1: 1}
    )

    summary = merge_trial_results([Path(shard_json)], out_name=str(tmp_path / "final"))

    assert summary["num_shards"] == 1
    assert (tmp_path / "final.tree").is_file()
