#!/usr/bin/env python3
"""Run the Python tests against an ASan/UBSan build of the extension.

Builds ``infomap._infomap`` with the flags of the C++ sanitizer build
(``INFOMAP_ENABLE_SANITIZERS``): address, undefined and float-divide-by-zero,
UB fatal, the flow invariant checks on, OpenMP off (``make test-tsan`` covers
the threads). The build goes to its own directory next to a copy of the
package sources, so the dev install's extension is never touched.

The interpreter itself is not instrumented, so the sanitizer runtime has to be
loaded before it: ``DYLD_INSERT_LIBRARIES`` on macOS, ``LD_PRELOAD`` on Linux.
Python's own allocator is switched off (``PYTHONMALLOC=malloc``) so ASan also
sees the memory of the Python objects the SWIG proxies hold on to. Leak
detection is off: CPython keeps much of its memory alive until exit on purpose.

Usage: run_python_asan.py [--cxx clang++] [--build-dir DIR] [--jobs N]
       [--skip-build] [-- pytest args]
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_SRC = REPO_ROOT / "interfaces" / "python" / "src" / "infomap"
SANITIZE = "address,undefined,float-divide-by-zero"
UBSAN_SUPPRESSIONS = REPO_ROOT / "test" / "python-ubsan.supp"


def is_clang(cxx: str) -> bool:
    out = subprocess.run(
        [cxx, "--version"], capture_output=True, text=True, check=True
    ).stdout
    return "clang" in out.lower()


def asan_runtime(cxx: str) -> Path:
    """The ASan runtime the extension links, which the interpreter must preload."""
    if sys.platform == "darwin":
        candidates = ["libclang_rt.asan_osx_dynamic.dylib"]
    elif is_clang(cxx):
        machine = platform.machine()
        candidates = ["libclang_rt.asan.so", f"libclang_rt.asan-{machine}.so"]
    else:
        candidates = ["libasan.so"]
    runtime_dir = subprocess.run(
        [cxx, "-print-runtime-dir"], capture_output=True, text=True, check=False
    ).stdout.strip()
    for name in candidates:
        if runtime_dir and (Path(runtime_dir) / name).exists():
            return (Path(runtime_dir) / name).resolve()
        found = subprocess.run(
            [cxx, f"-print-file-name={name}"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
        # -print-file-name echoes the bare name back when it finds nothing.
        if os.path.isabs(found) and os.path.exists(found):
            return Path(found).resolve()
    sys.exit(f"no ASan runtime found for {cxx} (tried {candidates})")


def _build(cxx: str, build_dir: Path, jobs: int) -> Path:
    lib_dir = build_dir / "lib"
    # Fresh copy of the pure-Python sources, without the dev install's extension.
    shutil.rmtree(lib_dir / "infomap", ignore_errors=True)
    shutil.copytree(
        PACKAGE_SRC,
        lib_dir / "infomap",
        ignore=shutil.ignore_patterns("_infomap*.so", "_infomap*.pyd", "__pycache__"),
    )
    sanitize_flags = f"-fsanitize={SANITIZE} -fno-omit-frame-pointer"
    # Clang links a shared object against no sanitizer runtime unless asked; the
    # interpreter preloads the shared one, so link that.
    shared_libsan = (
        " -shared-libsan" if sys.platform != "darwin" and is_clang(cxx) else ""
    )
    env = dict(os.environ)
    env.update(
        {
            "CC": cxx,
            "CXX": cxx,
            # debug: -g and the flow invariant checks (INFOMAP_CHECK_INVARIANTS),
            # as in the C++ sanitizer build. -O1 below keeps the suite's runtime
            # bearable; -UNDEBUG undoes the -DNDEBUG from Python's own CFLAGS.
            "MODE": "debug",
            "OPENMP": "0",
            # UB checks stay recoverable so test/python-ubsan.supp can apply;
            # halt_on_error=1 below still stops the run at any other report.
            "CXXFLAGS": f"-O1 -UNDEBUG {sanitize_flags} {os.environ.get('CXXFLAGS', '')}",
            "LDFLAGS": f"-fsanitize={SANITIZE}{shared_libsan} {os.environ.get('LDFLAGS', '')}",
            "INFOMAP_BUILD_JOBS": str(jobs),
        }
    )
    subprocess.run(
        [
            sys.executable,
            "setup.py",
            "-q",
            "build_ext",
            "--force",
            "--build-lib",
            str(lib_dir),
            "--build-temp",
            str(build_dir / "tmp"),
        ],
        cwd=REPO_ROOT,
        env=env,
        check=True,
    )
    return lib_dir


def _sanitizer_env(runtime: Path, lib_dir: Path) -> dict[str, str]:
    env = dict(os.environ)
    preload = "DYLD_INSERT_LIBRARIES" if sys.platform == "darwin" else "LD_PRELOAD"
    env[preload] = str(runtime)
    env["PYTHONPATH"] = os.pathsep.join(
        p for p in (str(lib_dir), os.environ.get("PYTHONPATH", "")) if p
    )
    env["PYTHONMALLOC"] = "malloc"
    env["ASAN_OPTIONS"] = ":".join(
        filter(
            None,
            [
                "detect_leaks=0",
                "strict_string_checks=1",
                "print_suppressions=0",
                # macOS: ASan drops itself from DYLD_INSERT_LIBRARIES once loaded,
                # so a test that starts another interpreter would load the
                # extension without it and abort.
                "strip_env=0" if sys.platform == "darwin" else "",
                os.environ.get("ASAN_OPTIONS", ""),
            ],
        )
    )
    env["UBSAN_OPTIONS"] = ":".join(
        filter(
            None,
            [
                "print_stacktrace=1",
                "halt_on_error=1",
                f"suppressions={UBSAN_SUPPRESSIONS}",
                os.environ.get("UBSAN_OPTIONS", ""),
            ],
        )
    )
    return env


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cxx", default=os.environ.get("CXX") or "clang++")
    parser.add_argument("--build-dir", default=str(REPO_ROOT / "build" / "python-asan"))
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument(
        "--skip-build", action="store_true", help="reuse the last build"
    )
    parser.add_argument("pytest_args", nargs="*")
    args = parser.parse_args()

    build_dir = Path(args.build_dir).resolve()
    lib_dir = (
        build_dir / "lib" if args.skip_build else _build(args.cxx, build_dir, args.jobs)
    )
    env = _sanitizer_env(asan_runtime(args.cxx), lib_dir)

    # Make sure the tests import this build, not an installed infomap.
    where = subprocess.run(
        [sys.executable, "-c", "import infomap._infomap as m; print(m.__file__)"],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    if where.returncode != 0 or not where.stdout.strip().startswith(str(lib_dir)):
        sys.stderr.write(where.stderr)
        sys.exit(
            f"run_python_asan: the tests would not import {lib_dir}: {where.stdout}"
        )

    pytest_args = args.pytest_args or ["test/python"]
    # --capture=sys: the default fd capture swallows a sanitizer report, which goes
    # straight to fd 2 just before the process aborts.
    return subprocess.run(
        [sys.executable, "-m", "pytest", "--capture=sys", *pytest_args],
        cwd=REPO_ROOT,
        env=env,
        check=False,
    ).returncode


if __name__ == "__main__":
    sys.exit(main())
