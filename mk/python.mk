PYTHON_SWIG_PY := interfaces/python/src/infomap/_swig.py
PYTHON_SWIG_CPP := interfaces/python/generated/infomap_wrap.cpp
SPHINX_SOURCE_DIR := interfaces/python/source
SPHINX_TARGET_DIR ?= docs
DOCS_FRESHNESS_EXCLUDES := maintainers .nojekyll .buildinfo
DOCS_FRESHNESS_DIFF_ARGS := $(foreach excluded,$(DOCS_FRESHNESS_EXCLUDES),--exclude=$(excluded))
DOCS_SYNC_ARGS := -a --delete $(foreach excluded,$(DOCS_FRESHNESS_EXCLUDES),--exclude=/$(excluded))
PYTHON_TEST_DIR := test/python
PYTHON_LINT_TARGETS := \
	interfaces/python/src/infomap/__init__.py \
	interfaces/python/src/infomap/__main__.py \
	interfaces/python/src/infomap/_bindings.py \
	interfaces/python/src/infomap/_facade.py \
	interfaces/python/src/infomap/_networkx.py \
	interfaces/python/src/infomap/_optional.py \
	interfaces/python/src/infomap/_options.py \
	interfaces/python/src/infomap/_results.py \
	interfaces/python/src/infomap/_version.py \
	interfaces/python/src/infomap/_writers.py \
	examples/python
PYTHON_FORMAT_TARGETS := \
	interfaces/python/src/infomap/__init__.py \
	interfaces/python/src/infomap/__main__.py \
	interfaces/python/src/infomap/_bindings.py \
	interfaces/python/src/infomap/_facade.py \
	interfaces/python/src/infomap/_networkx.py \
	interfaces/python/src/infomap/_optional.py \
	interfaces/python/src/infomap/_options.py \
	interfaces/python/src/infomap/_results.py \
	interfaces/python/src/infomap/_version.py \
	interfaces/python/src/infomap/_writers.py \
	examples/python \
	test/python
PYTEST_ARGS ?=
PYTHON_DIST_DIR := dist/python
PYPI_DIR := $(PYTHON_DIST_DIR)
PYPI_SDIST := $(shell find $(PYPI_DIR) -name "*.tar.gz" 2>/dev/null)
PYTHON_BUILD_CXX ?= $(if $(and $(filter Windows_NT,$(OS)),$(filter c++,$(CXX))),cl,$(CXX))
PYTHON_BUILD_CC ?= $(PYTHON_BUILD_CXX)
PYTHON_BUILD_ENV = \
	CC="$(PYTHON_BUILD_CC)" CXX="$(PYTHON_BUILD_CXX)" MODE="$(MODE)" OPENMP="$(OPENMP)" \
	CPPFLAGS="$(CPPFLAGS)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" \
	$(if $(MACOSX_DEPLOYMENT_TARGET),MACOSX_DEPLOYMENT_TARGET="$(MACOSX_DEPLOYMENT_TARGET)")

.PHONY: \
	build-python \
	build-python-swig \
	build-python-package-files \
	clean-python-build-cache \
	test-python-swig-freshness \
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

build-python:
	@$(MAKE) --no-print-directory clean-python-build-cache
	@mkdir -p $(PYTHON_DIST_DIR)
	@$(PYTHON_BUILD_ENV) $(PYTHON) -m build --wheel --outdir $(PYTHON_DIST_DIR) .

clean-python-build-cache:
	@find build -maxdepth 1 -type d \( -name 'bdist.*' -o -name 'lib.*' \) -exec rm -rf {} + 2>/dev/null || true

build-python-package-files: build-python-swig
	@true

build-python-swig:
	@SWIG="$(SWIG)" $(PYTHON) scripts/generate_python_swig.py --python-out $(PYTHON_SWIG_PY) --cpp-out $(PYTHON_SWIG_CPP)

test-python-swig-freshness:
	@SWIG="$(SWIG)" $(PYTHON) scripts/generate_python_swig.py --check --python-out $(PYTHON_SWIG_PY) --cpp-out $(PYTHON_SWIG_CPP)

dev-python-install:
	$(PYTHON_BUILD_ENV) $(PIP) install -e .[test,docs,examples]

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
		"$(CURDIR)/interfaces/python/src/infomap/_writers.py"

test-python-examples:
	@cd examples/python && for f in *.py; do $(PYTHON) "$$f" > /dev/null || exit 1; done

_build-docs-site:
	@mkdir -p "$(SPHINX_TARGET_DIR)"
	@cp -a README.rst "$(SPHINX_SOURCE_DIR)/index.rst"
	@trap 'rm -f "$(SPHINX_SOURCE_DIR)/index.rst"' EXIT; \
		$(SPHINX_BUILD) -b html "$(SPHINX_SOURCE_DIR)" "$(SPHINX_TARGET_DIR)"
	@searchindex="$(SPHINX_TARGET_DIR)/searchindex.js"; \
		if [ -f "$$searchindex" ] && [ -n "$$(tail -c 1 "$$searchindex" 2>/dev/null)" ]; then \
			printf '\n' >> "$$searchindex"; \
		fi
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
	$(RM) -r dist/python *.egg-info interfaces/python/src/infomap/_infomap*.so interfaces/python/src/infomap/*.pyd
	@find build -maxdepth 1 -type d \( -name 'bdist.*' -o -name 'lib.*' -o -name 'temp.*' \) -exec rm -rf {} + 2>/dev/null || true

format-python:
	$(RUFF) format $(PYTHON_FORMAT_TARGETS) || true

py-prepare:
	$(PIP) install -e '.[test,docs,examples,release]'

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
