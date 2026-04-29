#!/usr/bin/env python3

import argparse
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
INTERFACE_FILE = REPO_ROOT / "interfaces" / "swig" / "Infomap.i"
REQUIRED_SWIG_VERSION = "4.4.1"


def github_actions_escape(value: str) -> str:
    return value.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A").replace(":", "%3A")


def print_github_actions_error(message: str) -> None:
    if os.environ.get("GITHUB_ACTIONS") != "true":
        return
    title = github_actions_escape("Tracked Python SWIG outputs are stale")
    body = github_actions_escape(message)
    print(f"::error title={title}::{body}")


def get_swig_command() -> str:
    return os.environ.get("SWIG") or shutil.which("swig") or "swig"


def get_swig_version(swig_command: str) -> str:
    result = subprocess.run(
        [swig_command, "-version"],
        check=True,
        capture_output=True,
        text=True,
    )
    match = re.search(r"SWIG Version\s+(\S+)", result.stdout)
    if match is None:
        raise RuntimeError(f"Could not parse SWIG version from output:\n{result.stdout}")
    return match.group(1)


def validate_swig_version(swig_command: str) -> None:
    version = get_swig_version(swig_command)
    if version != REQUIRED_SWIG_VERSION:
        raise RuntimeError(
            "Tracked Python SWIG outputs must be generated with "
            f"SWIG {REQUIRED_SWIG_VERSION}, but got {version} from {swig_command}. "
            "Install the required SWIG version or point SWIG=/path/to/swig-4.4.1."
        )


def generate(temp_dir: Path) -> tuple[Path, Path]:
    cpp_out = temp_dir / "infomap_wrap.cpp"
    swig_command = get_swig_command()
    validate_swig_version(swig_command)
    command = [
        swig_command,
        "-c++",
        "-python",
        "-outdir",
        str(temp_dir),
        "-o",
        str(cpp_out),
        str(INTERFACE_FILE),
    ]
    subprocess.run(command, check=True)
    python_out = temp_dir / "infomap.py"
    if not python_out.exists():
        raise RuntimeError(f"SWIG did not produce {python_out}")
    return python_out, cpp_out


def copy_file(src: Path, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dest)


def files_match(expected: Path, actual: Path) -> bool:
    return expected.exists() and expected.read_text(encoding="utf-8") == actual.read_text(encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--python-out", required=True)
    parser.add_argument("--cpp-out", required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    python_out = Path(args.python_out)
    cpp_out = Path(args.cpp_out)

    with tempfile.TemporaryDirectory(prefix="infomap-swig-") as temp_dir_name:
        temp_dir = Path(temp_dir_name)
        generated_python, generated_cpp = generate(temp_dir)

        if args.check:
            failures = []
            if not files_match(python_out, generated_python):
                failures.append(str(python_out))
            if not files_match(cpp_out, generated_cpp):
                failures.append(str(cpp_out))
            if failures:
                print_github_actions_error(
                    "Regenerate the Python SWIG wrapper outputs with `make build-python-swig` "
                    "and commit the updated generated files."
                )
                print("Tracked SWIG outputs are stale:")
                for path in failures:
                    print(f"  {path}")
                print()
                print(
                    "This usually means a C++ header or SWIG interface changed, "
                    "but the generated Python wrapper files were not regenerated."
                )
                print("Run: make build-python-swig")
                print("Then commit the updated files listed above.")
                return 1
            print("Tracked SWIG outputs are fresh.")
            return 0

        copy_file(generated_python, python_out)
        copy_file(generated_cpp, cpp_out)

    print(f"Updated {python_out}")
    print(f"Updated {cpp_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
