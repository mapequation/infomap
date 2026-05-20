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
        },
    ]
    overrides = {
        "choices": {"--output": ["clu", "tree"]},
        "names": {
            "--verbose": {
                "python": "verbosity_level",
                "r": "verbosity_level",
                "ts": "verbose",
            }
        },
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
    assert (
        catalog.binding_only_entry("python", "include_self_links").render_policy
        == "deprecated_alias"
    )
    assert catalog.binding_only_entry("ts", "help").render_policy == "binding_only"
