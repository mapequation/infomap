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

from check_reference_edits import changed, mixed  # noqa: E402


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
