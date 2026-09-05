import json
import subprocess
import sys
import traceback
from pathlib import Path

from schema_validation import validate_json_schema


def run(infomap_bin, *args, cwd=None):
    return subprocess.run(
        [infomap_bin, *args],
        check=False,
        text=True,
        capture_output=True,
        cwd=str(cwd) if cwd is not None else None,
    )


def make_workdir(path: Path) -> Path:
    # Each test runs from its own directory with relative paths, so Infomap's
    # output-name derivation from the input file works the same on every OS
    # (an absolute Windows path would not be stripped to a bare basename).
    path.mkdir(parents=True, exist_ok=True)
    (path / "network.net").write_text(
        '*Vertices 2\n1 "one"\n2 "two"\n*Edges\n1 2 1\n', encoding="utf-8"
    )
    return path


def test_print_config_fingerprint_exits_without_output_directory(infomap_bin, work):
    make_workdir(work)

    result = run(
        infomap_bin, "network.net", "--silent", "--print-config-fingerprint", cwd=work
    )

    assert result.returncode == 0
    assert len(result.stdout.strip()) == 16
    assert result.stderr == ""


def test_missing_input_returns_input_exit_code(infomap_bin, work):
    make_workdir(work)

    result = run(infomap_bin, "missing.net", "out", "--silent", cwd=work)

    assert result.returncode == 2
    assert "Error:" in result.stderr


def test_no_overwrite_returns_output_exit_code(infomap_bin, work):
    make_workdir(work)
    out_dir = work / "out"
    out_dir.mkdir()
    (out_dir / "network.tree").write_text("existing\n", encoding="utf-8")

    result = run(
        infomap_bin, "network.net", "out", "--silent", "--no-overwrite", cwd=work
    )

    assert result.returncode == 3
    assert "Output file already exists" in result.stderr
    assert (out_dir / "network.tree").read_text(encoding="utf-8") == "existing\n"


def test_no_overwrite_preflight_writes_no_files_when_one_target_exists(
    infomap_bin, work
):
    make_workdir(work)
    out_dir = work / "out"
    out_dir.mkdir()
    # Only the .ftree target pre-exists; .tree and .clu do not.
    (out_dir / "network.ftree").write_text("existing\n", encoding="utf-8")

    result = run(
        infomap_bin,
        "network.net",
        "out",
        "--silent",
        "--tree",
        "--ftree",
        "--clu",
        "--no-overwrite",
        cwd=work,
    )

    assert result.returncode == 3
    assert "Output file already exists" in result.stderr
    # Pre-flight must run before any writing: no partial output set on disk.
    assert (out_dir / "network.ftree").read_text(encoding="utf-8") == "existing\n"
    assert not (out_dir / "network.tree").exists()
    assert not (out_dir / "network.clu").exists()


def test_no_overwrite_preflight_sees_a_shards_own_trial_paths(infomap_bin, work):
    # The plan enumerated `_trial_1..N` while the writers use the global
    # `trialOffset + i + 1`, so on a shard the two disagreed about every per-trial
    # filename. --no-overwrite then had nothing real to check and could only meet
    # the collision by running into it, after earlier artifacts were already
    # written -- which is the one thing the pre-flight exists to prevent.
    #
    # The *last* trial of the shard is the pre-existing file on purpose. With the
    # first one, the writer collides on its very first write and nothing has
    # reached disk yet, so the bug is invisible: the run exits 3 either way. At
    # trial 14 the buggy plan let the aggregate and trials 11 through 13 be
    # written before the refusal -- four files, under --no-overwrite.
    make_workdir(work)
    out_dir = work / "out"
    out_dir.mkdir()
    (out_dir / "network_trial_14.tree").write_text("existing\n", encoding="utf-8")

    result = run(
        infomap_bin,
        "network.net",
        "out",
        "--silent",
        "--seed",
        "42",
        "--trial-offset",
        "10",
        "-N4",
        "--print-all-trials",
        "-o",
        "tree",
        "--no-overwrite",
        cwd=work,
    )

    assert result.returncode == 3, result.stderr
    assert "Output file already exists" in result.stderr
    assert (out_dir / "network_trial_14.tree").read_text(
        encoding="utf-8"
    ) == "existing\n"
    # Nothing else reached disk, including the aggregate and the earlier trials.
    assert sorted(path.name for path in out_dir.iterdir()) == [
        "network_trial_14.tree"
    ], sorted(path.name for path in out_dir.iterdir())


