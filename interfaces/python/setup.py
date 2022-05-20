import codecs
import fnmatch
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from sysconfig import get_config_var

import package_meta
from pkg_resources import parse_version
from setuptools import Extension, setup


def get_compiler():
    cxx = os.environ.get("CXX", get_config_var("CXX"))
    if cxx is None:
        return "g++"  # need a better way to check this
    return cxx.split()[0]


def is_clang():
    return b"clang" in subprocess.check_output([get_compiler(), "--version"])


def have_homebrew():
    return shutil.which("brew") is not None


def get_homebrew_include():
    brew_path = subprocess.check_output(["brew", "--prefix"], encoding="utf8")
    brew_path = brew_path[:-1]  # Remove trailing newline
    brew_path = Path(brew_path)

    # returns (compile_args, link_args)
    return (
        "-I" + str(brew_path.joinpath("include")),
        "-L" + str(brew_path.joinpath("lib")),
    )


test_openmp = """#include <omp.h>
#include <cstdio>

int main() {
#pragma omp parallel
  printf("Hello from thread %d, nthreads %d\\n", omp_get_thread_num(), omp_get_num_threads());
}
"""


def have_openmp():
    # Create a temporary directory
    tmpdir = tempfile.mkdtemp()
    cwd = Path.cwd()
    os.chdir(tmpdir)

    filename = "test-openmp.cpp"
    with open(os.path.join(tmpdir, filename), "w") as f:
        f.write(test_openmp)

    compiler = get_compiler()
    compile_args = ["-fopenmp"]

    if is_clang():
        compile_args.insert(0, "-Xpreprocessor")
        compile_args.append("-lomp")

    if have_homebrew():
        compile_args.extend(get_homebrew_include())

    try:
        with open(os.devnull, "w") as fnull:
            exit_code = subprocess.call(
                [compiler, *compile_args, filename], stdout=fnull, stderr=fnull
            )
    except OSError:
        exit_code = 1

    # Clean up
    os.chdir(cwd)
    shutil.rmtree(tmpdir)

    return exit_code == 0


here = os.path.abspath(os.path.dirname(__file__))

# Get the long description from the relevant file
with codecs.open(os.path.join(here, "README.rst"), encoding="utf-8") as f:
    long_description = f.read()

sources = ["./infomap_wrap.cpp"]
headers = []
for root, dirnames, filenames in os.walk("./src"):
    for filename in fnmatch.filter(filenames, "*.cpp"):
        sources.append(os.path.join(root, filename))
    for filename in fnmatch.filter(filenames, "*.h"):
        headers.append(os.path.join(root, filename))

# Set minimum Mac OS X version to 10.9 to pick up C++ standard library
if (
    get_config_var("MACOSX_DEPLOYMENT_TARGET")
    and "MACOSX_DEPLOYMENT_TARGET" not in os.environ
):
    if parse_version(get_config_var("MACOSX_DEPLOYMENT_TARGET")) < parse_version(
        "10.9"
    ):
        os.environ["MACOSX_DEPLOYMENT_TARGET"] = "10.9"
    else:
        os.environ["MACOSX_DEPLOYMENT_TARGET"] = get_config_var(
            "MACOSX_DEPLOYMENT_TARGET"
        )

compiler_args = [
    "-DAS_LIB",
    "-DPYTHON",
    "-Wno-deprecated-register",
    "-Wno-deprecated-declarations",
    "-std=c++14",
]

link_args = []

if "CXXFLAGS" in os.environ:
    compiler_args.extend(os.environ["CXXFLAGS"].split())

if "LDFLAGS" in os.environ:
    link_args.extend(os.environ["LDFLAGS"].split())

if have_openmp():
    print("Building with OpenMP support")
    if is_clang():
        compiler_args.append("-Xpreprocessor")
        link_args.append("-lomp")
    else:
        link_args.append("-fopenmp")
    compiler_args.append("-fopenmp")

    if have_homebrew():
        include_dir, link_dir = get_homebrew_include()
        compiler_args.append(include_dir)
        link_args.append(link_dir)
else:
    print("Warning: building without OpenMP support")

if sys.platform == "win32":
    # Not executed if we are on WSL
    compiler_args = [
        "/DAS_LIB",
        "/DPYTHON",
        "/DNOMINMAX",
    ]

infomap_module = Extension(
    "_infomap",
    sources=sources,
    include_dirs=[
        "headers",
        "headers/src",
        "headers/src/core",
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
