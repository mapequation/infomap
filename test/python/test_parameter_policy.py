"""The 3.0 parameter cleanup policy (#755): schema, validation, coverage.

The policy is the single per-parameter, per-surface record in
``interfaces/parameters/overrides.json``: per-parameter entries take
precedence over group rules (one decision applied to a parameter list),
everything else defaults to ``keep``. Tier membership (``tier: common``),
kwarg inertness (``inertWithoutOutputDir``), and hidden generated surfaces
(``hide`` decisions) all live on the decisions -- there are no parallel
sections to keep consistent. ``scripts/parameter_catalog.py``
fail-loud-validates the policy when the catalog is constructed (so the
binding generator refuses to run on a broken policy), and
``scripts/render_parameter_policy.py`` renders the surface matrix.

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


def _policy_flags(overrides: dict) -> set[str]:
    policy = overrides["policy"]
    flags = set(policy["parameters"])
    for group in policy.get("groups", {}).values():
        flags.update(group.get("parameters", []))
    return flags


_EXTRA_FLAGS = {
    entry["flag"]
    for entries in OVERRIDES.get("bindingOnly", {}).values()
    for entry in entries
    if entry["flag"].startswith("--")
} | set(OVERRIDES["policy"].get("cliOnlyParameters", []))


def _overrides_section_flags(overrides: dict) -> set[str]:
    # Flags referenced by the names/defaults/docs sections. Many are keep-tier
    # (so absent from the policy decisions) but must still exist in the catalog
    # -- the catalog validates these sections too (issue #755).
    flags: set[str] = set()
    for section in ("names", "defaults", "docs"):
        flags.update(overrides.get(section, {}))
    return flags


def _fake_parameters() -> list[dict]:
    # A minimal stand-in for the C++ catalog: every flag the policy decides on
    # or the override sections reference (minus binding-only/CLI-only extras)
    # plus one policy-less parameter.
    flags = sorted(
        (
            (_policy_flags(OVERRIDES) | _overrides_section_flags(OVERRIDES))
            - _EXTRA_FLAGS
        )
        | {"--core-loop-limit"}
    )
    return [
        {"long": flag, "short": "", "group": "Algorithm", "description": "x"}
        for flag in flags
    ]


def _catalog(overrides: dict) -> ParameterCatalog:
    return ParameterCatalog(_fake_parameters(), overrides)


def test_real_policy_validates_and_defaults_to_keep():
    catalog = _catalog(OVERRIDES)

    plain = next(
        param for param in catalog.parameters if param.flag == "--core-loop-limit"
    )
    for surface in OVERRIDES["policy"]["surfaces"]:
        assert plain.policy(surface) == {"action": "keep"}
        assert plain.tier(surface) == "advanced"


def test_per_parameter_decisions_resolve():
    catalog = _catalog(OVERRIDES)
    silent = next(param for param in catalog.parameters if param.flag == "--silent")

    decision = silent.policy("python")

    assert decision["action"] == "remove"
    assert "logging" in decision["replacement"]
    assert silent.policy("cli") == {"action": "keep"}


def test_group_rules_resolve_and_carry_attributes():
    catalog = _catalog(OVERRIDES)
    tree = next(param for param in catalog.parameters if param.flag == "--tree")

    decision = tree.policy("python")

    assert decision["action"] == "args-only"
    assert tree.python_inert_without_outdir() is True
    # Other surfaces are untouched by the group.
    assert tree.policy("cli") == {"action": "keep"}


def test_tier_is_a_policy_attribute():
    catalog = _catalog(OVERRIDES)
    seed = next(param for param in catalog.parameters if param.flag == "--seed")

    assert seed.tier("python") == "common"
    assert seed.tier("r") == "advanced"
    assert seed.policy("python")["action"] == "keep"


def test_hidden_bindings_are_derived_from_hide_decisions():
    catalog = _catalog(OVERRIDES)

    assert "--num-threads" in catalog.hidden_bindings["ts"]
    assert "--threads" in catalog.hidden_bindings["ts"]
    visible = {param.flag for param in catalog.visible_parameters("ts")}
    assert "--num-threads" not in visible
    assert "--num-threads" in {
        param.flag for param in catalog.visible_parameters("python")
    }


def test_every_deprecated_option_field_carries_a_versioned_directive():
    """RELEASING.md requires a ``.. deprecated:: <version>`` note on every deprecated
    member, and the published ``Options`` reference -- which the package docstring
    points at as the parameter reference -- carried none for any field (#915).

    Asserted on the rendered docstring rather than on the generator, since that is what
    a user reads through ``inspect.getdoc``.
    """
    import inspect
    import re as _re

    from infomap import Options
    from infomap._options import _OPTION_TABLE

    # getdoc dedents, so a field heading sits at column 0 and its body is indented.
    doc = inspect.getdoc(Options) or ""
    blocks: dict[str, list[str]] = {}
    current = None
    for line in doc.splitlines():
        heading = _re.match(r"^(\w+) : ", line)
        if heading:
            current = heading.group(1)
            blocks[current] = []
        elif current is not None:
            blocks[current].append(line)

    deprecated_actions = {"remove", "args-only", "deprecate"}
    # _OPTION_TABLE is catalog-driven, so it excludes include_self_links: that field
    # is a binding-only compatibility alias handled separately. It is also the one
    # deprecated field on this surface whose directive is emitted by hand, so leaving
    # it out let the only case that could regress pass unchecked (#915).
    expected = sorted(
        [
            name
            for name, spec in _OPTION_TABLE.items()
            if spec.action in deprecated_actions
        ]
        + ["include_self_links"]
    )
    assert "include_self_links" in expected
    assert expected, "no deprecated fields to check; the policy shape changed"

    # Anchored to the start of an indented line, not a substring search: Sphinx only
    # parses a directive that opens its own line, so prose like "See .. deprecated::
    # 2.15" reads as a directive to `in` while rendering as plain text. The version
    # is captured from the rest of that line, which is also what catches a note
    # folded onto it -- Sphinx reads everything after the directive name as the
    # version, so a folded sentence renders as the version string.
    directive_pattern = _re.compile(r"^\s+\.\. deprecated::(?P<version>.*)$")

    for name in expected:
        assert name in blocks, f"{name} missing from the Options reference"
        directives = [
            match
            for match in (directive_pattern.match(line) for line in blocks[name])
            if match
        ]
        assert directives, f"{name} has no .. deprecated:: directive"
        for match in directives:
            version = match.group("version").strip()
            assert version, f"{name} has a directive with no version"
            assert " " not in version, (
                f"{name} folded its note onto the directive line, which Sphinx would "
                f"render as the version: {version!r}"
            )
            # The exact release, not merely "some token": RELEASING.md fixes this
            # deprecation wave at 2.15, and a nonempty-check accepted 2.14 or any
            # other string while claiming to protect that. Written literally rather
            # than imported from the generator so the two cannot drift together --
            # a future wave should have to change this line deliberately.
            assert version == "2.15", (
                f"{name} carries version {version!r}; this deprecation wave is 2.15"
            )


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


def test_unknown_tier_fails_loud():
    overrides = copy.deepcopy(OVERRIDES)
    overrides["policy"]["parameters"]["--seed"]["python"]["tier"] = "premium"

    with pytest.raises(RuntimeError, match="unknown tier 'premium'"):
        _catalog(overrides)


def test_tier_on_removed_parameter_fails_loud():
    overrides = copy.deepcopy(OVERRIDES)
    overrides["policy"]["parameters"]["--silent"]["python"]["tier"] = "common"

    with pytest.raises(RuntimeError, match="tiers only apply to kept"):
        _catalog(overrides)


def test_cli_only_parameter_rejects_binding_decisions():
    overrides = copy.deepcopy(OVERRIDES)
    overrides["policy"]["parameters"]["--pretty"]["python"] = {
        "action": "remove",
        "replacement": "x",
    }

    with pytest.raises(RuntimeError, match="declares it CLI-only"):
        _catalog(overrides)


def test_conflicting_group_claims_fail_loud():
    overrides = copy.deepcopy(OVERRIDES)
    overrides["policy"]["groups"]["duplicate"] = {
        "parameters": ["--tree"],
        "python": {"action": "remove", "replacement": "x"},
    }

    with pytest.raises(RuntimeError, match="both decide --tree/python"):
        _catalog(overrides)


def test_group_with_unknown_flag_fails_loud():
    overrides = copy.deepcopy(OVERRIDES)
    overrides["policy"]["groups"]["python-output-artifacts"]["parameters"].append(
        "--bogus"
    )

    with pytest.raises(RuntimeError, match="unknown flag '--bogus'"):
        _catalog(overrides)


def test_matrix_covers_every_parameter_and_replacement():
    from render_parameter_policy import render

    catalog = _catalog(OVERRIDES)
    matrix = render(catalog)

    for param in catalog.parameters:
        assert f"`{param.flag}`" in matrix
    # Non-existent surface cells render as an em dash, not a fabricated keep.
    assert "| `--pretty` | CLI-only (hidden) | **remove** | — | — | — |" in matrix
    # Every non-keep decision surfaces its replacement guidance.
    policy = OVERRIDES["policy"]
    decisions = [
        decision
        for per_surface in policy["parameters"].values()
        for decision in per_surface.values()
    ] + [
        group[surface]
        for group in policy.get("groups", {}).values()
        for surface in group
        if surface != "parameters"
    ]
    for decision in decisions:
        if decision["action"] != "keep":
            assert decision["replacement"] in matrix
