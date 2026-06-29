"""Guard: the generated Infomap.__init__/run signature block stays present and
in sync with the parameter catalog. The byte-level freshness is enforced by
``scripts/generate_binding_options.py --check`` (wired into
``make test-binding-options-freshness``); this test guards the runtime
invariants.
"""

import inspect
from pathlib import Path

import pytest

from infomap import Infomap
from infomap._options import _OPTION_FIELD_NAMES

FACADE = Path(__file__).resolve().parents[2] / (
    "interfaces/python/src/infomap/_facade.py"
)
BEGIN = "# === BEGIN generated: Infomap option signatures"
END = "# === END generated ==="

NON_OPTION_INIT = {"self", "args", "pretty"}
NON_OPTION_RUN = {"self", "args", "initial_partition", "pretty"}


@pytest.mark.fast
def test_marker_block_present():
    text = FACADE.read_text(encoding="utf-8")
    assert BEGIN in text
    assert END in text
    assert text.index(BEGIN) < text.index(END)


@pytest.mark.fast
def test_init_options_match_catalog():
    params = set(inspect.signature(Infomap.__init__).parameters) - NON_OPTION_INIT
    assert params == set(_OPTION_FIELD_NAMES)


@pytest.mark.fast
def test_run_options_match_catalog():
    params = set(inspect.signature(Infomap.run).parameters) - NON_OPTION_RUN
    assert params == set(_OPTION_FIELD_NAMES)
