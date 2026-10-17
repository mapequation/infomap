#!/usr/bin/env python3
"""Run the R package's tests and examples under ASan/UBSan or valgrind.

CRAN runs its additional checks ("gcc-ASAN", "clang-UBSAN", "valgrind") on every
package. This gets close without a sanitizer-built R:

  --mode asan      Installs the package, built with -fsanitize=address,undefined,
                   into a private library and runs the code with the sanitizer
                   runtime loaded before R (LD_PRELOAD on Linux,
                   DYLD_INSERT_LIBRARIES on macOS). R itself is not instrumented,
                   so only the package's own memory errors and UB are seen,
                   which is what the checks are for.
  --mode valgrind  Installs the package with the usual flags and runs the same
                   code under valgrind (`R -d valgrind`), which also catches reads
                   of uninitialised memory. Linux only.

The code run is the testthat suite, the examples in the Rd files (what R CMD
check runs) and examples/R/*.R. Vignettes are not run. Leaks are not checked:
R keeps much of its memory alive until exit, and CRAN reports leaks without
failing on them.

Usage: run_r_sanitizers.py [--mode asan|valgrind] [--cxx clang++] [--r R]
       [--build-dir DIR] [--jobs N] [--skip-install]
"""

from __future__ import annotations

import argparse
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

from run_python_asan import asan_runtime, is_clang

REPO_ROOT = Path(__file__).resolve().parents[2]
SANITIZE = "address,undefined"

# Sources every example and test file and stops at the first error. Kept in one
# R process per kind so a sanitizer report names the file it came from.
RUNNER = r"""
args <- commandArgs(trailingOnly = TRUE)
lib <- args[[1]]; tests <- args[[2]]; examples <- args[[3]]
.libPaths(c(lib, .libPaths()))
suppressPackageStartupMessages(library(infomap, lib.loc = lib))
cat("infomap loaded from", find.package("infomap"), "\n")

cat("== testthat\n")
testthat::test_dir(tests, package = "infomap", load_package = "installed",
                   stop_on_failure = TRUE, reporter = "summary")

cat("== Rd examples\n")
db <- tools::Rd_db("infomap", lib.loc = lib)
for (name in names(db)) {
  out <- tempfile(fileext = ".R")
  tools::Rd2ex(db[[name]], out, commentDontrun = TRUE, commentDonttest = FALSE)
  if (!file.exists(out)) next
  cat("--", name, "\n")
  withr::with_tempfile("wd", {
    dir.create(wd)
    withr::with_dir(wd, source(out, echo = FALSE, local = new.env()))
  })
}

cat("== examples/R\n")
for (f in Sys.glob(file.path(examples, "*.R"))) {
  if (grepl("load-infomap.R$", f)) next
  needs <- regmatches(readLines(f), regexpr("(?<=library\\()[A-Za-z0-9.]+", readLines(f), perl = TRUE))
  missing <- needs[!vapply(needs, requireNamespace, logical(1), quietly = TRUE)]
  if (length(missing)) {
    cat("-- skip", basename(f), "(needs", paste(missing, collapse = ", "), ")\n")
    next
  }
  cat("--", basename(f), "\n")
  withr::with_dir(dirname(f), source(f, echo = FALSE, local = new.env()))
}
cat("== done\n")
"""


def _run(cmd, **kwargs):
    print("+", shlex.join(str(c) for c in cmd), flush=True)
    return subprocess.run(cmd, check=True, **kwargs)


