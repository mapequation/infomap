#!/usr/bin/env python3
"""Stage the infomap R package for `R CMD build` or in-place install.

The hand-edited skeleton at interfaces/R/infomap/ does not contain the C++
core or the SWIG-generated wrapper. This script copies them in and renders
the Makevars{,.win} templates with an explicit OBJECTS list.

Two modes:

  --out-dir DIR       Stage a self-contained copy of the package into DIR
                      (used by `make build-r` to produce a tarball that
                      ships every C++ source. The configure script is
                      removed in this mode since the staged tree is
                      already complete).

  --in-place          Stage into the skeleton itself, populating
                      <skeleton>/src/infomap/ etc. (used by the
                      configure script that r-universe runs at install
                      time, when the full repo is available next to the
                      skeleton).
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SKELETON = REPO_ROOT / "interfaces" / "R" / "infomap"
DEFAULT_GENERATED = REPO_ROOT / "interfaces" / "R" / "generated"
DEFAULT_SRC = REPO_ROOT / "src"
DEFAULT_VENDOR = REPO_ROOT / "vendor"
DEFAULT_OUT_DIR = REPO_ROOT / "build" / "R" / "infomap"
PYTHON_VERSION_FILE = (
    REPO_ROOT / "interfaces" / "python" / "src" / "infomap" / "_version.py"
)


def find_cpp_sources(src_root: Path) -> list[Path]:
    return sorted(p for p in src_root.rglob("*.cpp") if p.name != "main.cpp")


def read_python_version() -> str:
    """Read the canonical version from interfaces/python/src/infomap/_version.py.

    That file is the single source of truth bumped by release-please.
    """
    if not PYTHON_VERSION_FILE.exists():
        return ""
    text = PYTHON_VERSION_FILE.read_text(encoding="utf-8")
    import re

    match = re.search(r'__version__\s*=\s*["\']([^"\']+)["\']', text)
    return match.group(1) if match else ""


def sync_description_version(description_path: Path, version: str) -> None:
    if not description_path.exists() or not version:
        return
    import re

    text = description_path.read_text(encoding="utf-8")
    new_text, n = re.subn(
        r"^Version:\s*\S+\s*$",
        f"Version: {version}",
        text,
        count=1,
        flags=re.MULTILINE,
    )
    if n == 1 and new_text != text:
        description_path.write_text(new_text, encoding="utf-8")


def render_objects(cpp_relpaths: list[str]) -> str:
    object_lines = [path[:-4] + ".o" for path in cpp_relpaths]
    indent = "  "
    return " \\\n".join(indent + line for line in object_lines)


def render_makevars(template: str, objects_block: str) -> str:
    if "@OBJECTS@" not in template:
        raise RuntimeError("Makevars template missing @OBJECTS@ placeholder")
    return template.replace("@OBJECTS@", objects_block)


def copy_tree(src: Path, dest: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(src, dest)


def copy_skeleton(skeleton: Path, dest: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(skeleton, dest)


def normalize_configure_permissions(pkg_root: Path) -> None:
    for name in ("configure", "configure.win", "cleanup"):
        path = pkg_root / name
        if path.exists():
            path.chmod(0o755)


def remove_compiled_artifacts(pkg_src: Path) -> None:
    for pattern in ("*.o", "*.so", "*.dll", "*.dylib"):
        for path in pkg_src.rglob(pattern):
            path.unlink()


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def stage_golden_fixtures(out_dir: Path) -> None:
    """Copy the golden-codelength manifest and the networks it references into
    the staged test tree, so the testthat suite can replay them. The source
    fixtures (test/fixtures/, examples/) are not shipped in the R package.
    Skipped when the manifest has not been generated yet."""
    manifest = REPO_ROOT / "test" / "fixtures" / "expected" / "golden-codelengths.json"
    if not manifest.exists():
        return
    golden_dir = out_dir / "tests" / "testthat" / "golden"
    golden_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(manifest, golden_dir / manifest.name)
    entries = json.loads(manifest.read_text(encoding="utf-8"))
    for network in sorted({entry["network"] for entry in entries}):
        shutil.copyfile(REPO_ROOT / network, golden_dir / Path(network).name)


def stage(
    out_dir: Path,
    skeleton: Path,
    generated: Path,
    src: Path,
    vendor: Path,
    in_place: bool,
) -> None:
    if in_place:
        if out_dir.resolve() != skeleton.resolve():
            raise RuntimeError("--in-place requires out_dir == skeleton")
    else:
        copy_skeleton(skeleton, out_dir)

    normalize_configure_permissions(out_dir)

    generated_cpp = generated / "infomap_wrap.cpp"
    generated_r = generated / "infomap.R"
    if not generated_cpp.exists():
        raise RuntimeError(
            f"Tracked SWIG C++ wrapper not found: {generated_cpp}. "
            "Run `make build-r-swig` to regenerate it."
        )
    if not generated_r.exists():
        raise RuntimeError(
            f"Tracked SWIG R wrapper not found: {generated_r}. "
            "Run `make build-r-swig` to regenerate it."
        )

    pkg_src = out_dir / "src"
    pkg_r = out_dir / "R"
    pkg_src.mkdir(parents=True, exist_ok=True)
    pkg_r.mkdir(parents=True, exist_ok=True)
    remove_compiled_artifacts(pkg_src)

    shutil.copyfile(generated_cpp, pkg_src / "infomap_wrap.cpp")
    # Rename the SWIG-generated R wrapper at stage time. Its default name
    # `infomap.R` collides with the user-facing facade `Infomap.R` on
    # case-insensitive filesystems (macOS HFS+/APFS, Windows NTFS).
    shutil.copyfile(generated_r, pkg_r / "swig_bindings.R")

    # The SWIG-generated wrapper has `#include "src/Infomap.h"`, so the
    # repo's src/ tree must be copied to <pkg>/src/src/ for the include
    # to resolve when -I. is on the include path.
    nested_src = pkg_src / "src"
    copy_tree(src, nested_src)
    copy_tree(vendor / "nlohmann_json", out_dir / "inst" / "nlohmann_json")
    copy_tree(vendor / "fmt", out_dir / "inst" / "fmt")

    cpp_sources = ["infomap_wrap.cpp"] + [
        f"src/{p.relative_to(src).as_posix()}" for p in find_cpp_sources(src)
    ]
    objects_block = render_objects(cpp_sources)

    for in_name, out_name in (
        ("Makevars.in", "Makevars"),
        ("Makevars.win.in", "Makevars.win"),
    ):
        template_path = pkg_src / in_name
        if not template_path.exists():
            raise RuntimeError(f"Skeleton missing template: {template_path}")
        rendered = render_makevars(
            template_path.read_text(encoding="utf-8"), objects_block
        )
        write_text(pkg_src / out_name, rendered)
        if not in_place:
            template_path.unlink()

    stage_golden_fixtures(out_dir)

    sync_description_version(out_dir / "DESCRIPTION", read_python_version())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=DEFAULT_OUT_DIR,
        help="Where to stage the package (default: build/R/infomap).",
    )
    parser.add_argument(
        "--skeleton",
        type=Path,
        default=DEFAULT_SKELETON,
        help="Path to the package skeleton (default: interfaces/R/infomap).",
    )
    parser.add_argument(
        "--generated",
        type=Path,
        default=DEFAULT_GENERATED,
        help="Path to tracked SWIG outputs (default: interfaces/R/generated).",
    )
    parser.add_argument(
        "--src",
        type=Path,
        default=DEFAULT_SRC,
        help="Path to the C++ core sources (default: src).",
    )
    parser.add_argument(
        "--vendor",
        type=Path,
        default=DEFAULT_VENDOR,
        help="Path to vendored C++ dependencies (default: vendor).",
    )
    parser.add_argument(
        "--in-place",
        action="store_true",
        help="Stage into the skeleton itself; skip skeleton copy and keep configure scripts.",
    )
    args = parser.parse_args()

    out_dir = args.out_dir.resolve()
    skeleton = args.skeleton.resolve()
    generated = args.generated.resolve()
    src = args.src.resolve()
    vendor = args.vendor.resolve()

    if args.in_place:
        out_dir = skeleton

    stage(out_dir, skeleton, generated, src, vendor, args.in_place)

    where = "in place" if args.in_place else f"into {out_dir}"
    print(f"Staged infomap R package {where}.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
