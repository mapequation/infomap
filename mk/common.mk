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
PYTHON_FOR_BUILD_CONFIG ?= $(shell command -v $(PYTHON) 2>/dev/null || command -v python3 2>/dev/null || true)
PIP ?= $(PYTHON) -m pip
PYTEST ?= $(PYTHON) -m pytest
RUFF ?= $(PYTHON) -m ruff
NPM ?= npm
NODE ?= node
SWIG ?= swig
EMXX ?= em++
R ?= R
RSCRIPT ?= Rscript
AIR ?= $(shell command -v air 2>/dev/null || command -v /opt/homebrew/bin/air 2>/dev/null || echo air)
SPHINX_BUILD_BIN := $(shell command -v sphinx-build 2>/dev/null || true)
SPHINX_BUILD ?= $(if $(SPHINX_BUILD_BIN),$(SPHINX_BUILD_BIN),$(PYTHON) -m sphinx)
CMAKE ?= $(shell command -v cmake 2>/dev/null || command -v /opt/homebrew/bin/cmake 2>/dev/null || echo cmake)
CTEST ?= $(shell command -v ctest 2>/dev/null || command -v /opt/homebrew/bin/ctest 2>/dev/null || echo ctest)
CLANG_FORMAT ?= $(shell command -v clang-format 2>/dev/null || command -v /opt/homebrew/bin/clang-format 2>/dev/null || command -v /opt/homebrew/opt/llvm/bin/clang-format 2>/dev/null || echo clang-format)
CLANG_TIDY ?= $(shell command -v clang-tidy 2>/dev/null || command -v /opt/homebrew/bin/clang-tidy 2>/dev/null || command -v /opt/homebrew/opt/llvm/bin/clang-tidy 2>/dev/null || echo clang-tidy)
CLANGD ?= $(shell command -v clangd 2>/dev/null || command -v /opt/homebrew/bin/clangd 2>/dev/null || command -v /opt/homebrew/opt/llvm/bin/clangd 2>/dev/null || echo clangd)
NINJA ?= $(shell command -v ninja 2>/dev/null || command -v /opt/homebrew/bin/ninja 2>/dev/null || echo ninja)

MODE ?= release
OPENMP ?= 1
NATIVE_ARCH ?= 0
FEATURES ?=

empty :=
space := $(empty) $(empty)

VALID_MODES := release debug
VALID_OPENMP := 0 1
VALID_NATIVE_ARCH := 0 1

ifeq ($(filter $(MODE),$(VALID_MODES)),)
$(error MODE must be one of: $(VALID_MODES))
endif

ifeq ($(filter $(OPENMP),$(VALID_OPENMP)),)
$(error OPENMP must be one of: $(VALID_OPENMP))
endif

ifeq ($(filter $(NATIVE_ARCH),$(VALID_NATIVE_ARCH)),)
$(error NATIVE_ARCH must be one of: $(VALID_NATIVE_ARCH))
endif

ifneq ($(FEATURES),)
ifeq ($(PYTHON_FOR_BUILD_CONFIG),)
$(error FEATURES requires python3 so scripts/build_config.py can validate feature names)
endif
endif

