#!/usr/bin/env python3

import argparse
import json
import subprocess
from pathlib import Path

from parameter_catalog import ParameterCatalog

REPO_ROOT = Path(__file__).resolve().parents[1]
OVERRIDES = REPO_ROOT / "interfaces" / "parameters" / "overrides.json"

PUBLIC_PARAMETER_KEYS = {
    "long",
    "short",
    "description",
    "group",
    "required",
    "advanced",
    "incremental",
    "longType",
    "shortType",
    "default",
    "choices",
    "min",
    "max",
}


def load_parameters(infomap_bin):
    infomap_bin = infomap_bin.resolve()
    completed = subprocess.run(
        [str(infomap_bin), "--print-json-parameters"],
        check=True,
        capture_output=True,
        text=True,
    )
    payload = json.loads(completed.stdout)
    return payload["parameters"]


def public_parameter_metadata(parameters):
    hidden = hidden_js_parameter_flags(parameters)
    return [
        {key: value for key, value in parameter.items() if key in PUBLIC_PARAMETER_KEYS}
        for parameter in parameters
        if parameter["long"] not in hidden
    ]


def hidden_js_parameter_flags(parameters):
    # Resolve `ts` `hide` decisions through the shared ParameterCatalog so the
    # JS surface matches the TypeScript binding generator and the policy matrix
    # exactly. This honours per-parameter precedence over group rules (a
    # per-parameter `keep` un-hides a flag its group hides), which an
    # independent reimplementation here would miss (issue #755).
    overrides = json.loads(OVERRIDES.read_text(encoding="utf-8"))
    catalog = ParameterCatalog(parameters, overrides)
    return catalog.hidden_bindings.get("ts", set())


def write_json(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--infomap-bin", required=True)
    parser.add_argument("--output-dir", default="interfaces/js/generated")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    parameters = load_parameters(Path(args.infomap_bin))

    write_json(output_dir / "parameters.json", public_parameter_metadata(parameters))


if __name__ == "__main__":
    main()
