"""The 3.0 parameter cleanup policy (#755): schema, validation, coverage.

The policy lives in ``interfaces/parameters/overrides.json`` (``policy``
section), keyed by CLI flag with one decision per surface (cli/python/r/ts).
``scripts/parameter_catalog.py`` fail-loud-validates it when the catalog is
constructed (so the binding generator refuses to run on a broken policy),
and ``scripts/render_parameter_policy.py`` renders the surface matrix.

These tests exercise the validation with the real overrides file plus
deliberately broken variants, without needing the compiled engine: the
catalog is constructed from a minimal parameter list.
"""

from __future__ import annotations

import copy
import json
import sys
from pathlib import Path

import pytest


pytestmark = pytest.mark.fast

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "scripts"))

from parameter_catalog import ParameterCatalog  # noqa: E402

OVERRIDES = json.loads(
    (REPO_ROOT / "interfaces" / "parameters" / "overrides.json").read_text(
        encoding="utf-8"
    )
)

# A minimal stand-in for the C++ catalog: one parameter per policy entry
# (minus bindingOnly/cliOnly extras) plus a policy-less parameter, enough for
# the validator to resolve every flag it checks.
_POLICY_FLAGS = set(OVERRIDES["policy"]["parameters"])
_EXTRA_FLAGS = {
    entry["flag"]
    for entries in OVERRIDES.get("bindingOnly", {}).values()
    for entry in entries
    if entry["flag"].startswith("--")
} | set(OVERRIDES["policy"].get("cliOnlyParameters", []))


def _fake_parameters() -> list[dict]:
    # Include everything the other override sections (tiers, apiNotes)
    # validate against, so their fail-loud checks stay satisfied.
    tier_flags = {
        flag
        for tiers in OVERRIDES.get("tiers", {}).values()
        for flag in tiers.get("common", [])
    }
    flags = sorted((_POLICY_FLAGS - _EXTRA_FLAGS) | tier_flags | {"--seed"})
    return [
        {"long": flag, "short": "", "group": "Algorithm", "description": "x"}
        for flag in flags
    ]


def _catalog(overrides: dict) -> ParameterCatalog:
    return ParameterCatalog(_fake_parameters(), overrides)


def test_real_policy_validates_and_defaults_to_keep():
    catalog = _catalog(OVERRIDES)

    seed = next(param for param in catalog.parameters if param.flag == "--seed")
    for surface in OVERRIDES["policy"]["surfaces"]:
        assert seed.policy(surface) == {"action": "keep"}


def test_policy_accessor_returns_the_decision():
    catalog = _catalog(OVERRIDES)
    silent = next(param for param in catalog.parameters if param.flag == "--silent")

    decision = silent.policy("python")

    assert decision["action"] == "remove"
    assert "logging" in decision["replacement"]
    assert silent.policy("cli") == {"action": "keep"}


def test_unknown_flag_fails_loud():
    overrides = copy.deepcopy(OVERRIDES)
    overrides["policy"]["parameters"]["--not-a-flag"] = {
        "python": {"action": "remove", "replacement": "x"}
    }

    with pytest.raises(RuntimeError, match="unknown flag '--not-a-flag'"):
        _catalog(overrides)


def test_unknown_surface_fails_loud():
    overrides = copy.deepcopy(OVERRIDES)
    overrides["policy"]["parameters"]["--silent"]["fortran"] = {
        "action": "remove",
        "replacement": "x",
    }

    with pytest.raises(RuntimeError, match="unknown surface 'fortran'"):
        _catalog(overrides)


def test_unknown_action_fails_loud():
    overrides = copy.deepcopy(OVERRIDES)
    overrides["policy"]["parameters"]["--silent"]["python"] = {
        "action": "obliterate",
        "replacement": "x",
    }

    with pytest.raises(RuntimeError, match="unknown action 'obliterate'"):
        _catalog(overrides)


def test_non_keep_without_replacement_fails_loud():
    overrides = copy.deepcopy(OVERRIDES)
    del overrides["policy"]["parameters"]["--silent"]["python"]["replacement"]

    with pytest.raises(RuntimeError, match="missing the required replacement"):
        _catalog(overrides)


def test_alias_requires_a_known_target():
    overrides = copy.deepcopy(OVERRIDES)
    overrides["policy"]["parameters"]["--threads"]["cli"]["aliasOf"] = "--warp-speed"

    with pytest.raises(RuntimeError, match="aliases unknown flag"):
        _catalog(overrides)


def test_hidden_bindings_must_be_policy_hidden():
    # The policy is the umbrella: everything a generated surface already
    # hides (hiddenBindings) must carry an explicit hide decision.
    overrides = copy.deepcopy(OVERRIDES)
    del overrides["policy"]["parameters"]["--num-threads"]

    with pytest.raises(RuntimeError, match=r"hiddenBindings.ts hides --num-threads"):
        _catalog(overrides)


def test_matrix_covers_every_parameter_and_replacement():
    from render_parameter_policy import render

    catalog = _catalog(OVERRIDES)
    matrix = render(catalog)

    for param in catalog.parameters:
        assert f"`{param.flag}`" in matrix
    # Every non-keep decision surfaces its replacement guidance.
    for flag, per_surface in OVERRIDES["policy"]["parameters"].items():
        for decision in per_surface.values():
            if decision["action"] != "keep":
                assert decision["replacement"] in matrix
