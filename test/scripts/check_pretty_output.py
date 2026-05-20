import re
import subprocess
import sys
import tempfile
from pathlib import Path


def run(*args):
    return subprocess.run(
        args,
        capture_output=True,
        text=True,
        check=True,
    ).stdout


def main():
    cli = sys.argv[1]
    repo_root = Path(sys.argv[2])
    network = repo_root / "examples" / "networks" / "modular_wd.net"
    cluster_network = repo_root / "examples" / "networks" / "twotriangles.net"
    cluster_tree = (
        repo_root / "test" / "fixtures" / "clusters" / "twotriangles_three_level.tree"
    )

    pretty = run(cli, str(network), "-0", "--directed", "--seed", "1", "--pretty")
    assert "\x1b[" not in pretty
    for expected in ("Network", "Flow", "Optimization", "Levels"):
        assert expected in pretty
    assert re.search(r"Total\s+7\s+25", pretty)
    for legacy in (
        "Parsing vertices...",
        "-------------------------------------",
        "Per level number of modules",
        "Initiated to codelength",
    ):
        assert legacy not in pretty
    assert re.search(r"Recursive.*0%", pretty)

    verbose_pretty = run(
        cli, str(network), "-0", "--directed", "--seed", "1", "--pretty", "-vv"
    )
    assert "Run Infomap..." in verbose_pretty

    silent_pretty = run(
        cli, str(network), "-0", "--directed", "--seed", "1", "--pretty", "--silent"
    )
    assert silent_pretty == ""

    cluster_pretty = run(
        cli,
        str(cluster_network),
        "-0",
        "--cluster-data",
        str(cluster_tree),
        "--seed",
        "1",
        "--pretty",
    )
    for legacy in (
        "Init partition from file",
        "Initiated to codelength",
        "done! Generated",
    ):
        assert legacy not in cluster_pretty

    with tempfile.TemporaryDirectory() as tmpdir:
        output_pretty = run(
            cli,
            str(network),
            tmpdir,
            "--out-name",
            "test",
            "--tree",
            "--directed",
            "--seed",
            "1",
            "--pretty",
        )
        assert Path(tmpdir, "test.tree").is_file()
        assert "Output" in output_pretty
        assert "Write tree to" not in output_pretty
        assert "done!" not in output_pretty

    with tempfile.TemporaryDirectory() as tmpdir:
        output_pretty = run(
            cli,
            str(network),
            tmpdir,
            "--out-name",
            "test",
            "--tree",
            "--ftree",
            "--clu",
            "--directed",
            "--seed",
            "1",
            "--pretty",
        )
        assert Path(tmpdir, "test.tree").is_file()
        assert Path(tmpdir, "test.ftree").is_file()
        assert Path(tmpdir, "test.clu").is_file()
        assert "3 files ->" in output_pretty
        assert ".{tree,ftree,clu}" in output_pretty
        assert "flow tree ->" not in output_pretty

    with tempfile.TemporaryDirectory() as tmpdir:
        output_pretty = run(
            cli,
            str(network),
            tmpdir,
            "--out-name",
            "test",
            "--output",
            "clu,tree,ftree,newick,json,csv,network,states,flow",
            "--directed",
            "--seed",
            "1",
            "--pretty",
        )
        for filename in (
            "test.clu",
            "test.csv",
            "test.ftree",
            "test.json",
            "test.net",
            "test.nwk",
            "test.tree",
            "test_flow.net",
            "test_states.net",
        ):
            assert Path(tmpdir, filename).is_file()
        assert "state network ->" in output_pretty
        assert "Pajek network ->" in output_pretty
        assert "flow network ->" in output_pretty
        assert "6 files ->" in output_pretty
        assert ".{tree,ftree,nwk,json,csv,clu}" in output_pretty
        assert "Writing " not in output_pretty

    state_network = repo_root / "examples" / "networks" / "states.net"
    with tempfile.TemporaryDirectory() as tmpdir:
        output_pretty = run(
            cli,
            str(state_network),
            tmpdir,
            "--out-name",
            "test",
            "--output",
            "clu,tree,ftree,newick,json,csv,network,states,flow",
            "--seed",
            "1",
            "--pretty",
        )
        for filename in (
            "test.clu",
            "test.csv",
            "test.ftree",
            "test.json",
            "test.net",
            "test.nwk",
            "test.tree",
            "test_states.clu",
            "test_states.csv",
            "test_states.ftree",
            "test_states.json",
            "test_states.net",
            "test_states.nwk",
            "test_states.tree",
            "test_states_as_physical_flow.net",
        ):
            assert Path(tmpdir, filename).is_file()
        assert "flow state network as Pajek ->" in output_pretty
        assert "12 files ->" in output_pretty
        assert "test.{tree,ftree,nwk,json,csv,clu}" in output_pretty
        assert "test_states.{tree,ftree,nwk,json,csv,clu}" in output_pretty
        assert "test.{tree,ftree,nwk,json,csv,clu,states" not in output_pretty
        assert "Writing " not in output_pretty

    plain = run(cli, str(network), "-0", "--directed", "--seed", "1")
    assert "Parsing vertices..." in plain
    assert "-------------------------------------" in plain
    assert "Per level number of modules" in plain


if __name__ == "__main__":
    main()
