import os
import sys
import importlib.util
from pathlib import Path

from setuptools import Extension, setup


REPO_ROOT = Path(__file__).resolve().parent


def repo_rel(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def load_build_config():
    build_config_path = REPO_ROOT / "scripts" / "build_config.py"
    spec = importlib.util.spec_from_file_location("build_config", build_config_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def collect_cpp_sources():
    generated = REPO_ROOT / "interfaces" / "python" / "generated" / "infomap_wrap.cpp"
    sources = [repo_rel(generated)]
    for path in sorted((REPO_ROOT / "src").rglob("*.cpp")):
        sources.append(repo_rel(path))
    return sources


build_config = load_build_config()
resolve_build_config = build_config.resolve_build_config
norm_openmp = build_config._norm_openmp

shared_build = resolve_build_config(
    mode=os.environ.get("MODE", "release"),
    openmp=norm_openmp(os.environ.get("OPENMP", "1")),
    compiler=os.environ.get("CXX", "c++"),
    cppflags=os.environ.get("CPPFLAGS", ""),
    cxxflags=os.environ.get("CXXFLAGS", ""),
    ldflags=os.environ.get("LDFLAGS", ""),
    deployment_target=os.environ.get("MACOSX_DEPLOYMENT_TARGET", ""),
    platform_name=sys.platform,
)

compiler_args = list(shared_build["compile_flags"])
link_args = list(shared_build["link_flags"])

define_macros = [
    ("AS_LIB", None),
    ("PYTHON", None),
]

if sys.platform == "win32":
    define_macros.append(("NOMINMAX", None))


setup(
    ext_modules=[
        Extension(
            "infomap._infomap",
            sources=collect_cpp_sources(),
            include_dirs=[
                repo_rel(REPO_ROOT),
                repo_rel(REPO_ROOT / "src"),
                repo_rel(REPO_ROOT / "src" / "core"),
                repo_rel(REPO_ROOT / "src" / "core" / "iterators"),
                repo_rel(REPO_ROOT / "src" / "io"),
                repo_rel(REPO_ROOT / "src" / "utils"),
            ],
            define_macros=define_macros,
            language="c++",
            extra_compile_args=compiler_args,
            extra_link_args=link_args,
        )
    ]
)