def test_shard_trial_output_does_not_overwrite_its_own_cluster_input(infomap_bin, work):
    # The mirror consumer of the same plan: feeding a previous run's per-trial clu
    # back in as the initial partition, into the directory it came from. The
    # refusal is not subject to the overwrite policy, so a plan blind to the
    # offset meant this run destroyed its own input and exited 0.
    make_workdir(work)
    partition = work / "network_trial_11.clu"
    partition.write_text("1 1\n2 1\n", encoding="utf-8")
    original = partition.read_text(encoding="utf-8")

    result = run(
        infomap_bin,
        "network.net",
        ".",
        "--silent",
        "--seed",
        "42",
        "--cluster-data",
        "network_trial_11.clu",
        "--trial-offset",
        "10",
        "-N4",
        "--print-all-trials",
        "-o",
        "clu",
        cwd=work,
    )

    assert result.returncode == 3, result.stderr
    assert "Refusing to write output" in result.stderr
    assert partition.read_text(encoding="utf-8") == original


def make_state_workdir(path: Path) -> Path:
    # A higher-order input, so the run writes both `<name>.tree` and
    # `<name>_states.tree`. The file is named after the state half on purpose.
    path.mkdir(parents=True, exist_ok=True)
    (path / "net_states.tree").write_text(
        '*States\n1 1 "a"\n2 2 "b"\n3 1 "a"\n*Links\n1 2 1\n2 3 1\n',
        encoding="utf-8",
    )
    return path


def test_state_output_does_not_overwrite_its_own_input(infomap_bin, work):
    # #1018: the input-overwrite pre-flight saw only the physical artifact, so a
    # state-network run whose input was named `<out-name>_states.<ext>` destroyed
    # the input and exited 0. The check needs the higher-order classification,
    # which is only settled once the network is read -- so this exercises the
    # ordering inside RunSession::run, not just the plan.
    make_state_workdir(work)
    original = (work / "net_states.tree").read_text(encoding="utf-8")

    result = run(
        infomap_bin,
        "net_states.tree",
        ".",
        "--out-name",
        "net",
        "--silent",
        "-N1",
        cwd=work,
    )

    assert result.returncode == 3, result.stderr
    assert "Refusing to write output" in result.stderr
    assert (work / "net_states.tree").read_text(encoding="utf-8") == original
    assert not (work / "net.tree").exists()


def test_first_order_run_may_write_beside_a_states_named_input(infomap_bin, work):
    # The mirror of the case above: first-order input writes no `_states` half, so
    # the same command must still be allowed. The fix must not buy its refusal by
    # rejecting runs that would have worked.
    make_workdir(work)
    (work / "net_states.tree").write_text(
        "not an input to this run\n", encoding="utf-8"
    )

    result = run(
        infomap_bin,
        "network.net",
        ".",
        "--out-name",
        "net",
        "--silent",
        "-N1",
        cwd=work,
    )

    assert result.returncode == 0, result.stderr
    assert (work / "net.tree").exists()


def make_multilayer_workdir(path: Path) -> Path:
    # *Intra/*Inter multilayer input. The inter-layer links break the symmetry, so
    # reading it expands the undirected links to directed pairs and the run's flow
    # model flips to directed -- which is what the Pajek dump has to declare.
    path.mkdir(parents=True, exist_ok=True)
    (path / "ml.net").write_text(
        '*Vertices 3\n1 "i"\n2 "j"\n3 "k"\n'
        "*Intra\n1 1 2 1\n1 2 1 1\n2 1 3 1\n2 3 1 1\n"
        "*Inter\n1 1 2 0.4\n2 1 1 0.4\n",
        encoding="utf-8",
    )
    return path


