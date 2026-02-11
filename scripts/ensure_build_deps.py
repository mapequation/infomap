#!/usr/bin/env python3
"""Ensure build dependencies are available for building the Python extension.

This script is intended to be invoked from the Makefile. It checks for
`setuptools` and installs `pip`, `setuptools`, and `wheel` if missing.
"""

import importlib.util
import subprocess
import sys


def main() -> int:
    if importlib.util.find_spec("setuptools") is None:
        print("Installing build dependencies: pip, setuptools, wheel")
        try:
            subprocess.check_call(
                [
                    sys.executable,
                    "-m",
                    "pip",
                    "install",
                    "--upgrade",
                    "pip",
                    "setuptools",
                    "wheel",
                ]
            )
        except subprocess.CalledProcessError as exc:
            print("Failed to install build dependencies:", exc)
            return 1
    else:
        print("Build dependencies already present: setuptools available")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
