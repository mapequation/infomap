PYTHON_SWIG_PY := interfaces/python/src/infomap/_swig.py
PYTHON_SWIG_CPP := interfaces/python/generated/infomap_wrap.cpp
SPHINX_SOURCE_DIR := interfaces/python/source
SPHINX_TARGET_DIR ?= docs
DOCS_BUILD_DIR ?= build/docs-site
DOCS_SYNC_ARGS := -a --delete
PYTHON_TEST_DIR := test/python
NOTEBOOK_DIR := examples/notebooks
NOTEBOOK_MANIFEST := $(NOTEBOOK_DIR)/notebooks.toml
NOTEBOOK_TIMEOUT ?= 300
# Lint/format the whole package source tree plus the examples. Ruff skips the
# SWIG-generated _swig.py via the extend-exclude in pyproject.toml.
PYTHON_LINT_TARGETS := \
	interfaces/python/src/infomap \
	examples/python
PYTHON_FORMAT_TARGETS := \
	interfaces/python/src/infomap \
	examples/python \
	test/python
PYTEST_ARGS ?=
PYTHON_DIST_DIR := dist/python
PYPI_DIR := $(PYTHON_DIST_DIR)
PYPI_SDIST := $(shell find $(PYPI_DIR) -name "*.tar.gz" 2>/dev/null)
# Route Python extension compiles through ccache too (the native build already
# does via CXX_COMPILE). distutils shlex-splits CC/CXX, so a "ccache c++" launcher
# prefix works. Skipped on Windows (MSVC `cl`), matching the native build.
ifeq ($(filter Windows_NT,$(OS)),)
PYTHON_BUILD_CXX ?= $(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),$(CCACHE_BIN) ,)$(CXX)
else
PYTHON_BUILD_CXX ?= cl
endif
PYTHON_BUILD_CC ?= $(PYTHON_BUILD_CXX)
PYTHON_BUILD_ENV = \
	CC="$(PYTHON_BUILD_CC)" CXX="$(PYTHON_BUILD_CXX)" MODE="$(MODE)" OPENMP="$(OPENMP)" \
	FEATURES="$(FEATURES)" INFOMAP_BUILD_JOBS="$(JOBS)" \
	CPPFLAGS="$(CPPFLAGS)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" \
	$(if $(MACOSX_DEPLOYMENT_TARGET),MACOSX_DEPLOYMENT_TARGET="$(MACOSX_DEPLOYMENT_TARGET)")

.PHONY: \
	build-python \
	build-python-swig \
	clean-python-build-cache \
	test-python-swig-freshness \
	dev-python-install \
	dev-python-notebooks-install \
	test-python \
	test-python-unit \
	test-python-doctest \
	test-python-typecheck \
	test-python-examples \
	test-python-notebooks-smoke \
	test-python-notebooks-full \
	build-docs \
	_build-docs-site \
	clean-python \
	format-python \
	release-python-dist \
	release-python-testpypi \
	release-python-pypi

build-python:
	@$(MAKE) --no-print-directory clean-python-build-cache
	@mkdir -p $(PYTHON_DIST_DIR)
	@$(PYTHON_BUILD_ENV) $(PYTHON) -m build --wheel --outdir $(PYTHON_DIST_DIR) .

clean-python-build-cache:
	@find build -maxdepth 1 -type d \( -name 'bdist.*' -o -name 'lib.*' \) -exec rm -rf {} + 2>/dev/null || true

build-python-swig:
	@SWIG="$(SWIG)" $(PYTHON) scripts/generate_python_swig.py --python-out $(PYTHON_SWIG_PY) --cpp-out $(PYTHON_SWIG_CPP)

test-python-swig-freshness:
	@SWIG="$(SWIG)" $(PYTHON) scripts/generate_python_swig.py --check --python-out $(PYTHON_SWIG_PY) --cpp-out $(PYTHON_SWIG_CPP)

# Extras installed by dev-python-install. Defaults to the full local dev set;
# CI test jobs override it (e.g. PYTHON_DEV_EXTRAS=test,examples) to skip docs.
PYTHON_DEV_EXTRAS ?= test,docs,examples

dev-python-install:
	$(PYTHON_BUILD_ENV) $(PIP) install -e ".[$(PYTHON_DEV_EXTRAS)]"

dev-python-notebooks-install:
	$(PYTHON_BUILD_ENV) $(PIP) install -e .[notebooks]

