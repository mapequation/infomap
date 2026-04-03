PY_BUILD_DIR := build/py
PY_ONLY_HEADERS := $(HEADERS:%.h=$(PY_BUILD_DIR)/headers/%.h)
PY_HEADERS := $(HEADERS:src/%.h=$(PY_BUILD_DIR)/src/%.h)
PY_SOURCES := $(SOURCES:src/%.cpp=$(PY_BUILD_DIR)/src/%.cpp)

SPHINX_SOURCE_DIR := interfaces/python/source
SPHINX_TARGET_DIR ?= docs
DOCS_FRESHNESS_EXCLUDES := automation maintainers plans .nojekyll .buildinfo
DOCS_FRESHNESS_DIFF_ARGS := $(foreach excluded,$(DOCS_FRESHNESS_EXCLUDES),--exclude=$(excluded))
DOCS_SYNC_ARGS := -a --delete $(foreach excluded,$(DOCS_FRESHNESS_EXCLUDES),--exclude=/$(excluded))
PYTHON_TEST_DIR := test/python
PYTEST_ARGS ?=
PYPI_DIR := $(PY_BUILD_DIR)
PYPI_SDIST := $(shell find $(PYPI_DIR) -name "*.tar.gz" 2>/dev/null)
PYTHON_BUILD_ENV = \
	CC="$(CXX)" CXX="$(CXX)" MODE="$(MODE)" OPENMP="$(OPENMP)" \
	CPPFLAGS="$(CPPFLAGS)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" \
	$(if $(MACOSX_DEPLOYMENT_TARGET),MACOSX_DEPLOYMENT_TARGET="$(MACOSX_DEPLOYMENT_TARGET)")

.PHONY: \
	build-python \
	build-python-swig \
	build-python-package-files \
	_build-python-package-layout \
	dev-python-install \
	test-python \
	test-python-unit \
	test-python-doctest \
	test-python-examples \
	build-docs \
	test-docs \
	_build-docs-site \
	clean-python \
	format-python \
	release-python-dist \
	release-python-testpypi \
	release-python-pypi \
	py-prepare

build-python: build-python-swig _build-python-package-layout
	@cd $(PY_BUILD_DIR) && $(PYTHON_BUILD_ENV) $(PYTHON) setup.py build_ext --inplace

build-python-package-files: _build-python-package-layout
	@true

build-python-swig: $(PY_HEADERS) $(PY_SOURCES) $(PY_ONLY_HEADERS) interfaces/python/infomap.py
	@mkdir -p $(PY_BUILD_DIR)
	@cp -a $(SWIG_FILES) $(PY_BUILD_DIR)/
	@$(SWIG) -version | grep "SWIG Version"
	$(SWIG) -c++ -python -outdir $(PY_BUILD_DIR) -o $(PY_BUILD_DIR)/infomap_wrap.cpp $(PY_BUILD_DIR)/Infomap.i

_build-python-package-layout: build-python-swig $(MK_FILES) Makefile
	@cp -a interfaces/python/setup.py $(PY_BUILD_DIR)/
	@cp -a scripts/build_config.py $(PY_BUILD_DIR)/
	@cp -a interfaces/python/pyproject.toml $(PY_BUILD_DIR)/
	@touch $(PY_BUILD_DIR)/__init__.py
	@$(PYTHON) utils/create-python-package-meta.py $(PY_BUILD_DIR)/package_meta.py
	@cat $(PY_BUILD_DIR)/package_meta.py $(PY_BUILD_DIR)/infomap.py > $(PY_BUILD_DIR)/temp.py
	@mv $(PY_BUILD_DIR)/temp.py $(PY_BUILD_DIR)/infomap.py
	@$(PYTHON) scripts/ensure_build_deps.py || true
	@$(RUFF) format $(PY_BUILD_DIR)/infomap.py || true
	@cp -a interfaces/python/MANIFEST.in $(PY_BUILD_DIR)/
	@cp -a README.rst $(PY_BUILD_DIR)/
	@cp -a LICENSE_GPLv3.txt $(PY_BUILD_DIR)/LICENSE

$(PY_BUILD_DIR)/src/%: src/%
	@mkdir -p $(dir $@)
	@cp -a $^ $@

$(PY_BUILD_DIR)/headers/%: %
	@mkdir -p $(dir $@)
	@cp -a $^ $@

dev-python-install: build-python
	$(PYTHON_BUILD_ENV) $(PIP) install --no-build-isolation -e $(PY_BUILD_DIR)

test-python: test-python-unit test-python-doctest test-python-examples
	@true

test-python-unit:
	@$(PYTEST) $(PYTEST_ARGS) $(PYTHON_TEST_DIR)

test-python-doctest:
	@cp -r examples/networks/*.net $(PY_BUILD_DIR) || true
	@cd $(PY_BUILD_DIR) && $(RUFF) check infomap.py
	@cd $(PY_BUILD_DIR) && $(PYTHON) -m doctest infomap.py

test-python-examples:
	@cd examples/python && for f in *.py; do $(PYTHON) "$$f" > /dev/null || exit 1; done

_build-docs-site:
	@mkdir -p "$(SPHINX_TARGET_DIR)"
	@cp -a README.rst "$(SPHINX_SOURCE_DIR)/index.rst"
	@trap 'rm -f "$(SPHINX_SOURCE_DIR)/index.rst"' EXIT; \
		$(SPHINX_BUILD) -b html "$(SPHINX_SOURCE_DIR)" "$(SPHINX_TARGET_DIR)"
	@rm -rf "$(SPHINX_TARGET_DIR)/.doctrees"

build-docs: dev-python-install
	@tmp_dir="$$(mktemp -d 2>/dev/null || mktemp -d -t infomap-docs)"; \
	trap 'rm -rf "$$tmp_dir"' EXIT; \
	$(MAKE) --no-print-directory SPHINX_TARGET_DIR="$$tmp_dir/docs" _build-docs-site; \
	mkdir -p docs; \
	rsync $(DOCS_SYNC_ARGS) "$$tmp_dir/docs/" docs/

test-docs: dev-python-install
	@tmp_dir="$$(mktemp -d 2>/dev/null || mktemp -d -t infomap-docs)"; \
	trap 'rm -rf "$$tmp_dir"' EXIT; \
	$(MAKE) --no-print-directory SPHINX_TARGET_DIR="$$tmp_dir/docs" _build-docs-site; \
	diff -ru $(DOCS_FRESHNESS_DIFF_ARGS) "$$tmp_dir/docs" docs

clean-python:
	$(RM) -r $(PY_BUILD_DIR)

format-python:
	$(RUFF) format interfaces/python examples/python || true

py-prepare:
	$(PIP) install -r requirements_dev.txt

release-python-dist: build-python
	cd $(PY_BUILD_DIR) && $(PYTHON_BUILD_ENV) $(PYTHON) setup.py sdist bdist_wheel

release-python-testpypi:
	@[ "$(PYPI_SDIST)" ] && echo "Publish dist..." || ( echo "dist files not built"; exit 1 )
	cd $(PYPI_DIR) && $(PYTHON) -m twine upload -r testpypi --verbose dist/*

release-python-pypi:
	@[ "$(PYPI_SDIST)" ] && echo "Publish dist..." || ( echo "dist files not built"; exit 1 )
	cd $(PYPI_DIR) && $(PYTHON) -m twine upload --skip-existing --verbose dist/*
