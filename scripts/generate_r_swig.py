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
    title = github_actions_escape("Tracked R SWIG outputs are stale")
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
            "Tracked R SWIG outputs must be generated with "
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
        "-r",
        "-outdir",
        str(temp_dir),
        "-o",
        str(cpp_out),
        str(INTERFACE_FILE),
    ]
    subprocess.run(command, check=True)
    r_out = temp_dir / "infomap.R"
    if not r_out.exists():
        raise RuntimeError(f"SWIG did not produce {r_out}")
    return r_out, cpp_out


def scrub_invocation_comment(text: str) -> str:
    """Remove the non-reproducible SWIG invocation banner.

    SWIG R writes the actual invocation (including absolute tmp paths)
    as a comment near the top of the generated R wrapper. The cpp output
    is unaffected. Strip those lines so tracked outputs are reproducible.
    """
    lines = text.splitlines(keepends=True)
    out = []
    skip_next_swig_line = False
    for line in lines:
        if skip_next_swig_line:
            skip_next_swig_line = False
            if line.lstrip().startswith("##") and "swig" in line:
                continue
        if line.startswith("##") and "command line invocation" in line.lower():
            skip_next_swig_line = True
            continue
        out.append(line)
    return "".join(out)


# REMOVE WHEN SWIG > 4.4.1 emits R 4.6-clean code (upstream fix in
# https://github.com/swig/swig/pull/3411, not yet released as of SWIG 4.4.1).
# R 4.6.0 (April 2025) removed two non-API macros that SWIG 4.4.1's R
# templates still emit:
#   - SET_S4_OBJECT(x): mutated the S4 bit in place. Replacement is
#     x = Rf_asS4(x, TRUE, 0), available since R 2.4.0.
#   - CHARACTER_POINTER(s): now returns const SEXP *, so the
#     CHARACTER_POINTER(s)[i] = Rf_mkChar(...) idiom no longer compiles.
#     Replacement is SET_STRING_ELT(s, i, Rf_mkChar(...)).
# The replacements come straight from SWIG PR #3411.
_CHARACTER_POINTER_INNER = r"(?:[^()]|\([^()]*\))*"
_CHARACTER_POINTER_RE = re.compile(
    r"CHARACTER_POINTER\((" + _CHARACTER_POINTER_INNER + r")\)\[([^\]]+)\]\s*=\s*(.+?);",
    re.DOTALL,
)


def patch_for_r_4_6(text: str) -> str:
    """Rewrite SWIG-emitted non-API macros to R 4.6-clean equivalents."""
    text = re.sub(
        r"SET_S4_OBJECT\((\w+)\);",
        r"\1 = Rf_asS4(\1, TRUE, 0);",
        text,
    )
    text = _CHARACTER_POINTER_RE.sub(
        lambda m: f"SET_STRING_ELT({m.group(1)}, {m.group(2)}, {m.group(3)});",
        text,
    )
    text = _patch_r_init_symbol_visibility(text)
    return text


# R CMD check --as-cran requires `R_useDynamicSymbols(dll, FALSE)` to
# constrain the .Call interface to the registered routines table. SWIG's
# R_init_<pkg> registers routines but doesn't call this, triggering a
# WARNING. Append the call inside R_init_infomap.
def _patch_r_init_symbol_visibility(text: str) -> str:
    needle = "R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);"
    if needle not in text:
        return text
    # Idempotent: skip if already patched.
    if "R_useDynamicSymbols(dll, FALSE);" in text:
        return text
    # NOTE: do not add R_forceSymbols(dll, TRUE) here — SWIG's generated
    # R bindings invoke C entry points by string name via .Call(...), which
    # forceSymbols breaks.
    replacement = (
        "R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);\n"
        "    R_useDynamicSymbols(dll, FALSE);"
    )
    return text.replace(needle, replacement, 1)


def copy_file(src: Path, dest: Path, scrub: bool = False, patch_cpp: bool = False) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if scrub:
        text = scrub_invocation_comment(src.read_text(encoding="utf-8"))
    elif patch_cpp:
        text = patch_for_r_4_6(src.read_text(encoding="utf-8"))
    else:
        shutil.copyfile(src, dest)
        return
    dest.write_text(text, encoding="utf-8")


def files_match(expected: Path, actual: Path, scrub: bool = False, patch_cpp: bool = False) -> bool:
    if not expected.exists():
        return False
    actual_text = actual.read_text(encoding="utf-8")
    if scrub:
        actual_text = scrub_invocation_comment(actual_text)
    elif patch_cpp:
        actual_text = patch_for_r_4_6(actual_text)
    return expected.read_text(encoding="utf-8") == actual_text


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--r-out", required=True)
    parser.add_argument("--cpp-out", required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    r_out = Path(args.r_out)
    cpp_out = Path(args.cpp_out)

    with tempfile.TemporaryDirectory(prefix="infomap-r-swig-") as temp_dir_name:
        temp_dir = Path(temp_dir_name)
        generated_r, generated_cpp = generate(temp_dir)

        if args.check:
            failures = []
            if not files_match(r_out, generated_r, scrub=True):
                failures.append(str(r_out))
            if not files_match(cpp_out, generated_cpp, patch_cpp=True):
                failures.append(str(cpp_out))
            if failures:
                print_github_actions_error(
                    "Regenerate the R SWIG wrapper outputs with `make build-r-swig` "
                    "and commit the updated generated files."
                )
                print("Tracked SWIG outputs are stale:")
                for path in failures:
                    print(f"  {path}")
                print()
                print(
                    "This usually means a C++ header or SWIG interface changed, "
                    "but the generated R wrapper files were not regenerated."
                )
                print("Run: make build-r-swig")
                print("Then commit the updated files listed above.")
                return 1
            print("Tracked SWIG outputs are fresh.")
            return 0

        copy_file(generated_r, r_out, scrub=True)
        copy_file(generated_cpp, cpp_out, patch_cpp=True)

    print(f"Updated {r_out}")
    print(f"Updated {cpp_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
