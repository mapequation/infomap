MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
DEFAULT_JOBS := $(shell command -v nproc >/dev/null 2>&1 && nproc || sysctl -n hw.logicalcpu 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
JOBS ?= $(DEFAULT_JOBS)
CCACHE_BIN := $(shell command -v ccache 2>/dev/null || true)
USE_CCACHE ?= $(if $(CCACHE_BIN),1,0)

ifeq ($(filter -j% --jobserver% --jobserver-auth=%,$(MAKEFLAGS)),)
ifneq ($(JOBS),1)
MAKEFLAGS += -j$(JOBS)
endif
endif

CXX ?= c++
AR ?= ar
PYTHON ?= $(shell command -v python 2>/dev/null || command -v python3 2>/dev/null || echo python3)
PYTHON_FOR_BUILD_CONFIG ?= $(shell command -v $(PYTHON) 2>/dev/null || command -v python3 2>/dev/null || echo python3)
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
BUILD_CONFIG_SCRIPT := scripts/build_config.py

define build_config_field
$(strip $(shell CPPFLAGS='$(CPPFLAGS)' CXXFLAGS='$(CXXFLAGS)' LDFLAGS='$(LDFLAGS)' MACOSX_DEPLOYMENT_TARGET='$(MACOSX_DEPLOYMENT_TARGET)' $(PYTHON_FOR_BUILD_CONFIG) $(BUILD_CONFIG_SCRIPT) field --field "$(1)" --mode "$(MODE)" --openmp "$(OPENMP)" --compiler "$(CXX)" --platform "$(UNAME_S)"))
endef

BUILD_CONFIG_MODE := $(call build_config_field,mode)
BUILD_CONFIG_OPENMP := $(call build_config_field,openmp)
BUILD_CONFIG_PLATFORM := $(call build_config_field,platform)
BUILD_CONFIG_COMPILER := $(call build_config_field,compiler)
BUILD_CONFIG_COMPILER_FAMILY := $(call build_config_field,compiler_family)
BUILD_CONFIG_BREW_PREFIX := $(call build_config_field,brew_prefix)
BUILD_CONFIG_LIBOMP_PREFIX := $(call build_config_field,libomp_prefix)
BUILD_CONFIG_DEPLOYMENT_TARGET := $(call build_config_field,deployment_target)
NATIVE_CXXFLAGS := $(call build_config_field,compile_flags)
NATIVE_LDFLAGS := $(call build_config_field,link_flags)
CXX_COMPILE := $(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),$(CCACHE_BIN) ,)$(CXX)

.PHONY: help doctor dev-bootstrap clean

help:
	@printf "%s\n" \
		"Infomap Make targets" \
		"" \
		"Build" \
		"  build-native          Build the Infomap binary (MODE=release|debug, OPENMP=0|1)." \
		"  build-lib             Build the static C++ library and exported headers." \
		"  build-python          Generate the SWIG wrapper and build the local Python extension." \
		"  build-js-metadata     Refresh the tracked JS parameters/changelog metadata." \
		"  build-js              Build the JS worker bundle and npm package assets from tracked metadata." \
		"  build-docs            Refresh the committed Python docs site in docs/." \
		"" \
		"Test" \
		"  test-native           Configure, build, and run the C++ doctest suite via CMake/CTest." \
		"  test-python           Run the full Python verification suite." \
		"  test-python-unit      Run pytest for test/python." \
		"  test-python-doctest   Run Python doctests and ruff checks for the built package." \
		"  test-python-examples  Run the Python example smoke tests." \
		"  test-docs             Rebuild docs in a temp dir and verify committed docs/ is fresh." \
		"  test-js-metadata      Regenerate JS metadata in a temp dir and verify tracked files are current." \
		"  test-js               Pack the npm package and smoke-test the browser example." \
		"  test-fast             Run the fast native + Python feedback suite." \
		"  test-sanitizers       Run the C++ test suite under ASan/UBSan." \
		"  bench-native          Run the native benchmark and memory baseline harness." \
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
		"  build-docs            Refresh the committed Python docs site after installing the local package." \
		"  test-docs             Verify committed docs/ matches a fresh Sphinx build." \
		"  release-python-dist   Build local sdist and wheel artifacts from build/py." \
		"  release-python-testpypi  Publish the built distributions to TestPyPI." \
		"  release-python-pypi      Publish the built distributions to PyPI." \
		"" \
		"CI / Advanced" \
		"  ci-export-github-env  Print GitHub Actions environment exports for macOS libomp." \
		"  test-js-internal      Smoke-build the internal-supported react/parser JS packages." \
		"  Advanced/internal targets such as R and docker-* remain available but are no longer primary." \
		"" \
		"Examples" \
		"  make build-native" \
		"  make build-native JOBS=1" \
		"  make build-native MODE=debug OPENMP=0" \
		"  make build-python dev-python-install test-python-unit" \
		"  make build-js-metadata test-js-metadata" \
		"  make build-js test-js" \
		"  make build-docs" \
		"  make test-docs" \
		"" \
		"Legacy aliases like make python, make js-worker, make cpp-test, and make py-test still work."

doctor:
	@printf "%s\n" "Infomap doctor" ""
	@printf "Platform: %s\n" "$(UNAME_S)"
	@printf "Mode: MODE=%s OPENMP=%s\n" "$(MODE)" "$(OPENMP)"
	@printf "Parallel jobs: %s\n" "$(JOBS)"
	@printf "ccache: %s\n" "$(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),$(CCACHE_BIN),disabled)"
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
		printf "brew prefix: %s\n" "$(if $(BUILD_CONFIG_BREW_PREFIX),$(BUILD_CONFIG_BREW_PREFIX),missing)"; \
		printf "libomp prefix: %s\n" "$(if $(BUILD_CONFIG_LIBOMP_PREFIX),$(BUILD_CONFIG_LIBOMP_PREFIX),missing)"; \
		printf "deployment target: %s\n" "$(if $(BUILD_CONFIG_DEPLOYMENT_TARGET),$(BUILD_CONFIG_DEPLOYMENT_TARGET),unset)"; \
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
