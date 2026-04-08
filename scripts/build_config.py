#!/usr/bin/env python3

import argparse
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path


def _split_flags(value):
    if not value:
        return []
    return shlex.split(value)


def _dedupe(items):
    seen = set()
    ordered = []
    for item in items:
        if not item or item in seen:
            continue
        seen.add(item)
        ordered.append(item)
    return ordered


def _norm_openmp(value):
    return str(value).lower() not in {"0", "false", "off", "no"}


def _compiler_version(compiler):
    try:
        completed = subprocess.run(
            shlex.split(compiler) + ["--version"],
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError:
        return ""
    return (completed.stdout or completed.stderr or "").lower()


def _compiler_command_basename(compiler):
    compiler = (compiler or "").strip()
    if not compiler:
        return ""

    stripped = compiler.strip("\"'")
    if ("\\" in stripped or "/" in stripped) and stripped.lower().endswith(".exe"):
        return Path(stripped.replace("\\", "/")).stem.lower()

    try:
        first_token = shlex.split(compiler)[0]
    except (ValueError, IndexError):
        first_token = stripped

    return Path(first_token).stem.lower()


def _compiler_family(compiler):
    version = _compiler_version(compiler)
    compiler_name = _compiler_command_basename(compiler)
    if "clang-cl" in compiler_name or compiler_name == "cl":
        return "msvc"
    if "clang" in version or "clang" in compiler_name:
        return "clang"
    if "gcc" in version or "g++" in compiler_name or "mingw" in version:
        return "gnu"
    if "microsoft" in version or "msvc" in version:
        return "msvc"
    return "unknown"


def _compiler_family_for_platform(compiler, platform_name):
    compiler_family = _compiler_family(compiler)
    if platform_name != "win32":
        return compiler_family

    compiler_name = _compiler_command_basename(compiler)
    if compiler_family == "msvc":
        return "msvc"
    if compiler_name in {"gcc", "g++"} or "mingw" in compiler_name:
        return "gnu"
    if compiler_name in {"clang", "clang++"}:
        return "clang"
    return "msvc"


def _brew_prefix(package=None):
    brew = shutil_which("brew")
    if not brew:
        return ""
    command = [brew, "--prefix"]
    if package:
        command.append(package)
    try:
        completed = subprocess.run(command, check=True, capture_output=True, text=True)
    except (OSError, subprocess.CalledProcessError):
        return ""
    return completed.stdout.strip()


def shutil_which(name):
    for directory in os.environ.get("PATH", "").split(os.pathsep):
        candidate = Path(directory) / name
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate)
    return None


def resolve_build_config(
    *,
    mode="release",
    openmp=True,
    compiler="c++",
    cppflags="",
    cxxflags="",
    ldflags="",
    deployment_target="",
    platform_name=None,
):
    platform_name = platform_name or sys.platform
    compiler_family = _compiler_family_for_platform(compiler, platform_name)
    is_darwin = platform_name in {"darwin", "Darwin"}

    brew_prefix = _brew_prefix() if is_darwin else ""
    libomp_prefix = _brew_prefix("libomp") if is_darwin else ""

    base_compile_flags = []
    if compiler_family == "msvc":
        base_compile_flags.append("/std:c++14")
    else:
        base_compile_flags.extend(["-Wall", "-Wextra", "-pedantic", "-Wnon-virtual-dtor", "-std=c++14"])

    mode_compile_flags = []
    if mode == "debug":
        if compiler_family == "msvc":
            mode_compile_flags.extend(["/Od", "/Zi"])
        else:
            mode_compile_flags.extend(["-O0", "-g"])
    elif compiler_family == "clang":
        mode_compile_flags.extend(["-Wshadow", "-O3"])
    elif compiler_family in {"gnu", "unknown"}:
        mode_compile_flags.append("-O4")
    else:
        mode_compile_flags.append("/O2")

    platform_compile_flags = []
    platform_link_flags = []
    if brew_prefix:
        platform_compile_flags.append(f"-I{brew_prefix}/include")
        platform_link_flags.append(f"-L{brew_prefix}/lib")

    if deployment_target and is_darwin:
        deployment_flag = f"-mmacosx-version-min={deployment_target}"
        platform_compile_flags.append(deployment_flag)
        platform_link_flags.append(deployment_flag)

    openmp_compile_flags = []
    openmp_link_flags = []
    if openmp:
        if compiler_family == "clang":
            openmp_compile_flags.extend(["-Xpreprocessor", "-fopenmp"])
            openmp_link_flags.append("-lomp")
            if libomp_prefix:
                platform_compile_flags.append(f"-I{libomp_prefix}/include")
                platform_link_flags.append(f"-L{libomp_prefix}/lib")
        elif compiler_family in {"gnu", "unknown"}:
            openmp_compile_flags.append("-fopenmp")
            openmp_link_flags.append("-fopenmp")

    compile_flags = _dedupe(
        base_compile_flags
        + mode_compile_flags
        + openmp_compile_flags
        + platform_compile_flags
        + _split_flags(cppflags)
        + _split_flags(cxxflags)
    )
    link_flags = _dedupe(platform_link_flags + openmp_link_flags + _split_flags(ldflags))

    return {
        "mode": mode,
        "openmp": bool(openmp),
        "platform": platform_name,
        "compiler": compiler,
        "compiler_family": compiler_family,
        "brew_prefix": brew_prefix,
        "libomp_prefix": libomp_prefix,
        "deployment_target": deployment_target,
        "compile_flags": compile_flags,
        "link_flags": link_flags,
    }


def _field_value(config, field):
    value = config[field]
    if isinstance(value, list):
        return " ".join(value)
    if isinstance(value, bool):
        return "1" if value else "0"
    return str(value)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("format", choices=["json", "field"])
    parser.add_argument("--field", choices=[
        "mode",
        "openmp",
        "platform",
        "compiler",
        "compiler_family",
        "brew_prefix",
        "libomp_prefix",
        "deployment_target",
        "compile_flags",
        "link_flags",
    ])
    parser.add_argument("--mode", default="release", choices=["release", "debug"])
    parser.add_argument("--openmp", default="1")
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--cppflags", default=os.environ.get("CPPFLAGS", ""))
    parser.add_argument("--cxxflags", default=os.environ.get("CXXFLAGS", ""))
    parser.add_argument("--ldflags", default=os.environ.get("LDFLAGS", ""))
    parser.add_argument("--deployment-target", default=os.environ.get("MACOSX_DEPLOYMENT_TARGET", ""))
    parser.add_argument("--platform", default=sys.platform)
    args = parser.parse_args()

    config = resolve_build_config(
        mode=args.mode,
        openmp=_norm_openmp(args.openmp),
        compiler=args.compiler,
        cppflags=args.cppflags,
        cxxflags=args.cxxflags,
        ldflags=args.ldflags,
        deployment_target=args.deployment_target,
        platform_name=args.platform,
    )

    if args.format == "json":
        json.dump(config, sys.stdout)
        sys.stdout.write("\n")
    else:
        if not args.field:
            parser.error("--field is required when format=field")
        sys.stdout.write(_field_value(config, args.field))
        sys.stdout.write("\n")


if __name__ == "__main__":
    main()
