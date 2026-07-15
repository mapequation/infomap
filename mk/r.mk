R_SKELETON_DIR := interfaces/R/infomap
R_GENERATED_DIR := interfaces/R/generated
R_GENERATED_R := $(R_GENERATED_DIR)/infomap.R
R_GENERATED_CPP := $(R_GENERATED_DIR)/infomap_wrap.cpp
R_BUILD_DIR := build/R
R_STAGED_DIR := $(R_BUILD_DIR)/infomap
R_DIST_DIR := dist/R
R_VERSION := $(shell awk '/^Version:/ {print $$2}' $(R_SKELETON_DIR)/DESCRIPTION 2>/dev/null)
R_TARBALL := $(R_DIST_DIR)/infomap_$(R_VERSION).tar.gz

R_STAGE_SCRIPT := scripts/stage_r_package.py
R_SWIG_SCRIPT := scripts/generate_r_swig.py
R_SOURCE_FILES := $(filter-out $(R_SKELETON_DIR)/R/options.R,$(wildcard $(R_SKELETON_DIR)/R/*.R))
R_TEST_FILES := $(wildcard $(R_SKELETON_DIR)/tests/*.R) $(wildcard $(R_SKELETON_DIR)/tests/testthat/*.R)
R_EXAMPLE_FILES := $(wildcard examples/R/*.R)
R_FORMAT_TARGETS := \
	$(R_SOURCE_FILES) \
	$(R_TEST_FILES) \
	$(R_EXAMPLE_FILES)

# R compiles the package's ~25 translation units serially through the toolchain
# from its Makeconf (no ccache). We speed both up:
#   * MAKE="make -j$(JOBS)": R drives the SHLIB build via $(MAKE), so overriding
#     it parallelizes the per-object compiles. (MAKEFLAGS=-j is NOT honored here.)
#   * ccache as a compiler launcher, so repeat builds hit the cache — same idea
#     as the native/Python builds. ccache execs the compiler unchanged for link
#     steps, so it is safe as the CXX used to both compile and link the .so.
#   * CCACHE_NOHASHDIR=1 (when ccache is active): R CMD INSTALL --build, the
#     r-universe build, and CI binary builds compile inside a fresh temp dir each
#     run, so ccache's default directory hashing (on with -g) would miss every
#     time. Dropping it lets tarball installs hit the cache too, and lets the
#     stable staged-dir install (dev-r-install) share entries with them.
CCACHE_LAUNCHER := $(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),$(CCACHE_BIN),)
R_PARALLEL := MAKE="$(MAKE) -j$(JOBS)"
# Only meaningful when ccache is active; empty otherwise so the no-ccache
# environment is left byte-for-byte unchanged. Trailing space lets it abut the
# next assignment cleanly.
R_CCACHE_ENV := $(if $(CCACHE_LAUNCHER),CCACHE_NOHASHDIR=1 ,)
R_MAKEVARS_FILE := $(CURDIR)/$(R_BUILD_DIR)/.Makevars.infomap

# On macOS, Homebrew LLVM clang++ may be first in PATH but Homebrew R uses
# Apple's libc++ at runtime.  Compilation with LLVM clang++ produces a .so
# that references LLVM-only symbols (e.g. __ZNSt3__113__hash_memoryEPKvm)
# that the Apple runtime cannot resolve.  The package's configure script pins
# CC/CXX to Apple's toolchain with GNU Make `override` (which beats both Makeconf
# and a user Makevars), keeping the compiled .so ABI-compatible with R. We pass
# the ccache launcher through INFOMAP_R_CXX_LAUNCHER so configure can prefix the
# pinned compiler; we still write the user Makevars as a historical fallback.
# REMOVE WHEN: Homebrew R links against the LLVM libc++ runtime, or the
# CRAN macOS toolchain assumption changes such that LLVM-built .so files
# load cleanly under R.
ifeq ($(UNAME_S),Darwin)
_R_CC  := $(if $(CCACHE_LAUNCHER),$(CCACHE_LAUNCHER) ,)/usr/bin/clang
_R_CXX := $(if $(CCACHE_LAUNCHER),$(CCACHE_LAUNCHER) ,)/usr/bin/clang++
R_CMD_ENV := R_MAKEVARS_USER=$(R_MAKEVARS_FILE) $(R_PARALLEL) $(R_CCACHE_ENV)INFOMAP_R_CXX_LAUNCHER=$(CCACHE_LAUNCHER)
_write_r_makevars = @mkdir -p $(R_BUILD_DIR) && \
	printf 'CC = %s\nCXX = %s\nCXX17 = %s\n' '$(_R_CC)' '$(_R_CXX)' '$(_R_CXX)' \
	> $(R_MAKEVARS_FILE)
else ifneq ($(CCACHE_LAUNCHER),)
# Other unix: no configure override, so route R's configured compiler through
# ccache via a user Makevars.
R_CMD_ENV := R_MAKEVARS_USER=$(R_MAKEVARS_FILE) $(R_PARALLEL) $(R_CCACHE_ENV)
_write_r_makevars = @mkdir -p $(R_BUILD_DIR) && \
	cc="$$($(R) CMD config CC)"; cxx="$$($(R) CMD config CXX)"; cxx17="$$($(R) CMD config CXX17)"; \
	printf 'CC = %s %s\nCXX = %s %s\nCXX17 = %s %s\n' \
	'$(CCACHE_LAUNCHER)' "$$cc" '$(CCACHE_LAUNCHER)' "$$cxx" '$(CCACHE_LAUNCHER)' "$$cxx17" \
	> $(R_MAKEVARS_FILE)
else
R_CMD_ENV := $(R_PARALLEL)
_write_r_makevars := @true
endif

.PHONY: \
	build-r-swig \
	test-r-swig-freshness \
	build-r-stage \
	build-r \
	build-r-binary \
	build-r-binary-from-tarball \
	test-r \
	test-r-examples \
	dev-r-install \
	format-r \
	format-r-check \
	clean-r \
	release-r-dist

build-r-swig:
	@SWIG="$(SWIG)" $(PYTHON) $(R_SWIG_SCRIPT) --r-out $(R_GENERATED_R) --cpp-out $(R_GENERATED_CPP)

test-r-swig-freshness:
	@SWIG="$(SWIG)" $(PYTHON) $(R_SWIG_SCRIPT) --check --r-out $(R_GENERATED_R) --cpp-out $(R_GENERATED_CPP)

build-r-stage:
	@$(PYTHON) $(R_STAGE_SCRIPT) --out-dir $(R_STAGED_DIR)

build-r: build-r-stage
	@mkdir -p $(R_DIST_DIR)
	@$(R) CMD build --no-manual $(R_STAGED_DIR)
	@mv infomap_$(R_VERSION).tar.gz $(R_DIST_DIR)/
	@echo "Built $(R_TARBALL)"

build-r-binary: build-r
	$(MAKE) build-r-binary-from-tarball

build-r-binary-from-tarball:
	$(_write_r_makevars)
	@mkdir -p $(R_DIST_DIR)
	@test -f $(R_TARBALL)
	cd $(R_DIST_DIR) && $(R_CMD_ENV) $(R) CMD INSTALL --build $(CURDIR)/$(R_TARBALL)
	@echo "Built R binary in $(R_DIST_DIR)/"

test-r: build-r-stage
	$(_write_r_makevars)
	@mkdir -p $(R_BUILD_DIR)/check
	$(R_CMD_ENV) _R_CHECK_FORCE_SUGGESTS_=false \
		$(RSCRIPT) scripts/r_check.R $(R_STAGED_DIR) $(R_BUILD_DIR)/check \
		--no-manual --no-vignettes --as-cran

dev-r-install: build-r-stage
	$(_write_r_makevars)
	$(R_CMD_ENV) $(R) CMD INSTALL $(R_STAGED_DIR)

test-r-examples: dev-r-install
	@for f in examples/R/*.R; do \
		case "$$f" in *load-infomap.R) continue ;; esac; \
		echo "Running $$f"; \
		$(RSCRIPT) "$$f" > /dev/null || exit 1; \
	done

format-r:
	$(AIR) format $(R_FORMAT_TARGETS)

format-r-check:
	$(AIR) format $(R_FORMAT_TARGETS) --check

clean-r:
	$(RM) -r $(R_BUILD_DIR) $(R_DIST_DIR)

release-r-dist: clean-r build-r build-r-binary
	@true
