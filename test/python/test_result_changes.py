"""Shape checks on RESULT_CHANGES.csv, the list of intended result changes.

Harnesses that compare Infomap versions read this file, so a row that a plain
CSV reader cannot use, or a flag the CLI does not have, is caught here.
"""

from __future__ import annotations

import csv
import re
from pathlib import Path

import pytest

pytestmark = pytest.mark.fast

REPO_ROOT = Path(__file__).resolve().parents[2]
RESULT_CHANGES = REPO_ROOT / "RESULT_CHANGES.csv"
CATALOG = REPO_ROOT / "src" / "io" / "ParameterCatalog.cpp"

COLUMNS = ["version", "flags", "changes", "description", "issue", "pr"]
CHANGES = {
    "codelength",
    "partition",
    "module-ids",
    "levels",
    "output",
    "warning",
    "error",
    "runs",
}
VERSION = re.compile(r"^(unreleased|\d+\.\d+\.\d+)$")
NUMBERS = re.compile(r"^\d+( \d+)*$")


def _rows() -> list[dict[str, str]]:
    with RESULT_CHANGES.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        assert reader.fieldnames == COLUMNS
        return list(reader)


def test_rows_use_the_documented_values() -> None:
    long_names = set(re.findall(r'\.longName\("([a-z0-9-]+)"\)', CATALOG.read_text()))
    assert long_names, "no long option names found in the parameter catalog"
    rows = _rows()
    assert rows
    for line, row in enumerate(rows, start=2):
        assert None not in row, f"line {line}: too many fields"
        assert VERSION.match(row["version"]), f"line {line}: version"
        for flag in row["flags"].split():
            assert flag.startswith("--"), f"line {line}: use the long flag, not {flag}"
            assert flag[2:] in long_names, f"line {line}: unknown flag {flag}"
        changes = row["changes"].split()
        assert changes, f"line {line}: changes is empty"
        assert set(changes) <= CHANGES, f"line {line}: changes {changes}"
        assert row["description"].strip(), f"line {line}: description is empty"
        assert row["issue"] == "" or NUMBERS.match(row["issue"]), f"line {line}: issue"
        assert NUMBERS.match(row["pr"]), f"line {line}: pr"
