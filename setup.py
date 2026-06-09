import os
import sys
import importlib.util
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext


REPO_ROOT = Path(__file__).resolve().parent
SWIG_CPP_SOURCE = REPO_ROOT / "interfaces" / "python" / "generated" / "infomap_wrap.cpp"


def repo_rel(path: Path) -> str:
    return path.relative_to(REPO_ROOT).as_posix()


def load_build_config():
    build_config_path = REPO_ROOT / "scripts" / "build_config.py"
    spec = importlib.util.spec_from_file_location("build_config", build_config_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def collect_cpp_sources():
    sources = [repo_rel(SWIG_CPP_SOURCE)]
    for path in sorted((REPO_ROOT / "src").rglob("*.cpp")):
        sources.append(repo_rel(path))
    return sources


def swig_warning_suppression(compiler_family):
    if compiler_family == "msvc":
        return ["/w"]
    if compiler_family in {"clang", "gnu", "unknown"}:
        return ["-w"]
    return []


def resolve_build_jobs():
    env_jobs = os.environ.get("INFOMAP_BUILD_JOBS", "").strip()
    if env_jobs.isdigit() and int(env_jobs) > 0:
        return int(env_jobs)
    return os.cpu_count() or 1


class BuildExt(build_ext):
    def build_extensions(self):
        compiler = self.compiler
        original_compile = compiler._compile
        quiet_swig_flags = swig_warning_suppression(shared_build["compiler_family"])
        swig_source = str(SWIG_CPP_SOURCE)

        def compile_with_source_overrides(
            obj, src, ext, cc_args, extra_postargs, pp_opts
        ):
            source = os.path.abspath(src)
            compile_args = list(extra_postargs or [])
            if source == swig_source and quiet_swig_flags:
                compile_args.extend(quiet_swig_flags)
            return original_compile(obj, src, ext, cc_args, compile_args, pp_opts)

        compiler._compile = compile_with_source_overrides

        # Compile this extension's ~30 translation units concurrently.
        # setuptools' build_ext.parallel only parallelizes *across extensions*,
        # and the package ships a single extension, so we parallelize the
        # per-object compiles ourselves by overriding the compiler's compile
        # loop. Only the unix-style compiler drives compilation through the
        # _compile hook used above; MSVC has its own loop, so leave it serial.
        jobs = resolve_build_jobs()
        # Parallelizing a single extension's sources means reimplementing the
        # compiler's compile loop with distutils internals (_setup_compile /
        # _get_cc_args / _compile). Guard on their presence so a future setuptools
        # that renames or drops them simply falls back to the correct serial
        # build instead of erroring.
        parallelize = (
            jobs > 1
            and getattr(compiler, "compiler_type", None) in {"unix", "cygwin"}
            and all(
                hasattr(compiler, attr)
                for attr in ("_setup_compile", "_get_cc_args", "_compile")
            )
        )
        original_compile_method = compiler.compile

        def parallel_compile(
            sources,
            output_dir=None,
            macros=None,
            include_dirs=None,
            debug=0,
            extra_preargs=None,
            extra_postargs=None,
            depends=None,
        ):
            macros, objects, extra_postargs, pp_opts, build = compiler._setup_compile(
                output_dir, macros, include_dirs, sources, depends, extra_postargs
            )
            cc_args = compiler._get_cc_args(pp_opts, debug, extra_preargs)

            def compile_one(obj):
                try:
                    src, ext = build[obj]
                except KeyError:
                    return
                compiler._compile(obj, src, ext, cc_args, extra_postargs, pp_opts)

            with ThreadPoolExecutor(max_workers=jobs) as executor:
                # Iterate results so the first compile error propagates.
                for _ in executor.map(compile_one, objects):
                    pass
            return objects

        if parallelize:
            compiler.compile = parallel_compile
        try:
            super().build_extensions()
        finally:
            compiler._compile = original_compile
            if parallelize:
                compiler.compile = original_compile_method


build_config = load_build_config()
resolve_build_config = build_config.resolve_build_config
norm_openmp = build_config._norm_openmp
requested_build_compiler = os.environ.get("CXX", "")
if sys.platform == "win32" and requested_build_compiler in {"", "c++"}:
    default_build_compiler = "cl"
else:
    default_build_compiler = requested_build_compiler or "c++"

shared_build = resolve_build_config(
    mode=os.environ.get("MODE", "release"),
    openmp=norm_openmp(os.environ.get("OPENMP", "1")),
    compiler=default_build_compiler,
    cppflags=os.environ.get("CPPFLAGS", ""),
    cxxflags=os.environ.get("CXXFLAGS", ""),
    ldflags=os.environ.get("LDFLAGS", ""),
    deployment_target=os.environ.get("MACOSX_DEPLOYMENT_TARGET", ""),
    platform_name=sys.platform,
    native_arch=norm_openmp(os.environ.get("NATIVE_ARCH", "0")),
    features=os.environ.get("FEATURES", ""),
)

compiler_args = [arg.replace('\\"', '"') for arg in shared_build["compile_flags"]]
link_args = list(shared_build["link_flags"])

define_macros = [
    ("AS_LIB", None),
    ("PYTHON", None),
]

if sys.platform == "win32":
    define_macros.append(("NOMINMAX", None))


setup(
    cmdclass={"build_ext": BuildExt},
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
                repo_rel(REPO_ROOT / "vendor" / "nlohmann_json" / "include"),
                repo_rel(REPO_ROOT / "vendor" / "fmt" / "include"),
            ],
            define_macros=define_macros,
            language="c++",
            extra_compile_args=compiler_args,
            extra_link_args=link_args,
        )
    ],
)
