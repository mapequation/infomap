import ast
import importlib.util
import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
BUILD_CONFIG_PATH = REPO_ROOT / "scripts" / "build_config.py"
PYTHON_MK_PATH = REPO_ROOT / "mk" / "python.mk"
SETUP_PY_PATH = REPO_ROOT / "setup.py"


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

    for flag in [
        "-Wall",
        "-Wextra",
        "-Wshadow",
        "-pedantic",
        "-Wnon-virtual-dtor",
        "-std=c++14",
    ]:
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


def test_features_are_disabled_by_default():
    config = resolve_build_config(
        platform_name="linux",
        compiler="clang++",
        mode="release",
        openmp=False,
    )

    assert config["enabled_features"] == []
    assert config["enabled_feature_defines"] == []
    assert "-DINFOMAP_USE_SIMD_LOG=1" not in config["compile_flags"]
    assert "-DINFOMAP_FEATURE_REGULARIZED_MULTILAYER=1" not in config["compile_flags"]
    assert "-DINFOMAP_FEATURE_TEST_FEATURE=1" not in config["compile_flags"]
    assert "INFOMAP_ENABLED_FEATURES" not in " ".join(config["compile_flags"])


def test_explicit_feature_emits_compile_define():
    config = resolve_build_config(
        platform_name="linux",
        compiler="clang++",
        mode="release",
        openmp=False,
        features=["test-feature"],
    )

    assert config["enabled_features"] == ["test-feature"]
    assert config["enabled_feature_defines"] == [
        "INFOMAP_FEATURE_TEST_FEATURE=1",
        'INFOMAP_ENABLED_FEATURES="test-feature"',
    ]
    assert "-DINFOMAP_FEATURE_TEST_FEATURE=1" in config["compile_flags"]
    assert '-DINFOMAP_ENABLED_FEATURES=\\"test-feature\\"' in config["compile_flags"]
    assert "-DINFOMAP_FEATURE_TEST_FEATURE=1" not in config["cmake_compile_flags"]
    assert "INFOMAP_ENABLED_FEATURES" not in " ".join(config["cmake_compile_flags"])


def test_simd_log_is_feature():
    config = resolve_build_config(
        platform_name="linux",
        compiler="clang++",
        mode="release",
        openmp=False,
        features=["simd-log"],
    )

    assert config["enabled_features"] == ["simd-log"]
    assert config["enabled_feature_defines"] == [
        "INFOMAP_USE_SIMD_LOG=1",
        'INFOMAP_ENABLED_FEATURES="simd-log"',
    ]
    assert "-DINFOMAP_USE_SIMD_LOG=1" in config["compile_flags"]
    assert '-DINFOMAP_ENABLED_FEATURES=\\"simd-log\\"' in config["compile_flags"]


def test_regularized_multilayer_is_feature():
    config = resolve_build_config(
        platform_name="linux",
        compiler="clang++",
        mode="release",
        openmp=False,
        features=["regularized-multilayer"],
    )

    assert config["enabled_features"] == ["regularized-multilayer"]
    assert config["enabled_feature_defines"] == [
        "INFOMAP_FEATURE_REGULARIZED_MULTILAYER=1",
        'INFOMAP_ENABLED_FEATURES="regularized-multilayer"',
    ]
    assert "-DINFOMAP_FEATURE_REGULARIZED_MULTILAYER=1" in config["compile_flags"]
    assert (
        '-DINFOMAP_ENABLED_FEATURES=\\"regularized-multilayer\\"'
        in config["compile_flags"]
    )


def test_features_use_registry_order():
    config = resolve_build_config(
        platform_name="linux",
        compiler="clang++",
        mode="release",
        openmp=False,
        features="test-feature,regularized-multilayer,simd-log",
    )

    assert config["enabled_features"] == [
        "simd-log",
        "regularized-multilayer",
        "test-feature",
    ]
    assert config["enabled_feature_defines"][-1] == (
        'INFOMAP_ENABLED_FEATURES="simd-log,regularized-multilayer,test-feature"'
    )