def _r_env(r: str) -> dict[str, str]:
    """The environment R's shell wrapper sets up before it starts the R binary."""
    out = subprocess.run(
        [
            r,
            "CMD",
            sys.executable,
            "-c",
            "import os, json; print(json.dumps(dict(os.environ)))",
        ],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return json.loads(out.strip().splitlines()[-1])


def _install(args, build_dir: Path, lib_dir: Path) -> None:
    pkg_dir = build_dir / "infomap"
    _run(
        [sys.executable, "scripts/stage_r_package.py", "--out-dir", str(pkg_dir)],
        cwd=REPO_ROOT,
    )
    lib_dir.mkdir(parents=True, exist_ok=True)
    makevars = build_dir / "Makevars"
    lines = []
    if args.mode == "asan":
        # Close to CRAN's sanitizer flags, but fatal: CRAN reads the reports out
        # of the check log, a test run has to fail on them.
        shared_libsan = (
            " -shared-libsan" if sys.platform != "darwin" and is_clang(args.cxx) else ""
        )
        lines += [
            (
                f"CXX17FLAGS = -g -O1 -fno-omit-frame-pointer -fsanitize={SANITIZE}"
                " -fno-sanitize-recover=undefined"
            ),
            f"LDFLAGS += -fsanitize={SANITIZE}{shared_libsan}",
        ]
        if sys.platform != "darwin":
            # macOS: the package's configure pins Apple clang (see
            # interfaces/R/infomap/configure), which is the compiler whose runtime
            # we load. Elsewhere, build with the compiler whose runtime we load.
            lines += [f"CXX17 = {args.cxx}", f"CXX = {args.cxx}"]
    else:
        lines += ["CXX17FLAGS = -g -O2"]
    makevars.write_text("\n".join(lines) + "\n")
    env = dict(os.environ)
    env["R_MAKEVARS_USER"] = str(makevars)
    env["MAKE"] = f"make -j{args.jobs}"
    env.pop("MAKEFLAGS", None)
    # --no-test-load: loading the instrumented library into an R without the
    # sanitizer runtime in front aborts. The run below loads it properly.
    _run(
        [
            args.r,
            "CMD",
            "INSTALL",
            "--no-test-load",
            "--no-multiarch",
            f"--library={lib_dir}",
            str(pkg_dir),
        ],
        env=env,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--mode", choices=["asan", "valgrind"], default="asan")
    parser.add_argument("--cxx", default=os.environ.get("CXX") or "clang++")
    parser.add_argument("--r", default="R")
    parser.add_argument("--build-dir", default=None)
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--valgrind-args", default="")
    parser.add_argument(
        "--skip-install", action="store_true", help="reuse the last install"
    )
    args = parser.parse_args()

    build_dir = Path(args.build_dir or REPO_ROOT / "build" / f"R-{args.mode}").resolve()
    lib_dir = build_dir / "lib"
    if not args.skip_install:
        _install(args, build_dir, lib_dir)

    runner = build_dir / "run.R"
    runner.write_text(RUNNER)
    r_args = [
        "--vanilla",
        "--no-echo",
        "-f",
        str(runner),
        "--args",
        str(lib_dir),
        str(build_dir / "infomap" / "tests" / "testthat"),
        str(REPO_ROOT / "examples" / "R"),
    ]

    if args.mode == "valgrind":
        valgrind = (
            "valgrind --error-exitcode=1 --leak-check=no --track-origins=yes "
            + args.valgrind_args
        ).strip()
        return subprocess.run(
            [args.r, "-d", valgrind, *r_args], cwd=build_dir, check=False
        ).returncode

    # R's front end is a shell script, and macOS strips DYLD_* variables from
    # anything started through /bin/sh. Take the environment the script would
    # set and start the R binary directly instead.
    env = _r_env(args.r)
    # macOS: configure pins Apple clang, so its runtime is the one the package links.
    runtime = asan_runtime("/usr/bin/clang++" if sys.platform == "darwin" else args.cxx)
    if sys.platform == "darwin":
        env["DYLD_INSERT_LIBRARIES"] = str(runtime)
    else:
        env["LD_PRELOAD"] = str(runtime)
    env["ASAN_OPTIONS"] = ":".join(
        filter(None, ["detect_leaks=0", os.environ.get("ASAN_OPTIONS", "")])
    )
    env["UBSAN_OPTIONS"] = ":".join(
        filter(
            None,
            [
                "print_stacktrace=1",
                "halt_on_error=1",
                os.environ.get("UBSAN_OPTIONS", ""),
            ],
        )
    )
    r_binary = Path(env["R_HOME"]) / "bin" / "exec" / "R"
    return subprocess.run(
        [str(r_binary), *r_args], env=env, cwd=build_dir, check=False
    ).returncode


if __name__ == "__main__":
    sys.exit(main())
