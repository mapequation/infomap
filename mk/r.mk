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
R_TEST_DIR := test/R

# On macOS, Homebrew LLVM clang++ may be first in PATH but Homebrew R uses
# Apple's libc++ at runtime.  Compilation with LLVM clang++ produces a .so
# that references LLVM-only symbols (e.g. __ZNSt3__113__hash_memoryEPKvm)
# that the Apple runtime cannot resolve.  Writing a temporary Makevars file
# that pins CC/CXX to Apple's toolchain — and passing it via
# R_MAKEVARS_USER — keeps the compiled .so ABI-compatible with R.
ifeq ($(UNAME_S),Darwin)
_R_APPLE_MV  := $(CURDIR)/$(R_BUILD_DIR)/.Makevars.apple
R_CMD_ENV    := R_MAKEVARS_USER=$(_R_APPLE_MV)
_write_apple_mv = @mkdir -p $(R_BUILD_DIR) && \
	printf 'CC = /usr/bin/clang\nCXX = /usr/bin/clang++\nCXX17 = /usr/bin/clang++ -std=c++17\n' \
	> $(_R_APPLE_MV)
else
R_CMD_ENV       :=
_write_apple_mv := @true
endif

.PHONY: \
	build-r-swig \
	test-r-swig-freshness \
	build-r-stage \
	build-r \
	build-r-binary \
	test-r \
	test-r-examples \
	dev-r-install \
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
	@$(R) CMD build --no-manual --no-build-vignettes $(R_STAGED_DIR)
	@mv infomap_$(R_VERSION).tar.gz $(R_DIST_DIR)/
	@echo "Built $(R_TARBALL)"

build-r-binary: build-r
	$(_write_apple_mv)
	@mkdir -p $(R_DIST_DIR)
	cd $(R_DIST_DIR) && $(R_CMD_ENV) $(R) CMD INSTALL --build $(CURDIR)/$(R_TARBALL)
	@echo "Built R binary in $(R_DIST_DIR)/"

test-r: build-r-stage
	$(_write_apple_mv)
	@mkdir -p $(R_BUILD_DIR)/check
	$(R_CMD_ENV) $(R) CMD check --as-cran --no-manual --no-vignettes \
		--output=$(R_BUILD_DIR)/check $(R_STAGED_DIR)

dev-r-install: build-r-stage
	$(_write_apple_mv)
	$(R_CMD_ENV) $(R) CMD INSTALL $(R_STAGED_DIR)

test-r-examples: dev-r-install
	@for f in examples/R/*.R; do \
		case "$$f" in *load-infomap.R) continue ;; esac; \
		echo "Running $$f"; \
		$(RSCRIPT) "$$f" > /dev/null || exit 1; \
	done

clean-r:
	$(RM) -r $(R_BUILD_DIR) $(R_DIST_DIR)

release-r-dist: clean-r build-r build-r-binary
	@true
