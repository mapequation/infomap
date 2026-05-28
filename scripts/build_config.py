#!/usr/bin/env python3

import argparse
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path


FEATURE_REGISTRY = {
    "simd-log": {
        "define": "INFOMAP_USE_SIMD_LOG",
        "description": "Batch plogp's log2 calls through an inlined SIMD polynomial.",
        "requires": [],
        "conflicts": [],
    },
    "regularized-multilayer": {
        "define": "INFOMAP_FEATURE_REGULARIZED_MULTILAYER",
        "description": "Enable experimental regularized flow for multilayer networks.",
        "requires": [],
        "conflicts": [],
    },
    "test-feature": {
        "define": "INFOMAP_FEATURE_TEST_FEATURE",
        "description": "Internal canary used to verify compile-time feature gates.",
        "requires": [],
        "conflicts": [],
    },
}


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


def _base_compile_flags(compiler_family):
    if compiler_family == "msvc":
        return ["/std:c++14"]

    flags = ["-Wall", "-Wextra", "-pedantic", "-Wnon-virtual-dtor", "-std=c++14"]
    if compiler_family in {"clang", "gnu"}:
        flags.append("-Wshadow")
    return flags


def _mode_compile_flags(mode, compiler_family):
    if mode == "debug":
        if compiler_family == "msvc":
            return ["/Od", "/Zi"]
        return ["-O0", "-g"]

    if compiler_family == "msvc":
        return ["/O2"]
    return ["-O3"]


def _native_arch_compile_flags(compiler_family):
    if compiler_family in {"clang", "gnu"}:
        return ["-march=native", "-flto", "-funroll-loops"]
    return []


def _native_arch_link_flags(compiler_family):
    if compiler_family in {"clang", "gnu"}:
        return ["-flto"]
    return []


def _normalize_features(features):
    if not features:
        return []
    if isinstance(features, str):
        candidates = features.replace(",", " ").split()
    else:
        candidates = list(features)

    requested = set()
    for feature in candidates:
        feature = str(feature).strip()
        if not feature:
            continue
        if feature not in FEATURE_REGISTRY:
            known = ", ".join(sorted(FEATURE_REGISTRY))
            raise ValueError(
                f"Unknown feature '{feature}'. Known features: {known}."
            )
        requested.add(feature)
    return [feature for feature in FEATURE_REGISTRY if feature in requested]


def _validate_features(features):
    enabled = set(features)
    for feature in features:
        spec = FEATURE_REGISTRY[feature]
        missing = [
            dependency for dependency in spec["requires"] if dependency not in enabled
        ]
        if missing:
            raise ValueError(
                f"Feature '{feature}' requires: {', '.join(missing)}."
            )
        conflicts = [conflict for conflict in spec["conflicts"] if conflict in enabled]
        if conflicts:
            raise ValueError(
                f"Feature '{feature}' conflicts with: {', '.join(conflicts)}."
            )


def _define_flag(compiler_family, define):
    if compiler_family == "msvc":
        return f"/D{define}=1"
    return f"-D{define}=1"


def _define_string_flag(compiler_family, define, value):
    if compiler_family == "msvc":
        return f'/D{define}=\\"{value}\\"'
    return f'-D{define}=\\"{value}\\"'


def _feature_compile_flags(compiler_family, features):
    flags = [
        _define_flag(compiler_family, FEATURE_REGISTRY[feature]["define"])
        for feature in features
    ]
    if features:
        flags.append(
            _define_string_flag(
                compiler_family,
                "INFOMAP_ENABLED_FEATURES",
                ",".join(features),
            )
        )
    return flags


def _feature_definitions(features):
    definitions = [f"{FEATURE_REGISTRY[feature]['define']}=1" for feature in features]
    if features:
        definitions.append(f'INFOMAP_ENABLED_FEATURES="{",".join(features)}"')
    return definitions


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
    native_arch=False,
    features=None,
):
    platform_name = platform_name or sys.platform
    compiler_family = _compiler_family_for_platform(compiler, platform_name)
    is_darwin = platform_name in {"darwin", "Darwin"}
    enabled_features = _normalize_features(features)
    _validate_features(enabled_features)

    brew_prefix = _brew_prefix() if is_darwin else ""
    libomp_prefix = _brew_prefix("libomp") if is_darwin else ""

    base_compile_flags = _base_compile_flags(compiler_family)
    mode_compile_flags = _mode_compile_flags(mode, compiler_family)
    native_compile_flags = (
        _native_arch_compile_flags(compiler_family)
        if native_arch and mode == "release"
        else []
    )
    native_link_flags = (
        _native_arch_link_flags(compiler_family)
        if native_arch and mode == "release"
        else []
    )
    feature_compile_flags = _feature_compile_flags(compiler_family, enabled_features)

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

    cmake_compile_flags = _dedupe(
        base_compile_flags
        + mode_compile_flags
        + native_compile_flags
        + openmp_compile_flags
        + platform_compile_flags
        + _split_flags(cppflags)
        + _split_flags(cxxflags)
    )
    compile_flags = _dedupe(cmake_compile_flags + feature_compile_flags)
    link_flags = _dedupe(
        platform_link_flags
        + native_link_flags
        + openmp_link_flags
        + _split_flags(ldflags)
    )

    return {
        "mode": mode,
        "openmp": bool(openmp),
        # Report native_arch as effective only when flags were actually emitted —
        # i.e. release mode AND a compiler family that we know how to tune
        # (clang/gnu). For MSVC or unknown toolchains the request is silently
        # ignored, and the reported field stays false to match reality.
        "native_arch": bool(native_compile_flags),
        "enabled_features": enabled_features,
        "enabled_feature_defines": _feature_definitions(enabled_features),
        "platform": platform_name,
        "compiler": compiler,
        "compiler_family": compiler_family,
        "brew_prefix": brew_prefix,
        "libomp_prefix": libomp_prefix,
        "deployment_target": deployment_target,
        "cmake_compile_flags": cmake_compile_flags,
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
    parser.add_argument(
        "--field",
        choices=[
            "mode",
            "openmp",
            "native_arch",
            "enabled_features",
            "enabled_feature_defines",
            "platform",
            "compiler",
            "compiler_family",
            "brew_prefix",
            "libomp_prefix",
            "deployment_target",
            "cmake_compile_flags",
            "compile_flags",
            "link_flags",
        ],
    )
    parser.add_argument("--mode", default="release", choices=["release", "debug"])
    parser.add_argument("--openmp", default="1")
    parser.add_argument("--native-arch", default="0")
    parser.add_argument("--features", default="")
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--cppflags", default=os.environ.get("CPPFLAGS", ""))
    parser.add_argument("--cxxflags", default=os.environ.get("CXXFLAGS", ""))
    parser.add_argument("--ldflags", default=os.environ.get("LDFLAGS", ""))
    parser.add_argument(
        "--deployment-target", default=os.environ.get("MACOSX_DEPLOYMENT_TARGET", "")
    )
    parser.add_argument("--platform", default=sys.platform)
    args = parser.parse_args()

    try:
        config = resolve_build_config(
            mode=args.mode,
            openmp=_norm_openmp(args.openmp),
            compiler=args.compiler,
            cppflags=args.cppflags,
            cxxflags=args.cxxflags,
            ldflags=args.ldflags,
            deployment_target=args.deployment_target,
            platform_name=args.platform,
            native_arch=_norm_openmp(args.native_arch),
            features=args.features,
        )
    except ValueError as error:
        parser.error(str(error))

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
