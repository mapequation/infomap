PY_BUILD_DIR := build/py
PY_ONLY_HEADERS := $(HEADERS:%.h=$(PY_BUILD_DIR)/headers/%.h)
PY_HEADERS := $(HEADERS:src/%.h=$(PY_BUILD_DIR)/src/%.h)
PY_SOURCES := $(SOURCES:src/%.cpp=$(PY_BUILD_DIR)/src/%.cpp)

SPHINX_SOURCE_DIR := interfaces/python/source
SPHINX_TARGET_DIR := docs
PYTHON_TEST_DIR := test/python
PYTEST_ARGS ?=
PYPI_DIR := $(PY_BUILD_DIR)
PYPI_SDIST := $(shell find $(PYPI_DIR) -name "*.tar.gz" 2>/dev/null)

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
	clean-python \
	format-python \
	release-python-dist \
	release-python-testpypi \
	release-python-pypi \
	py-prepare

build-python: build-python-swig _build-python-package-layout
	@cd $(PY_BUILD_DIR) && CC="$(CXX)" CXXFLAGS="$(NATIVE_CXXFLAGS)" LDFLAGS="$(NATIVE_LDFLAGS)" $(PYTHON) setup.py build_ext --inplace

build-python-package-files: _build-python-package-layout
	@true

build-python-swig: $(PY_HEADERS) $(PY_SOURCES) $(PY_ONLY_HEADERS) interfaces/python/infomap.py
	@mkdir -p $(PY_BUILD_DIR)
	@cp -a $(SWIG_FILES) $(PY_BUILD_DIR)/
	@$(SWIG) -version | grep "SWIG Version"
	$(SWIG) -c++ -python -outdir $(PY_BUILD_DIR) -o $(PY_BUILD_DIR)/infomap_wrap.cpp $(PY_BUILD_DIR)/Infomap.i

_build-python-package-layout: build-python-swig $(MK_FILES) Makefile
	@cp -a interfaces/python/setup.py $(PY_BUILD_DIR)/
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
	$(PIP) install --no-build-isolation -e $(PY_BUILD_DIR)

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

build-docs: dev-python-install
	@mkdir -p $(SPHINX_TARGET_DIR)
	@cp -a README.rst $(SPHINX_SOURCE_DIR)/index.rst
	$(SPHINX_BUILD) -b html $(SPHINX_SOURCE_DIR) $(SPHINX_TARGET_DIR)
	@rm -r $(SPHINX_SOURCE_DIR)/index.rst
	npx prettier --write docs/searchindex.js

clean-python:
	$(RM) -r $(PY_BUILD_DIR)

format-python:
	$(RUFF) format interfaces/python examples/python || true

py-prepare:
	$(PIP) install -r requirements_dev.txt

release-python-dist: build-python
	cd $(PY_BUILD_DIR) && $(PYTHON) setup.py sdist bdist_wheel

release-python-testpypi:
	@[ "$(PYPI_SDIST)" ] && echo "Publish dist..." || ( echo "dist files not built"; exit 1 )
	cd $(PYPI_DIR) && $(PYTHON) -m twine upload -r testpypi --verbose dist/*

release-python-pypi:
	@[ "$(PYPI_SDIST)" ] && echo "Publish dist..." || ( echo "dist files not built"; exit 1 )
	cd $(PYPI_DIR) && $(PYTHON) -m twine upload --skip-existing --verbose dist/*
