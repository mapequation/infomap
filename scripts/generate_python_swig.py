#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
INTERFACE_FILE = REPO_ROOT / "interfaces" / "swig" / "Infomap.i"


def generate(temp_dir: Path) -> tuple[Path, Path]:
    cpp_out = temp_dir / "infomap_wrap.cpp"
    command = [
        os.environ.get("SWIG") or shutil.which("swig") or "swig",
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
                print("Tracked SWIG outputs are stale:")
                for path in failures:
                    print(f"  {path}")
                print("Run: make build-python-swig")
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
