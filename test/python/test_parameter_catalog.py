import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))

from parameter_catalog import ParameterCatalog  # noqa: E402


def test_parameter_catalog_classifies_binding_render_policies():
    parameters = [
        {
            "long": "--silent",
            "short": "",
            "description": "Suppress console output.",
            "group": "Output",
            "required": False,
            "advanced": False,
            "incremental": False,
            "default": False,
        },
        {
            "long": "--num-trials",
            "short": "-N",
            "description": "Run this many independent trials.",
            "group": "Accuracy",
            "required": True,
            "advanced": False,
            "incremental": False,
            "longType": "integer",
            "default": "1",
        },
        {
            "long": "--verbose",
            "short": "-v",
            "description": "Increase console verbosity.",
            "group": "Output",
            "required": False,
            "advanced": False,
            "incremental": True,
            "default": False,
            "renderPolicy": "repeated_short",
        },
        {
            "long": "--output",
            "short": "-o",
            "description": "Write selected output formats.",
            "group": "Output",
            "required": True,
            "advanced": False,
            "incremental": False,
            "longType": "list",
            "default": "",
            "choices": ["clu", "tree"],
            "renderPolicy": "comma_list",
        },
    ]
    overrides = {
        # Surface knowledge (names/defaults/docs) lives in overrides, not the
        # engine catalog; the accessors read it from here.
        "names": {"--verbose": {"python": "verbosity_level", "r": "verbosity_level"}},
        "defaults": {
            "--num-trials": {"python": "1", "r": "1L"},
            "--verbose": {"python": "1", "r": "1L"},
        },
        "docs": {"--verbose": {"python": "Verbosity level on the console."}},
        "bindingOnly": {
            "python": [
                {
                    "name": "include_self_links",
                    "flag": "--include-self-links",
                    "type": "bool | None",
                    "default": "None",
                    "reason": "Deprecated compatibility alias.",
                }
            ],
            "ts": [
                {
                    "name": "help",
                    "flag": "--help",
                    "type": 'boolean | "advanced"',
                    "reason": "Existing TypeScript argument helper exposes CLI about flags.",
                }
            ],
        },
    }

    catalog = ParameterCatalog(parameters, overrides)
    by_flag = {parameter.flag: parameter for parameter in catalog.parameters}

    assert by_flag["--silent"].render_policy == "flag"
    assert by_flag["--num-trials"].render_policy == "value"
    assert by_flag["--verbose"].render_policy == "repeated_short"
    assert by_flag["--output"].render_policy == "comma_list"
    assert by_flag["--verbose"].name("python") == "verbosity_level"
    assert by_flag["--verbose"].name("r") == "verbosity_level"
    assert by_flag["--verbose"].name("ts") == "verbose"
    assert by_flag["--output"].choices == ["clu", "tree"]
    assert by_flag["--num-trials"].python_default_value() == "1"
    assert by_flag["--num-trials"].r_default() == "1L"
    assert (
        by_flag["--verbose"].python_doc_description()
        == "Verbosity level on the console."
    )
    assert (
        catalog.binding_only_entry("python", "include_self_links").render_policy
        == "deprecated_alias"
    )
    assert catalog.binding_only_entry("ts", "help").render_policy == "binding_only"


def _minimal_param(flag, group="Accuracy", **extra):
    param = {
        "long": flag,
        "short": "",
        "description": f"Test parameter {flag}.",
        "group": group,
        "required": False,
        "advanced": False,
        "incremental": False,
        "default": False,
    }
    param.update(extra)
    return param


def _tier_policy(*flags):
    return {
        "vocabulary": {"keep": "keep"},
        "surfaces": ["cli", "python", "r", "ts"],
        "default": "keep",
        "parameters": {
            flag: {"python": {"action": "keep", "tier": "common"}} for flag in flags
        },
    }


def test_parameter_tier_defaults_to_advanced_and_reads_policy():
    parameters = [
        _minimal_param("--seed", required=True, longType="integer"),
        _minimal_param("--two-level"),
        _minimal_param("--core-loop-limit", required=True, longType="integer"),
    ]
    overrides = {"policy": _tier_policy("--seed", "--two-level")}

    catalog = ParameterCatalog(parameters, overrides)
    tiers = {param.flag: param.tier("python") for param in catalog.parameters}

    assert tiers == {
        "--seed": "common",
        "--two-level": "common",
        "--core-loop-limit": "advanced",
    }
    # Tier declarations are per surface; an undeclared surface is all-advanced.
    assert all(param.tier("r") == "advanced" for param in catalog.parameters)


def test_parameter_catalog_rejects_unknown_tier_flag():
    import pytest

    parameters = [_minimal_param("--seed", required=True, longType="integer")]
    overrides = {"policy": _tier_policy("--seeed")}

    with pytest.raises(RuntimeError, match=r"--seeed"):
        ParameterCatalog(parameters, overrides)


def test_parameter_catalog_rejects_unknown_tier_name():
    import pytest

    parameters = [_minimal_param("--seed", required=True, longType="integer")]
    overrides = {"policy": _tier_policy("--seed")}
    overrides["policy"]["parameters"]["--seed"]["python"]["tier"] = "comon"

    with pytest.raises(RuntimeError, match="comon"):
        ParameterCatalog(parameters, overrides)


def test_python_common_tier_matches_issue_738_decision(test_paths):
    import json

    overrides = json.loads(
        (test_paths.repo_root / "interfaces" / "parameters" / "overrides.json")
        .read_text(encoding="utf-8")
    )

    common = sorted(
        flag
        for flag, per_surface in overrides["policy"]["parameters"].items()
        if per_surface.get("python", {}).get("tier") == "common"
    )
    assert common == sorted(
        ["--seed", "--num-trials", "--two-level", "--directed", "--markov-time"]
    )


def test_parameter_catalog_rejects_unknown_choices_flag():
    import pytest

    parameters = [_minimal_param("--seed", required=True, longType="integer")]
    overrides = {
        "policy": _tier_policy("--seed"),
        "choices": {"--seeed": ["a", "b"]},
    }

    with pytest.raises(RuntimeError, match=r"--seeed"):
        ParameterCatalog(parameters, overrides)


def test_parameter_catalog_rejects_non_list_choices():
    import pytest

    parameters = [_minimal_param("--seed", required=True, longType="integer")]
    overrides = {
        "policy": _tier_policy("--seed"),
        "choices": {"--seed": "clu"},
    }

    with pytest.raises(RuntimeError, match="list of strings"):
        ParameterCatalog(parameters, overrides)


def test_parameter_catalog_rejects_unknown_binding_only_language():
    import pytest

    parameters = [_minimal_param("--seed", required=True, longType="integer")]
    overrides = {
        "policy": _tier_policy("--seed"),
        "bindingOnly": {"cpp": [{"name": "foo", "flag": "--foo", "reason": "x"}]},
    }

    with pytest.raises(RuntimeError, match="cpp"):
        ParameterCatalog(parameters, overrides)


def test_parameter_catalog_rejects_unknown_binding_only_entry_key():
    import pytest

    parameters = [_minimal_param("--seed", required=True, longType="integer")]
    overrides = {
        "policy": _tier_policy("--seed"),
        "bindingOnly": {
            "python": [
                {"name": "foo", "flag": "--foo", "reason": "x", "typ": "bool"}
            ]
        },
    }

    with pytest.raises(RuntimeError, match="typ"):
        ParameterCatalog(parameters, overrides)
