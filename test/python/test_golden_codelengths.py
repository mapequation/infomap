"""Golden-codelength parity: the Python API must reproduce the CLI-generated
manifest (test/fixtures/expected/golden-codelengths.json) for the same network
and the same raw CLI flag string. Together with the ctest freshness gate (CLI)
and the R testthat suite, this pins codelength three-way across surfaces.

Regenerate the manifest with `make build-golden-codelengths` after an
intentional algorithm/codelength change.
"""

import json
from pathlib import Path

import pytest
from infomap import Infomap

REPO_ROOT = Path(__file__).resolve().parents[2]
MANIFEST = REPO_ROOT / "test/fixtures/expected/golden-codelengths.json"
ENTRIES = json.loads(MANIFEST.read_text(encoding="utf-8"))


@pytest.mark.parametrize("entry", ENTRIES, ids=[e["id"] for e in ENTRIES])
def test_golden_codelength(entry):
    im = Infomap(args=entry["flags"])
    im.read_file(str(REPO_ROOT / entry["network"]))
    result = im.run()

    # Tolerance matches the C++ suite (checkApproxCodelength); counts are exact.
    assert result.codelength == pytest.approx(entry["codelength"], abs=1e-10)
    assert result.num_top_modules == entry["num_top_modules"]
    assert result.num_levels == entry["num_levels"]
