#!/usr/bin/env python3

import json
import subprocess
import sys


def main() -> int:
    cli = sys.argv[1]
    result = subprocess.run(
        [cli, "--print-json-parameters"],
        check=True,
        capture_output=True,
        text=True,
    )
    payload = json.loads(result.stdout)
    parameters = payload["parameters"]
    assert parameters[0]["long"] == "--help"
    assert parameters[1]["long"] == "--version"
    assert "--completion" not in {param["long"] for param in parameters}
    assert "--print-json-parameters" not in {param["long"] for param in parameters}

    groups = [param["group"] for param in parameters]
    assert groups[:2] == ["About", "About"]
    assert "Input" in groups
    assert "Output" in groups
    assert "Algorithm" in groups
    assert "Accuracy" in groups

    by_long = {param["long"]: param for param in parameters}
    assert by_long["--flow-model"]["choices"] == [
        "undirected",
        "directed",
        "undirdir",
        "outdirdir",
        "rawdir",
        "precomputed",
    ]
    assert by_long["--output"]["choices"] == [
        "clu",
        "tree",
        "ftree",
        "newick",
        "json",
        "csv",
        "network",
        "states",
        "flow",
    ]
    assert by_long["--verbose"]["bindingNames"]["python"] == "verbosity_level"
    assert by_long["--verbose"]["bindingNames"]["r"] == "verbosity_level"
    assert by_long["--verbose"]["bindingNames"]["ts"] == "verbose"
    assert by_long["--verbose"]["renderPolicy"] == "repeated_short"
    assert by_long["--verbose"]["bindingDocs"]["python"]["description"].startswith(
        "Verbosity level on the console."
    )
    assert by_long["--verbose"]["bindingDefaults"]["python"] == {"value": "1"}
    assert by_long["--verbose"]["bindingDefaults"]["r"] == {"value": "1L"}
    assert by_long["--output"]["renderPolicy"] == "comma_list"
    assert by_long["--fast-hierarchical-solution"]["bindingDocs"]["python"][
        "description"
    ].startswith("Find top modules fast.")
    assert by_long["--teleportation-probability"]["bindingDefaults"]["python"] == {
        "value": "0.15"
    }
    assert by_long["--teleportation-probability"]["min"] == "0"
    assert by_long["--teleportation-probability"]["max"] == "1"
    assert by_long["--directed"]["bindingDefaults"]["python"]["value"] == "None"
    assert by_long["--num-trials"]["min"] == "1"
    assert by_long["--num-trials"]["bindingDefaults"]["r"]["value"] == "1L"
    assert by_long["--core-level-limit"]["default"] == "0"
    assert by_long["--core-level-limit"]["min"] == "0"
    assert by_long["--tune-iteration-limit"]["default"] == "0"
    assert by_long["--tune-iteration-limit"]["min"] == "0"
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
