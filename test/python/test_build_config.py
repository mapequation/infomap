import importlib.util
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
BUILD_CONFIG_PATH = REPO_ROOT / "scripts" / "build_config.py"


def load_build_config():
    spec = importlib.util.spec_from_file_location("build_config", BUILD_CONFIG_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


build_config = load_build_config()
resolve_build_config = build_config.resolve_build_config


def test_windows_cl_exe_uses_msvc_flags():
    config = resolve_build_config(platform_name="win32", compiler="cl.exe", openmp=True)

    assert config["compiler_family"] == "msvc"
    assert "/std:c++14" in config["compile_flags"]
    assert "-Wextra" not in config["compile_flags"]
    assert "-fopenmp" not in config["compile_flags"]


def test_windows_quoted_cl_path_uses_msvc_flags():
    compiler = (
        '"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\MSVC\\'
        '14.44.35207\\bin\\HostX86\\x64\\cl.exe"'
    )
    config = resolve_build_config(platform_name="win32", compiler=compiler, openmp=True)

    assert config["compiler_family"] == "msvc"
    assert "/std:c++14" in config["compile_flags"]
    assert "-Wextra" not in config["compile_flags"]
    assert "-fopenmp" not in config["compile_flags"]


def test_windows_unknown_compiler_defaults_to_msvc_flags():
    config = resolve_build_config(platform_name="win32", compiler="c++", openmp=True)

    assert config["compiler_family"] == "msvc"
    assert "/std:c++14" in config["compile_flags"]
    assert "-Wextra" not in config["compile_flags"]
    assert "-fopenmp" not in config["compile_flags"]


def test_clang_debug_and_release_share_warning_policy():
    debug_config = resolve_build_config(
        platform_name="linux",
        compiler="clang++",
        mode="debug",
        openmp=False,
    )
    release_config = resolve_build_config(
        platform_name="linux",
        compiler="clang++",
        mode="release",
        openmp=False,
    )

    for flag in ["-Wall", "-Wextra", "-Wshadow", "-pedantic", "-Wnon-virtual-dtor", "-std=c++14"]:
        assert flag in debug_config["compile_flags"]
        assert flag in release_config["compile_flags"]

    assert "-O0" in debug_config["compile_flags"]
    assert "-g" in debug_config["compile_flags"]
    assert "-O3" in release_config["compile_flags"]


def test_clang_without_openmp_drops_openmp_flags():
    config = resolve_build_config(
        platform_name="linux",
        compiler="clang++",
        mode="release",
        openmp=False,
    )

    assert "-Xpreprocessor" not in config["compile_flags"]
    assert "-fopenmp" not in config["compile_flags"]
    assert "-lomp" not in config["link_flags"]
