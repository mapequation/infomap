"""The CI check that keeps reference oracle edits apart from src/ (#1261)."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

pytestmark = pytest.mark.fast

SCRIPTS = Path(__file__).resolve().parents[2] / "scripts"
if not (SCRIPTS / "check_reference_edits.py").is_file():
    pytest.skip("scripts not available in this layout", allow_module_level=True)
sys.path.insert(0, str(SCRIPTS))

from check_reference_edits import mixed  # noqa: E402


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
