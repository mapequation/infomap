# Nanobind spike build (#743). Not wired into packaging; build manually:
#
#   cmake -S spikes/nanobind -B build/nanobind-spike \
#     -DCMAKE_BUILD_TYPE=Release \
#     -Dnanobind_DIR=$(.venv/bin/python -m nanobind --cmake_dir)
#   cmake --build build/nanobind-spike -j
#
# Mirrors the SWIG extension's build inputs: every src/*.cpp, -DAS_LIB,
# C++17, OpenMP, -O3.

cmake_minimum_required(VERSION 3.18)
project(infomap_nanobind_spike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Python 3.11 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(OpenMP REQUIRED)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
file(GLOB_RECURSE INFOMAP_SOURCES "${REPO_ROOT}/src/*.cpp")

nanobind_add_module(_infomap_nb STABLE_ABI infomap_nb.cpp ${INFOMAP_SOURCES})
target_include_directories(_infomap_nb PRIVATE
  "${REPO_ROOT}"
  "${REPO_ROOT}/vendor/fmt/include"
  "${REPO_ROOT}/vendor/nlohmann_json/include")
target_compile_definitions(_infomap_nb PRIVATE AS_LIB)
target_link_libraries(_infomap_nb PRIVATE OpenMP::OpenMP_CXX)
# The SWIG extension exports the engine's symbols with default visibility;
# loading both extensions in one process interposes them across modules and
# breaks whichever loads second. Hide everything but the module init so the
# spike coexists with the SWIG backend (needed for parity comparisons).
target_compile_options(_infomap_nb PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
target_link_options(_infomap_nb PRIVATE -Wl,--exclude-libs,ALL)