test-python: test-python-unit test-python-doctest test-python-examples
	@true

test-python-unit:
	@$(PYTEST) $(PYTEST_ARGS) $(PYTHON_TEST_DIR)

test-python-doctest:
	@$(RUFF) check $(PYTHON_LINT_TARGETS)
	@tmp_dir="$$(mktemp -d 2>/dev/null || mktemp -d -t infomap-doctest)"; \
	trap 'rm -rf "$$tmp_dir"' EXIT; \
	cp examples/networks/*.net "$$tmp_dir"/; \
	cd "$$tmp_dir" && $(PYTEST) --doctest-modules -q \
		"$(CURDIR)/interfaces/python/src/infomap/_facade.py" \
		"$(CURDIR)/interfaces/python/src/infomap/_results.py" \
		"$(CURDIR)/interfaces/python/src/infomap/result.py" \
		"$(CURDIR)/interfaces/python/src/infomap/network.py" \
		"$(CURDIR)/interfaces/python/src/infomap/run.py" \
		"$(CURDIR)/interfaces/python/src/infomap/io/writers.py"

test-python-examples:
	@cd examples/python && for f in *.py; do $(PYTHON) "$$f" > /dev/null || exit 1; done

# Static type check of the hand-written package. Config + scope (include the
# package, exclude the SWIG-generated _swig.py) live in [tool.pyright] in
# pyproject.toml. Not part of the default `test-python` aggregate -- run it
# explicitly or in a dedicated CI job.
test-python-typecheck:
	@$(PYRIGHT)

test-python-notebooks-smoke:
	@cd $(NOTEBOOK_DIR) && $(PYTHON) ../../scripts/notebook_manifest.py --manifest notebooks.toml --suite smoke --print0 | \
		xargs -0 $(PYTEST) --nbmake --nbmake-timeout=$(NOTEBOOK_TIMEOUT)

test-python-notebooks-full:
	@cd $(NOTEBOOK_DIR) && $(PYTHON) ../../scripts/notebook_manifest.py --manifest notebooks.toml --suite full --print0 | \
		xargs -0 $(PYTEST) --nbmake --nbmake-timeout=$(NOTEBOOK_TIMEOUT)

define BUILD_DOCS_SITE
set -e; \
mkdir -p "$(1)"; \
$(SPHINX_BUILD) -b html "$(SPHINX_SOURCE_DIR)" "$(1)"; \
searchindex="$(1)/searchindex.js"; \
if [ -f "$$searchindex" ] && [ -n "$$(tail -c 1 "$$searchindex" 2>/dev/null)" ]; then \
	printf '\n' >> "$$searchindex"; \
fi; \
rm -rf "$(1)/.doctrees"
endef

_build-docs-site:
	@$(call BUILD_DOCS_SITE,$(SPHINX_TARGET_DIR))

build-docs: build-native dev-python-install
	@rm -rf "$(DOCS_BUILD_DIR)"
	@$(call BUILD_DOCS_SITE,$(DOCS_BUILD_DIR))
	mkdir -p docs; \
	rsync $(DOCS_SYNC_ARGS) "$(DOCS_BUILD_DIR)/" docs/

clean-python:
	$(RM) -r dist/python *.egg-info interfaces/python/src/infomap/_infomap*.so interfaces/python/src/infomap/*.pyd
	@find build -maxdepth 1 -type d \( -name 'bdist.*' -o -name 'lib.*' -o -name 'temp.*' \) -exec rm -rf {} + 2>/dev/null || true

format-python:
	$(RUFF) format $(PYTHON_FORMAT_TARGETS)

release-python-dist:
	@$(MAKE) --no-print-directory clean-python-build-cache
	@mkdir -p $(PYTHON_DIST_DIR)
	@$(PYTHON_BUILD_ENV) $(PYTHON) -m build --sdist --wheel --outdir $(PYTHON_DIST_DIR) .

release-python-testpypi:
	@[ "$(PYPI_SDIST)" ] && echo "Publish dist..." || ( echo "dist files not built"; exit 1 )
	$(PYTHON) -m twine upload -r testpypi --verbose $(PYPI_DIR)/*

release-python-pypi:
	@[ "$(PYPI_SDIST)" ] && echo "Publish dist..." || ( echo "dist files not built"; exit 1 )
	$(PYTHON) -m twine upload --skip-existing --verbose $(PYPI_DIR)/*
