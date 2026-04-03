MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
BREW_PREFIX := $(shell brew --prefix 2>/dev/null || true)
LIBOMP_PREFIX := $(shell brew --prefix libomp 2>/dev/null || true)

CXX ?= c++
AR ?= ar
PYTHON ?= python
PIP ?= $(PYTHON) -m pip
PYTEST ?= $(PYTHON) -m pytest
RUFF ?= $(PYTHON) -m ruff
NPM ?= npm
NODE ?= node
SWIG ?= swig
EMXX ?= em++
SPHINX_BUILD_BIN := $(shell command -v sphinx-build 2>/dev/null || true)
SPHINX_BUILD ?= $(if $(SPHINX_BUILD_BIN),$(SPHINX_BUILD_BIN),$(PYTHON) -m sphinx)
CMAKE ?= $(shell command -v cmake 2>/dev/null || command -v /opt/homebrew/bin/cmake 2>/dev/null || echo cmake)
CTEST ?= $(shell command -v ctest 2>/dev/null || command -v /opt/homebrew/bin/ctest 2>/dev/null || echo ctest)

MODE ?= release
OPENMP ?= 1

VALID_MODES := release debug
VALID_OPENMP := 0 1

ifeq ($(filter $(MODE),$(VALID_MODES)),)
$(error MODE must be one of: $(VALID_MODES))
endif

ifeq ($(filter $(OPENMP),$(VALID_OPENMP)),)
$(error OPENMP must be one of: $(VALID_OPENMP))
endif

HEADERS := $(shell find src -name "*.h")
SOURCES := $(shell find src -name "*.cpp")
SWIG_FILES := $(shell find interfaces/swig -name "*.i")
MK_FILES := $(wildcard mk/*.mk)

.PHONY: help doctor dev-bootstrap clean

help:
	@printf "%s\n" \
		"Infomap Make targets" \
		"" \
		"Build" \
		"  build-native          Build the Infomap binary (MODE=release|debug, OPENMP=0|1)." \
		"  build-lib             Build the static C++ library and exported headers." \
		"  build-python          Generate the SWIG wrapper and build the local Python extension." \
		"  build-js              Build the JS worker bundle and npm package assets." \
		"  build-docs            Build the Python docs site into docs/." \
		"" \
		"Test" \
		"  test-native           Configure, build, and run the C++ doctest suite via CMake/CTest." \
		"  test-python           Run the full Python verification suite." \
		"  test-python-unit      Run pytest for test/python." \
		"  test-python-doctest   Run Python doctests and ruff checks for the built package." \
		"  test-python-examples  Run the Python example smoke tests." \
		"  test-js               Pack the npm package and smoke-test the browser example." \
		"  test-fast             Run the fast native + Python feedback suite." \
		"  test-sanitizers       Run the C++ test suite under ASan/UBSan." \
		"  bench-python          Run the nightly-style Python benchmark harness." \
		"" \
		"Dev" \
		"  help                  Show this guide." \
		"  doctor                Inspect tool availability and active build flags." \
		"  dev-bootstrap         Install Python dev dependencies and run npm ci." \
		"  dev-python-install    Install the built Python package in editable mode." \
		"  clean                 Remove native, Python, and JS build outputs." \
		"  clean-native          Remove native build artifacts, libraries, and CMake build dirs." \
		"  clean-python          Remove Python build outputs." \
		"  clean-js              Remove JS build and pack outputs." \
		"" \
		"Docs / Release" \
		"  build-docs            Build the Python docs site after installing the local package." \
		"  release-python-dist   Build local sdist and wheel artifacts from build/py." \
		"  release-python-testpypi  Publish the built distributions to TestPyPI." \
		"  release-python-pypi      Publish the built distributions to PyPI." \
		"" \
		"CI / Advanced" \
		"  ci-export-github-env  Print GitHub Actions environment exports for macOS libomp." \
		"  Advanced/internal targets such as R and docker-* remain available but are no longer primary." \
		"" \
		"Examples" \
		"  make build-native" \
		"  make build-native MODE=debug OPENMP=0" \
		"  make build-python dev-python-install test-python-unit" \
		"  make build-js test-js" \
		"  make build-docs" \
		"" \
		"Legacy aliases like make python, make js-worker, make cpp-test, and make py-test still work."

doctor:
	@printf "%s\n" "Infomap doctor" ""
	@printf "Platform: %s\n" "$(UNAME_S)"
	@printf "Mode: MODE=%s OPENMP=%s\n" "$(MODE)" "$(OPENMP)"
	@printf "make: %s\n" "$$(command -v $(MAKE) 2>/dev/null || echo missing)"
	@printf "cxx (%s): %s\n" "$(CXX)" "$$(command -v $(CXX) 2>/dev/null || echo missing)"
	@printf "python (%s): %s\n" "$(PYTHON)" "$$(command -v $(PYTHON) 2>/dev/null || echo missing)"
	@printf "swig (%s): %s\n" "$(SWIG)" "$$(command -v $(SWIG) 2>/dev/null || echo missing)"
	@printf "node (%s): %s\n" "$(NODE)" "$$(command -v $(NODE) 2>/dev/null || echo missing)"
	@printf "npm (%s): %s\n" "$(NPM)" "$$(command -v $(NPM) 2>/dev/null || echo missing)"
	@printf "sphinx (%s): %s\n" "$(SPHINX_BUILD)" "$(if $(SPHINX_BUILD_BIN),$(SPHINX_BUILD_BIN),python -m sphinx)"
	@printf "cmake (%s): %s\n" "$(CMAKE)" "$$(command -v $(CMAKE) 2>/dev/null || echo missing)"
	@printf "ctest (%s): %s\n" "$(CTEST)" "$$(command -v $(CTEST) 2>/dev/null || echo missing)"
	@printf "em++ (%s): %s\n" "$(EMXX)" "$$(command -v $(EMXX) 2>/dev/null || echo missing)"
	@printf "NATIVE_CXXFLAGS=%s\n" "$(NATIVE_CXXFLAGS)"
	@printf "NATIVE_LDFLAGS=%s\n" "$(NATIVE_LDFLAGS)"
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		printf "brew prefix: %s\n" "$(if $(BREW_PREFIX),$(BREW_PREFIX),missing)"; \
		printf "libomp prefix: %s\n" "$(if $(LIBOMP_PREFIX),$(LIBOMP_PREFIX),missing)"; \
		printf "CMake extra cxx flags: %s\n" "$(CMAKE_EXTRA_CXX_FLAGS)"; \
		printf "CMake extra link flags: %s\n" "$(CMAKE_EXTRA_LINK_FLAGS)"; \
	fi

dev-bootstrap:
	@$(PIP) install -r requirements_dev.txt
	@$(NPM) ci
	@printf "%s\n" \
		"Bootstrap complete." \
		"Next steps:" \
		"  make build-native" \
		"  make build-python dev-python-install" \
		"  make test-fast"

clean: clean-native clean-python clean-js
	@true