def test_pajek_dump_of_a_higher_order_network_is_named_and_labelled_for_it(
    infomap_bin, work
):
    # The BeforeFlow artifacts used to be written before configureNetworkMode(),
    # so the Pajek dump of a higher-order network was named `<out-name>.net` --
    # the `_states_as_physical` variant was unreachable from a run -- and declared
    # `*Edges` while the run had already expanded the links to directed. Both came
    # from writing the artifact before the config that describes it was finished.
    make_multilayer_workdir(work)

    result = run(
        infomap_bin,
        "ml.net",
        "out",
        "--out-name",
        "ml",
        "--silent",
        "-o",
        "network",
        "-N1",
        cwd=work,
    )

    assert result.returncode == 0, result.stderr
    dump = work / "out" / "ml_states_as_physical.net"
    assert dump.exists()
    assert not (work / "out" / "ml.net").exists()

    text = dump.read_text(encoding="utf-8")
    assert "# State network as physical network" in text
    assert "*Arcs" in text
    assert "*Edges" not in text


def test_pajek_dump_of_a_first_order_network_is_unchanged(infomap_bin, work):
    # The control: first-order input keeps the bare name and the undirected label.
    make_workdir(work)

    result = run(
        infomap_bin,
        "network.net",
        "out",
        "--silent",
        "-o",
        "network",
        "-N1",
        cwd=work,
    )

    assert result.returncode == 0, result.stderr
    dump = work / "out" / "network.net"
    assert dump.exists()
    assert not (work / "out" / "network_states_as_physical.net").exists()

    text = dump.read_text(encoding="utf-8")
    assert "*Edges" in text
    assert "# State network as physical network" not in text


def test_default_headers_record_the_effective_seed(infomap_bin, work):
    # #1026: the header echoes the as-typed argument string, so a run on the default
    # seed published artifacts from which the seed was unrecoverable -- the one
    # parameter reproducibility actually hinges on. No --seed here on purpose.
    make_workdir(work)

    result = run(
        infomap_bin,
        "network.net",
        "out",
        "--silent",
        "-N2",
        "-o",
        "tree,json",
        cwd=work,
    )

    assert result.returncode == 0, result.stderr

    tree = (work / "out" / "network.tree").read_text(encoding="utf-8")
    assert "# seed 123" in tree
    # No shard offset in play, so the line carries the seed alone.
    assert "offset" not in tree.split("# seed")[1].split("\n")[0]

    data = json.loads((work / "out" / "network.json").read_text(encoding="utf-8"))
    assert data["seed"] == 123
    assert "trialOffset" not in data
    # The trial counts the text header has had since #906, which this output lacked.
    assert data["trials"] == 2
    assert data["numTrials"] == 2


def test_headers_record_the_trial_offset_of_a_shard(infomap_bin, work):
    # Per-trial seeds derive from base + offset + i, so the offset is part of the
    # answer and a shard's artifact has to carry it.
    make_workdir(work)

    result = run(
        infomap_bin,
        "network.net",
        "out",
        "--silent",
        "--seed",
        "42",
        "-N2",
        "--trial-offset",
        "10",
        "-o",
        "tree,json",
        cwd=work,
    )

    assert result.returncode == 0, result.stderr

    tree = (work / "out" / "network.tree").read_text(encoding="utf-8")
    assert "# seed 42 offset 10" in tree

    data = json.loads((work / "out" / "network.json").read_text(encoding="utf-8"))
    assert data["seed"] == 42
    assert data["trialOffset"] == 10


def test_every_artifact_records_the_same_base_seed(infomap_bin, work):
    # The serial loop moves Config::seedToRandomNumberGenerator to the current trial's
    # seed and only gives it back when the loop ends, while updateBestResult writes the
    # aggregate artifact from inside the loop. A header reading the live field recorded
    # 42, 43, 44 across the per-trial files, and the aggregate kept the last trial's
    # seed whenever the best trial was the last -- restoreBestResult then rewrites
    # nothing. One stable meaning instead: every artifact carries the run's base seed,
    # and a trial's own seed is base + offset + (trial - 1), with the trial in the
    # filename.
    make_workdir(work)

    result = run(
        infomap_bin,
        "network.net",
        "out",
        "--silent",
        "--seed",
        "42",
        "-N3",
        "--print-all-trials",
        "-o",
        "tree",
        cwd=work,
    )

    assert result.returncode == 0, result.stderr

    trees = sorted((work / "out").glob("*.tree"))
    assert len(trees) >= 4, [t.name for t in trees]

    seeds = {}
    for tree in trees:
        lines = [
            line
            for line in tree.read_text(encoding="utf-8").splitlines()
            if line.startswith("# seed ")
        ]
        assert len(lines) == 1, (tree.name, lines)
        seeds[tree.name] = lines[0]

    assert set(seeds.values()) == {"# seed 42"}, seeds


