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
    assert any(param["long"] == "--flow-model" for param in parameters)
    assert any(param["long"] == "--output" for param in parameters)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
