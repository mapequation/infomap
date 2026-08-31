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
            test_state_output_does_not_overwrite_its_own_input,
            test_first_order_run_may_write_beside_a_states_named_input,
            test_pajek_dump_of_a_higher_order_network_is_named_and_labelled_for_it,
            test_pajek_dump_of_a_first_order_network_is_unchanged,
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
