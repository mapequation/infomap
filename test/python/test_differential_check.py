"""The comparison logic of the differential checks in test/differential."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

pytestmark = pytest.mark.fast

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "test" / "differential"))

import differential  # noqa: E402

TREE = """# codelength 2.32 bits
1:1 0.2 "A" 1
1:2 0.2 "B" 2
2:1 0.3 "C" 3
2:2 0.3 "D" 4
"""
# The same modules, numbered the other way round.
TREE_REORDERED = """2:1 0.2 "A" 1
2:2 0.2 "B" 2
1:1 0.3 "C" 3
1:2 0.3 "D" 4
"""
TREE_MOVED = """1:1 0.2 "A" 1
1:2 0.2 "B" 2
1:3 0.3 "C" 3
2:1 0.3 "D" 4
"""


def _write(run_dir: Path, case_id: str, tree: str, codelength: float = 2.32) -> Path:
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / f"{case_id}.tree").write_text(tree)
    (run_dir / f"{case_id}.codelength").write_text(f"{codelength!r}\n")
    return run_dir


def _outcome(tree: str, codelength: float = 2.32):
    return differential.Outcome(
        codelength=codelength, nodes=differential._parse_tree(tree)
    )


def test_differences_name_what_changed():
    assert differential.differences(_outcome(TREE), _outcome(TREE)) == []
    assert differential.differences(_outcome(TREE), _outcome(TREE_REORDERED)) == [
        "order"
    ]
    assert differential.differences(_outcome(TREE), _outcome(TREE_MOVED, 2.4)) == [
        "codelength",
        "tree",
    ]
    assert differential.differences(
        _outcome(TREE), _outcome(TREE.replace("0.3", "0.31"))
    ) == ["flow"]
    assert differential.differences(
        _outcome(TREE), differential.Outcome(error="exit 2")
    ) == ["error"]


def test_state_and_first_order_trees_compare_on_state_ids():
    first_order = '1:1 0.5 "A" 7\n'
    states = '1:1 0.5 "A" 7 7\n'
    assert differential.differences(_outcome(first_order), _outcome(states)) == []


def _release_list(tmp_path: Path, rows: str) -> Path:
    path = tmp_path / "release_differences.tsv"
    path.write_text(f"# reference: v2.15.1\nid\tchanges\texplained_by\n{rows}")
    return path


def test_release_check_requires_every_difference_listed_and_explained(tmp_path, capsys):
    old = _write(tmp_path / "old", "twotriangles", TREE)
    new = _write(tmp_path / "new", "twotriangles", TREE_MOVED, 2.4)

    listed = _release_list(tmp_path, "twotriangles\tcodelength,tree\t#1075\n")
    assert differential.compare_release(old, new, listed) == 0

    assert differential.compare_release(old, new, _release_list(tmp_path, "")) == 1
    assert "not listed" in capsys.readouterr().out

    wrong_changes = _release_list(tmp_path, "twotriangles\torder\t#1075\n")
    assert differential.compare_release(old, new, wrong_changes) == 1

    # Not a RESULT_CHANGES.csv row.
    unexplained = _release_list(tmp_path, "twotriangles\tcodelength,tree\t#1\n")
    assert differential.compare_release(old, new, unexplained) == 1


def test_release_check_fails_on_a_stale_line(tmp_path, capsys):
    old = _write(tmp_path / "old", "twotriangles", TREE)
    new = _write(tmp_path / "new", "twotriangles", TREE)
    stale = _release_list(tmp_path, "twotriangles\tcodelength,tree\t#1075\n")
    assert differential.compare_release(old, new, stale) == 1
    assert "remove the line" in capsys.readouterr().out


def test_frontend_check_skips_and_lists(tmp_path):
    cli = _write(tmp_path / "cli", "twotriangles", TREE)
    js = _write(tmp_path / "js", "twotriangles", TREE_REORDERED)
    (js / "twotriangles-meta.skip").write_text("no input files\n")
    expected = tmp_path / "frontend_differences.tsv"

    expected.write_text("id\trun\tchanges\texplained_by\n")
    # An order difference is shown but not checked yet (#1325).
    assert differential.compare([cli, js], expected) == 0

    _write(js, "twotriangles", TREE_MOVED, 2.4)
    assert differential.compare([cli, js], expected) == 1
    expected.write_text(
        "id\trun\tchanges\texplained_by\ntwotriangles\tjs\tcodelength,tree\t#1325\n"
    )
    assert differential.compare([cli, js], expected) == 0