HEADERS := $(shell find src -name "*.h")
SOURCES := $(shell find src -name "*.cpp")
SWIG_FILES := $(shell find interfaces/swig -name "*.i")
MK_FILES := $(wildcard mk/*.mk)
BINDING_OPTIONS_SCRIPT := scripts/generate_binding_options.py
BUILD_CONFIG_SCRIPT := scripts/build_config.py
INFOMAP_VENDOR_CPPFLAGS := -Ivendor/nlohmann_json/include -Ivendor/fmt/include

# Resolve the whole build configuration in a single build_config.py invocation
# and `include` the emitted Make assignments (BUILD_CONFIG_* plus NATIVE_CXXFLAGS
# / NATIVE_LDFLAGS). build_config.py probes the compiler (`--version`) and shells
# out to brew, so doing this once per `make` run instead of once per field cuts
# the fixed cost paid by every build, including no-op incremental ones.
#
# Skip resolution when no requested goal consumes the build config — clean
# targets (which would otherwise recreate the build/ artifacts a clean removes)
# and `help`, the default goal. This keeps plain `make` / `make help` from
# probing the compiler and brew, so they work even without a toolchain.
BUILD_CONFIG_GOALS := $(if $(MAKECMDGOALS),$(MAKECMDGOALS),$(.DEFAULT_GOAL))
BUILD_CONFIG_NEEDED := $(filter-out help clean%,$(BUILD_CONFIG_GOALS))
BUILD_CONFIG_MK := build/build_config.generated.mk
ifneq ($(BUILD_CONFIG_NEEDED),)
ifneq ($(PYTHON_FOR_BUILD_CONFIG),)
# stderr is left attached so a failing resolution surfaces the real diagnostic
# (Python traceback / argparse error) rather than only the generic $(error) below.
BUILD_CONFIG_STATUS := $(shell mkdir -p $(dir $(BUILD_CONFIG_MK)) && CPPFLAGS='$(CPPFLAGS)' CXXFLAGS='$(CXXFLAGS)' LDFLAGS='$(LDFLAGS)' MACOSX_DEPLOYMENT_TARGET='$(MACOSX_DEPLOYMENT_TARGET)' $(PYTHON_FOR_BUILD_CONFIG) $(BUILD_CONFIG_SCRIPT) make-export --mode "$(MODE)" --openmp "$(OPENMP)" --native-arch "$(NATIVE_ARCH)" --features "$(FEATURES)" --compiler "$(CXX)" --platform "$(UNAME_S)" > $(BUILD_CONFIG_MK) && echo ok)
ifneq ($(BUILD_CONFIG_STATUS),ok)
$(error Failed to resolve build configuration via $(BUILD_CONFIG_SCRIPT))
endif
include $(BUILD_CONFIG_MK)
endif
endif
FEATURE_CACHE_KEY := $(if $(BUILD_CONFIG_ENABLED_FEATURES),$(subst $(space),_,$(BUILD_CONFIG_ENABLED_FEATURES)),none)
CXX_COMPILE := $(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),$(CCACHE_BIN) ,)$(CXX)

# LTO + GCC: plain `ar` strips the LTO bitcode from .o files, leaving an empty
# static archive. Prefer gcc-ar (which loads the LTO plugin) when both are in
# play. clang's ar handles LTO bitcode natively, so no override is needed there.
# A user-set AR (env, command line, or `override` directive) is respected;
# Make's built-in default (origin == "default") is overridden.
ifeq ($(NATIVE_ARCH),1)
ifeq ($(BUILD_CONFIG_COMPILER_FAMILY),gnu)
# $(origin AR) can be a two-word string ("command line", "environment override").
# Take the first word so the filter catches both single- and two-word origins.
ifeq ($(filter command environment override,$(firstword $(origin AR))),)
GCC_AR_BIN := $(shell command -v gcc-ar 2>/dev/null)
ifneq ($(GCC_AR_BIN),)
AR := $(GCC_AR_BIN)
endif
endif
endif
endif

.PHONY: help doctor dev-bootstrap clean build-binding-options

help:
	@printf "%s\n" \
		"Infomap Make targets" \
		"" \
		"Build" \
		"  build-native          Build the Infomap binary (MODE=release|debug, OPENMP=0|1)." \
		"  build-lib             Build the static C++ library and exported headers." \
		"  build-python          Build a Python wheel from the repo root using pyproject metadata." \
		"  build-python-swig     Regenerate the tracked SWIG wrapper outputs for Python maintainers." \
		"  build-r               Build the infomap R source tarball into dist/R/." \
		"  build-r-binary        Build a platform-native R binary into dist/R/." \
		"  build-r-binary-from-tarball Build a platform-native R binary from dist/R/." \
		"  build-r-swig          Regenerate the tracked SWIG wrapper outputs for R maintainers." \
		"  build-binding-options Regenerate Python, R, and TypeScript option APIs from C++ metadata." \
		"  build-js-metadata     Refresh the tracked JS parameter metadata." \
		"  build-js              Build the JS worker bundle and npm package assets from tracked metadata." \
		"  build-docs            Build the Python docs site into docs/ (untracked output)." \
		"" \
		"Test" \
		"  test-cpp-stream-policy  Verify runtime C++ does not use direct global std streams outside approved files." \
		"  test-native           Configure, build, and run the C++ doctest suite via CMake/CTest." \
		"  test-python           Run the full Python verification suite." \
		"  test-python-unit      Run pytest for test/python." \
		"  test-python-doctest   Run Python doctests and ruff checks for the installed package." \
		"  test-python-examples  Run the Python example smoke tests." \
		"  test-python-notebooks-smoke  Run PR-safe tutorial notebook smoke tests with nbmake." \
		"  test-python-notebooks-full   Run all CI-maintained tutorial notebooks with nbmake." \
		"  test-python-swig-freshness  Verify tracked SWIG outputs are up to date." \
		"  test-r                Run R CMD check --as-cran on the staged infomap R package." \
		"  test-r-examples       Run the R example smoke tests." \
		"  test-r-swig-freshness Verify tracked R SWIG outputs are up to date." \
		"  test-binding-options-freshness Verify tracked binding option APIs are current." \
		"  test-js-metadata      Regenerate JS metadata in a temp dir and verify tracked files are current." \
		"  test-js               Run JS lint/typecheck/unit/browser/package verification for the built npm package." \
		"  test-fast             Run the fast native + Python feedback suite." \
		"  test-sanitizers       Run the C++ test suite under ASan/UBSan." \
		"  bench-native          Run the native benchmark and memory baseline harness." \
		"  bench-python          Run the nightly-style Python benchmark harness." \
		"" \
		"Dev" \
		"  help                  Show this guide." \
		"  doctor                Inspect tool availability and active build flags." \
		"  configure-cpp-dev     Configure a clangd-friendly CMake dev build." \
		"  format-native         Rewrite C++ sources with clang-format." \
		"  format-native-check   Verify C++ sources are clang-format clean." \
		"  format-r              Rewrite R sources with Air." \
		"  format-r-check        Verify R sources are Air-format clean." \
		"  tidy-native           Run clang-tidy over C++ source files." \
		"  dev-cpp-check         Run the fast C++ developer feedback suite." \
		"  dev-bootstrap         Install Python dev dependencies and run npm ci." \
		"  dev-python-install    Install the built Python package in editable mode." \
		"  dev-python-notebooks-install Install notebook execution dependencies." \
		"  clean                 Remove native, Python, and JS build outputs." \
		"  clean-native          Remove native build artifacts, libraries, and CMake build dirs." \
		"  clean-python          Remove Python build outputs." \
		"  clean-r               Remove R build outputs." \
		"  clean-js              Remove JS build and pack outputs." \
		"" \
		"Docs / Release" \
		"  build-docs            Build the Python docs site into docs/ after installing the local package." \
		"  release-python-dist   Build local sdist and wheel artifacts from the repo root." \
		"  release-python-testpypi  Publish the built distributions to TestPyPI." \
		"  release-python-pypi      Publish the built distributions to PyPI." \
		"" \
		"CI / Advanced" \
		"  ci-export-github-env  Print GitHub Actions environment exports for macOS libomp." \
		"  Advanced/internal targets such as docker-* remain available but are secondary." \
		"" \
		"Examples" \
		"  make build-native" \
		"  make build-native JOBS=1" \
		"  make build-native MODE=debug OPENMP=0" \
		"  make build-native NATIVE_ARCH=1                 # -march=native + LTO + unroll (non-portable)" \
		"  make build-native NATIVE_ARCH=1 FEATURES=simd-log # plus inlined SIMD log2 (arm64 Neon / x86_64 AVX2+FMA)" \
		"  make build-python dev-python-install test-python-unit" \
		"  make build-binding-options test-binding-options-freshness" \
		"  make build-js-metadata test-js-metadata" \
		"  make build-js test-js" \
		"  make build-docs"

build-binding-options: build-native
	@$(PYTHON_FOR_BUILD_CONFIG) $(BINDING_OPTIONS_SCRIPT) --infomap-bin ./Infomap --output-root .

doctor:
	@printf "%s\n" "Infomap doctor" ""
	@printf "Platform: %s\n" "$(UNAME_S)"
	@printf "Mode: MODE=%s OPENMP=%s NATIVE_ARCH=%s FEATURES=%s\n" "$(MODE)" "$(OPENMP)" "$(NATIVE_ARCH)" "$(if $(BUILD_CONFIG_ENABLED_FEATURES),$(BUILD_CONFIG_ENABLED_FEATURES),none)"
	@printf "Policy: MODE drives Infomap optimization/debug flags for native, Python, and CMake targets.\n"
	@printf "Opt-ins: NATIVE_ARCH=1 enables -march=native+LTO+unroll; FEATURES=simd-log enables inlined SIMD log2 (arm64 Neon or x86_64 AVX2+FMA). Both produce non-portable binaries.\n"
	@printf "Mode detail: %s\n" "$(if $(filter debug,$(MODE)),$(if $(filter msvc,$(BUILD_CONFIG_COMPILER_FAMILY)),debug => /Od /Zi,debug => -O0 -g),$(if $(filter msvc,$(BUILD_CONFIG_COMPILER_FAMILY)),release => /O2,release => -O3))"
	@printf "CMake detail: CMAKE_BUILD_TYPE=%s is kept for generator/output behavior; MODE still owns Infomap compile flags.\n" "$(CMAKE_BUILD_TYPE)"
	@printf "Parallel jobs: %s\n" "$(JOBS)"
	@printf "ccache: %s\n" "$(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),$(CCACHE_BIN),disabled)"
	@printf "make: %s\n" "$$(command -v $(MAKE) 2>/dev/null || echo missing)"
	@printf "cxx (%s): %s\n" "$(CXX)" "$$(command -v $(CXX) 2>/dev/null || echo missing)"
	@printf "python (%s): %s\n" "$(PYTHON)" "$$(command -v $(PYTHON) 2>/dev/null || echo missing)"
	@printf "python build module: %s\n" "$$($(PYTHON) -c 'from importlib.metadata import version; print(version(\"build\"))' 2>/dev/null || echo 'missing (pip install build)')"
	@printf "swig (%s): %s\n" "$(SWIG)" "$$(command -v $(SWIG) 2>/dev/null || echo missing)"
	@printf "swig version: %s\n" "$$($(SWIG) -version 2>/dev/null | sed -nE 's/.*SWIG Version[[:space:]]+([0-9.]+).*/\1/p' | head -1 || echo unknown) (tracked wrappers require 4.4.1)"
	@printf "R (%s): %s\n" "$(R)" "$$(command -v $(R) 2>/dev/null || echo missing)"
	@printf "R version: %s\n" "$$($(R) --version 2>/dev/null | sed -nE 's/^R version ([0-9.]+).*/\1/p' | head -1 || echo missing)"
	@printf "Rscript (%s): %s\n" "$(RSCRIPT)" "$$(command -v $(RSCRIPT) 2>/dev/null || echo missing)"
	@printf "air (%s): %s\n" "$(AIR)" "$$(command -v $(AIR) 2>/dev/null || echo missing)"
	@printf "air version: %s\n" "$$($(AIR) --version 2>/dev/null || echo missing)"
	@printf "R packages:\n"
	@if command -v $(RSCRIPT) >/dev/null 2>&1; then \
		for pkg in R6 methods roxygen2 rcmdcheck testthat igraph tibble; do \
			ver=$$($(RSCRIPT) -e "cat(as.character(packageVersion('$$pkg')))" 2>/dev/null); \
			if [ -n "$$ver" ]; then \
				printf "  %-10s %s\n" "$$pkg" "$$ver"; \
			else \
				printf "  %-10s missing (Rscript -e 'install.packages(\"%s\")')\n" "$$pkg" "$$pkg"; \
			fi; \
		done; \
	else \
		printf "  Rscript not available; cannot probe R packages\n"; \
	fi
	@printf "node (%s): %s\n" "$(NODE)" "$$(command -v $(NODE) 2>/dev/null || echo missing)"
	@printf "npm (%s): %s\n" "$(NPM)" "$$(command -v $(NPM) 2>/dev/null || echo missing)"
	@printf "sphinx (%s): %s\n" "$(SPHINX_BUILD)" "$(if $(SPHINX_BUILD_BIN),$(SPHINX_BUILD_BIN),python -m sphinx)"
	@printf "cmake (%s): %s\n" "$(CMAKE)" "$$(command -v $(CMAKE) 2>/dev/null || echo missing)"
	@printf "ctest (%s): %s\n" "$(CTEST)" "$$(command -v $(CTEST) 2>/dev/null || echo missing)"
	@printf "clang-format (%s): %s\n" "$(CLANG_FORMAT)" "$$(command -v $(CLANG_FORMAT) 2>/dev/null || echo missing)"
	@printf "clang-tidy (%s): %s\n" "$(CLANG_TIDY)" "$$(command -v $(CLANG_TIDY) 2>/dev/null || echo missing)"
	@printf "clangd (%s): %s\n" "$(CLANGD)" "$$(command -v $(CLANGD) 2>/dev/null || echo missing)"
	@printf "ninja (%s): %s\n" "$(NINJA)" "$$(command -v $(NINJA) 2>/dev/null || echo missing)"
	@printf "compile_commands.json: %s\n" "$$([ -e compile_commands.json ] && printf present || printf missing)"
	@printf "em++ (%s): %s\n" "$(EMXX)" "$$(command -v $(EMXX) 2>/dev/null || echo missing)"
	@printf "NATIVE_CXXFLAGS=%s\n" "$(NATIVE_CXXFLAGS)"
	@printf "NATIVE_LDFLAGS=%s\n" "$(NATIVE_LDFLAGS)"
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		printf "brew prefix: %s\n" "$(if $(BUILD_CONFIG_BREW_PREFIX),$(BUILD_CONFIG_BREW_PREFIX),missing)"; \
		printf "libomp prefix: %s\n" "$(if $(BUILD_CONFIG_LIBOMP_PREFIX),$(BUILD_CONFIG_LIBOMP_PREFIX),missing)"; \
		printf "deployment target: %s\n" "$(if $(BUILD_CONFIG_DEPLOYMENT_TARGET),$(BUILD_CONFIG_DEPLOYMENT_TARGET),unset)"; \
		printf "Apple clang++ (for R workaround): %s\n" "$$([ -x /usr/bin/clang++ ] && /usr/bin/clang++ --version 2>/dev/null | head -1 || echo 'missing (xcode-select --install)')"; \
	fi

dev-bootstrap:
	@$(PIP) install -e '.[test,docs,examples,release]'
	@$(NPM) ci
	@printf "%s\n" \
		"Bootstrap complete." \
		"Next steps:" \
		"  make build-native" \
		"  make build-python dev-python-install" \
		"  make test-fast"

clean: clean-native clean-python clean-r clean-js
	@true
