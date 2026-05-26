#!/usr/bin/env python3

import subprocess
import sys


def run(cli, *args):
    return subprocess.run(
        [cli, *args],
        check=True,
        capture_output=True,
        text=True,
    ).stdout


def assert_ordered(output: str, needles: list[str]) -> None:
    position = -1
    for needle in needles:
        next_position = output.find(needle)
        assert next_position > position, f"Expected {needle!r} after offset {position}"
        position = next_position


def main() -> int:
    cli = sys.argv[1]

    standard = run(cli, "--help")
    version = run(cli, "--version")
    assert "Usage:\n        Infomap network_file out_directory [options]" in standard
    assert "[network_file]" in standard
    assert "[out_directory]" in standard
    assert "--print-json-parameters" in standard
    assert "--flow-model <option>" in standard
    assert "--num-trials <integer>" in standard
    assert "--skip-adjust-bipartite-flow" not in standard
    assert "--teleportation-probability" not in standard
    assert "--output <list>" not in standard
    assert "--test-native-feature" not in standard
    assert_ordered(
        standard,
        [
            "About\n-----",
            "Input\n-----",
            "Algorithm\n---------",
            "Accuracy\n--------",
            "Output\n------",
        ],
    )

    advanced = run(cli, "-hh")
    has_test_native_feature = "--test-native-feature" in advanced
    assert "--skip-adjust-bipartite-flow" in advanced
    assert "--teleportation-probability <probability>" in advanced
    assert "--output <list>" in advanced
    assert "--include-self-links" in advanced
    assert "--print-json-parameters" in advanced
    if has_test_native_feature:
        assert "Native features: test-native-feature" in version
    else:
        assert "test-native-feature" not in version
    assert_ordered(
        advanced,
        [
            "--weight-threshold <number>",
            "--include-self-links",
            "--no-self-links",
            "--node-limit <integer>",
        ],
    )
    assert_ordered(
        advanced,
        [
            "--flow-model <option>",
            "--directed",
            "--recorded-teleportation",
            "--teleportation-probability <probability>",
        ],
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
