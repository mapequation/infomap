import fnmatch
import os
import sys
import importlib.util
from pathlib import Path
from setuptools import Extension, setup


def load_package_meta():
    package_meta_path = Path(__file__).resolve().with_name("package_meta.py")
    spec = importlib.util.spec_from_file_location("package_meta", package_meta_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


package_meta = load_package_meta()


def load_build_config():
    local_build_config = Path(__file__).resolve().with_name("build_config.py")
    if not local_build_config.exists():
        local_build_config = Path(__file__).resolve().parents[2] / "scripts" / "build_config.py"
    spec = importlib.util.spec_from_file_location("build_config", local_build_config)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


here = os.path.abspath(os.path.dirname(__file__))

# Get the long description from the relevant file
with open(os.path.join(here, "README.rst"), encoding="utf-8") as f:
    long_description = f.read()

sources = ["./infomap_wrap.cpp"]
headers = []
for root, dirnames, filenames in os.walk("./src"):
    for filename in fnmatch.filter(filenames, "*.cpp"):
        sources.append(os.path.join(root, filename))
    for filename in fnmatch.filter(filenames, "*.h"):
        headers.append(os.path.join(root, filename))


build_config = load_build_config()
shared_build = build_config.resolve_build_config(
    mode=os.environ.get("MODE", "release"),
    openmp=os.environ.get("OPENMP", "1"),
    compiler=os.environ.get("CXX", "c++"),
    cppflags=os.environ.get("CPPFLAGS", ""),
    cxxflags=os.environ.get("CXXFLAGS", ""),
    ldflags=os.environ.get("LDFLAGS", ""),
    deployment_target=os.environ.get("MACOSX_DEPLOYMENT_TARGET", ""),
    platform_name=sys.platform,
)

compiler_args = list(shared_build["compile_flags"])
link_args = list(shared_build["link_flags"])

if sys.platform == "win32":
    # Not executed if we are on WSL
    compiler_args = compiler_args + [
        "/DAS_LIB",
        "/DPYTHON",
        "/DNOMINMAX",
    ]
else:
    compiler_args = [
        "-DAS_LIB",
        "-DPYTHON",
        *compiler_args,
    ]

if shared_build["openmp"]:
    print("Building with OpenMP support")
else:
    print("Warning: building without OpenMP support")


infomap_module = Extension(
    "_infomap",
    sources=sources,
    include_dirs=[
        "headers",
        "headers/src",
        "headers/src/core",
        "headers/src/core/iterators",
        "headers/src/io",
        "headers/src/utils",
    ],
    language="c++",
    extra_compile_args=compiler_args,
    extra_link_args=link_args,
)

setup(
    name=package_meta.__name__,
    version=package_meta.__version__,
    description=package_meta.__description__,
    long_description=long_description,
    url=package_meta.__url__,
    author=package_meta.__author__,
    author_email=package_meta.__email__,
    license=package_meta.__license__,
    classifiers=package_meta.__classifiers__,
    keywords=package_meta.__keywords__,
    py_modules=["infomap", "package_meta"],
    entry_points={
        "console_scripts": [
            "infomap=infomap:main",
        ],
    },
    project_urls={
        "Bug Reports": package_meta.__issues__,
        "Source": package_meta.__repo__,
    },
    include_package_data=True,
    ext_modules=[infomap_module],
    python_requires=">=3",
)
