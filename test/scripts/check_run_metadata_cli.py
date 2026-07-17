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