def test_parallel_trials_artifacts_record_the_base_seed(infomap_bin, work):
    # The parallel path writes through a worker Infomap rather than the main instance,
    # so it gets its own assertion. A worker does not inherit the main run's captured
    # base seed and its live seed field is the current trial's, so without the
    # propagation in runTrialsInParallel the per-trial files here reported 52, 53, 54,
    # 55 while the aggregate reported 42.
    #
    # A build without OpenMP warns and runs the same command serially, which asserts
    # the same invariant over the serial writer; the worker lines are covered by the
    # OpenMP build.
    make_workdir(work)

    result = run(
        infomap_bin,
        "network.net",
        "out",
        "--silent",
        "--seed",
        "42",
        "-N4",
        "--trial-offset",
        "10",
        "--parallel-trials",
        "--print-all-trials",
        "-o",
        "tree",
        cwd=work,
    )

    assert result.returncode == 0, result.stderr

    # The aggregate plus one file per trial, numbered by offset + trial index.
    names = sorted(tree.name for tree in (work / "out").glob("*.tree"))
    assert names == [
        "network.tree",
        "network_trial_11.tree",
        "network_trial_12.tree",
        "network_trial_13.tree",
        "network_trial_14.tree",
    ], names

    for name in names:
        text = (work / "out" / name).read_text(encoding="utf-8")
        assert "# seed 42 offset 10" in text, (name, text)


def test_overwrite_flag_is_removed(infomap_bin, work):
    make_workdir(work)

    result = run(infomap_bin, "network.net", "out", "--silent", "--overwrite", cwd=work)

    assert result.returncode == 1
    assert "Unrecognized option" in result.stderr


def test_run_manifest_contains_fingerprints_and_outputs(infomap_bin, work):
    make_workdir(work)

    result = run(
        infomap_bin,
        "network.net",
        "out",
        "--silent",
        "--manifest-json",
        "manifest.json",
        cwd=work,
    )

    assert result.returncode == 0, result.stderr
    data = json.loads((work / "manifest.json").read_text(encoding="utf-8"))
    validate_json_schema(data, "run-manifest.schema.json")
    assert len(data["config_fingerprint"]) == 16
    assert data["input"]["path"] == "network.net"
    assert data["input"]["size"] == (work / "network.net").stat().st_size
    assert any(output["path"].endswith("network.tree") for output in data["outputs"])


def main(argv):
    infomap_bin = argv[1]
    import tempfile

    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        for test in [
            test_print_config_fingerprint_exits_without_output_directory,
            test_missing_input_returns_input_exit_code,
            test_no_overwrite_returns_output_exit_code,
            test_no_overwrite_preflight_writes_no_files_when_one_target_exists,
            test_no_overwrite_preflight_sees_a_shards_own_trial_paths,
            test_shard_trial_output_does_not_overwrite_its_own_cluster_input,
            test_state_output_does_not_overwrite_its_own_input,
            test_first_order_run_may_write_beside_a_states_named_input,
            test_pajek_dump_of_a_higher_order_network_is_named_and_labelled_for_it,
            test_pajek_dump_of_a_first_order_network_is_unchanged,
            test_default_headers_record_the_effective_seed,
            test_headers_record_the_trial_offset_of_a_shard,
            test_every_artifact_records_the_same_base_seed,
            test_parallel_trials_artifacts_record_the_base_seed,
            test_overwrite_flag_is_removed,
            test_run_manifest_contains_fingerprints_and_outputs,
        ]:
            try:
                test(infomap_bin, tmp_path / test.__name__)
            except Exception as exc:
                failures += 1
                print(f"{test.__name__}: {exc}", file=sys.stderr)
                traceback.print_exc(file=sys.stderr)

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