def test_explicit_feature_uses_msvc_define_syntax():
    config = resolve_build_config(
        platform_name="win32",
        compiler="cl.exe",
        openmp=False,
        features="test-feature",
    )

    assert config["enabled_features"] == ["test-feature"]
    assert config["enabled_feature_defines"] == [
        "INFOMAP_FEATURE_TEST_FEATURE=1",
        'INFOMAP_ENABLED_FEATURES="test-feature"',
    ]
    assert "/DINFOMAP_FEATURE_TEST_FEATURE=1" in config["compile_flags"]
    assert '/DINFOMAP_ENABLED_FEATURES=\\"test-feature\\"' in config["compile_flags"]


def test_unknown_feature_fails_early():
    try:
        resolve_build_config(
            platform_name="linux",
            compiler="clang++",
            openmp=False,
            features=["unknown-feature"],
        )
    except ValueError as error:
        assert "Unknown feature 'unknown-feature'" in str(error)
    else:
        raise AssertionError("unknown feature did not fail")


def test_python_make_build_env_passes_features():
    python_mk = PYTHON_MK_PATH.read_text(encoding="utf-8")

    assert 'FEATURES="$(FEATURES)"' in python_mk


def test_python_make_build_env_passes_build_jobs():
    python_mk = PYTHON_MK_PATH.read_text(encoding="utf-8")

    assert 'INFOMAP_BUILD_JOBS="$(JOBS)"' in python_mk


def test_make_export_matches_per_field_values():
    config = resolve_build_config(
        platform_name="linux",
        compiler="clang++",
        openmp=True,
    )

    lines = build_config._make_export(config).splitlines()

    # One assignment per mapped field, each a parseable Make assignment.
    assert len(lines) == len(build_config.MAKE_EXPORT_FIELDS)
    rendered = {}
    for line in lines:
        name, sep, value = line.partition(" := ")
        assert sep, f"not a Make assignment: {line!r}"
        assert re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", name), name
        rendered[name] = value

    # Values must equal the prior per-field rendering (with `$` escaped).
    for make_var, field in build_config.MAKE_EXPORT_FIELDS:
        expected = build_config._field_value(config, field).replace("$", "$$")
        assert rendered[make_var] == expected


def test_make_export_escapes_dollar_signs():
    config = resolve_build_config(
        platform_name="linux",
        compiler="clang++",
        openmp=False,
        cxxflags="-DPATH=$ORIGIN",
    )

    raw = build_config._field_value(config, "compile_flags")
    assert "$ORIGIN" in raw and "$$" not in raw

    # On `include`, an unescaped `$` would be expanded by Make, so it is doubled.
    assert "$$ORIGIN" in build_config._make_export(config)


def test_setup_py_passes_features_env_to_build_config():
    setup_tree = ast.parse(SETUP_PY_PATH.read_text(encoding="utf-8"))

    resolve_calls = [
        node
        for node in ast.walk(setup_tree)
        if isinstance(node, ast.Call)
        and isinstance(node.func, ast.Name)
        and node.func.id == "resolve_build_config"
    ]

    assert len(resolve_calls) == 1
    feature_keywords = [
        keyword for keyword in resolve_calls[0].keywords if keyword.arg == "features"
    ]
    assert len(feature_keywords) == 1
    feature_value = feature_keywords[0].value
    assert isinstance(feature_value, ast.Call)
    assert isinstance(feature_value.func, ast.Attribute)
    assert feature_value.func.attr == "get"
    assert isinstance(feature_value.func.value, ast.Attribute)
    assert feature_value.func.value.attr == "environ"
    assert isinstance(feature_value.func.value.value, ast.Name)
    assert feature_value.func.value.value.id == "os"
    assert [arg.value for arg in feature_value.args] == ["FEATURES", ""]


def test_setup_py_unescapes_string_defines_for_setuptools():
    setup_py = SETUP_PY_PATH.read_text(encoding="utf-8")

    assert (
        "arg.replace('\\\\\"', '\"') for arg in shared_build[\"compile_flags\"]"
        in setup_py
    )
