"""The CI check that keeps reference oracle edits apart from src/ (#1261)."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

pytestmark = pytest.mark.fast

SCRIPTS = Path(__file__).resolve().parents[2] / "scripts"
if not (SCRIPTS / "check_reference_edits.py").is_file():
    pytest.skip("scripts not available in this layout", allow_module_level=True)
sys.path.insert(0, str(SCRIPTS))

from check_reference_edits import changed, edited, mixed  # noqa: E402


def test_a_reference_edit_with_a_core_edit_is_refused():
    paths = [
        "test/python/reference_codelength.py",
        "src/core/InfomapBase.cpp",
        "test/python/test_flow.py",
    ]
    assert mixed(paths) == (
        ["test/python/reference_codelength.py"],
        ["src/core/InfomapBase.cpp"],
    )


@pytest.mark.parametrize(
    "paths",
    [
        ["test/python/reference_flow.py", "test/python/test_reference_flow.py"],
        ["src/core/InfomapBase.cpp", "test/python/test_flow.py"],
        ["interfaces/python/src/infomap/_facade.py", "test/python/reference_flow.py"],
        [],
    ],
)
def test_other_changes_pass(paths):
    assert mixed(paths) == ([], [])


def _git(repo: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
        cwd=repo,
        check=True,
        capture_output=True,
    )


@pytest.mark.parametrize(
    ("moved", "to"),
    [
        ("test/python/reference_flow.py", "test/python/flow_oracle.py"),
        ("src/core/Flow.cpp", "test/python/flow.cpp"),
    ],
)
def test_a_move_counts_at_both_paths(tmp_path, moved, to):
    other = (
        "src/core/Base.cpp"
        if moved.startswith("test")
        else "test/python/reference_codelength.py"
    )
    for path in (moved, other):
        (tmp_path / path).parent.mkdir(parents=True, exist_ok=True)
        (tmp_path / path).write_text("".join(f"line {i}\n" for i in range(50)))
    _git(tmp_path, "init", "-q", "-b", "base")
    _git(tmp_path, "add", ".")
    _git(tmp_path, "commit", "-qm", "base")
    _git(tmp_path, "switch", "-qc", "change")
    (tmp_path / to).parent.mkdir(parents=True, exist_ok=True)
    _git(tmp_path, "mv", moved, to)
    (tmp_path / other).write_text("changed\n")
    _git(tmp_path, "commit", "-qam", "change")
    references, core = mixed(changed("base", cwd=str(tmp_path)))
    assert references and core


def _write(repo: Path, path: str, text: str) -> None:
    (repo / path).parent.mkdir(parents=True, exist_ok=True)
    (repo / path).write_text(text)


def test_a_sync_of_master_into_a_branch_passes_and_a_branch_edit_does_not(tmp_path):
    # master edits an oracle and the core in separate pull requests; a long-lived
    # branch then merges master in, bringing both, which it did not edit itself.
    reference, core = "test/python/reference_flow.py", "src/core/Flow.cpp"
    _write(tmp_path, reference, "oracle\n")
    _write(tmp_path, core, "core\n")
    _git(tmp_path, "init", "-q", "-b", "master")
    _git(tmp_path, "add", ".")
    _git(tmp_path, "commit", "-qm", "base")
    _git(tmp_path, "switch", "-qc", "branch")
    _write(tmp_path, "src/core/Branch.cpp", "branch\n")
    _git(tmp_path, "add", ".")
    _git(tmp_path, "commit", "-qm", "branch work")
    _git(tmp_path, "switch", "-q", "master")
    _write(tmp_path, reference, "oracle, revised\n")
    _git(tmp_path, "commit", "-qam", "oracle edit")
    _write(tmp_path, core, "core, fixed\n")
    _git(tmp_path, "commit", "-qam", "core fix")

    _git(tmp_path, "switch", "-qc", "sync", "branch")
    _git(tmp_path, "merge", "-q", "--no-ff", "-m", "sync", "master")
    paths = changed("branch", cwd=str(tmp_path))
    assert mixed(paths) == ([reference], [core])
    assert mixed(edited(paths, "master", cwd=str(tmp_path))) == ([], [])

    _git(tmp_path, "switch", "-qc", "change", "branch")
    _write(tmp_path, reference, "oracle, edited on the branch\n")
    _write(tmp_path, "src/core/Branch.cpp", "branch, fixed\n")
    _git(tmp_path, "commit", "-qam", "both")
    paths = changed("branch", cwd=str(tmp_path))
    assert mixed(edited(paths, "master", cwd=str(tmp_path))) == (
        [reference],
        ["src/core/Branch.cpp"],
    )


def test_without_the_upstream_branch_every_changed_file_counts(tmp_path):
    _write(tmp_path, "a.txt", "a\n")
    _git(tmp_path, "init", "-q", "-b", "base")
    _git(tmp_path, "add", ".")
    _git(tmp_path, "commit", "-qm", "base")
    paths = ["test/python/reference_flow.py", "src/core/Flow.cpp"]
    assert edited(paths, "origin/no-such-branch", cwd=str(tmp_path)) == paths
