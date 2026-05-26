#!/usr/bin/env python3

import subprocess
import sys


def run(cli, *args):
    return subprocess.run(
        [cli, *args],
        check=False,
        capture_output=True,
        text=True,
    )


def assert_completion(output):
    assert "Infomap" in output
    assert "infomap" in output
    assert "--completion" in output
    assert "--flow-model" in output
    assert "--cluster-data" in output
    assert "--output" in output
    assert "--print-json-parameters" not in output


def main() -> int:
    cli = sys.argv[1]
    help_result = run(cli, "-hh")
    assert help_result.returncode == 0, help_result.stderr
    expect_test_native_feature = "--test-native-feature" in help_result.stdout

    bash = run(cli, "--completion", "bash")
    assert bash.returncode == 0, bash.stderr
    assert "complete -F _infomap_completion Infomap infomap" in bash.stdout
    assert "bash zsh" in bash.stdout
    assert_completion(bash.stdout)
    assert ("--test-native-feature" in bash.stdout) == expect_test_native_feature

    zsh = run(cli, "--completion", "zsh")
    assert zsh.returncode == 0, zsh.stderr
    assert "#compdef Infomap infomap" in zsh.stdout
    assert "bash zsh" in zsh.stdout
    assert_completion(zsh.stdout)
    assert ("--test-native-feature" in zsh.stdout) == expect_test_native_feature

    invalid = run(cli, "--completion", "fish")
    assert invalid.returncode != 0
    assert "Unsupported completion shell 'fish'" in invalid.stderr

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
