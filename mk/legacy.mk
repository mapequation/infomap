.PHONY: \
	all \
	noomp \
	debug \
	python \
	py-swig \
	py-build \
	py-package-files \
	py-test \
	py-test-unit \
	py-test-doctest \
	py-test-examples \
	py-local-install \
	py-doc \
	py-clean \
	py-format \
	js-worker \
	js-test \
	js-clean \
	js-format \
	lib \
	cpp-test \
	perf-bench \
	ci-github-env \
	pypi-dist \
	pypitest-publish \
	pypi-publish \
	format

all: build-native

noomp:
	@$(MAKE) build-native OPENMP=0

debug:
	@$(MAKE) build-native MODE=debug

python: build-python
py-swig: build-python-swig
py-build: build-python
py-package-files: build-python-package-files
py-test: test-python
py-test-unit: test-python-unit
py-test-doctest: test-python-doctest
py-test-examples: test-python-examples
py-local-install: dev-python-install
py-doc: build-docs
py-clean: clean-python
py-format: format-python

js-worker: build-js
js-test: test-js
js-clean: clean-js
js-format: format-js

lib: build-lib
cpp-test: test-native
perf-bench: bench-python
ci-github-env: ci-export-github-env

pypi-dist: release-python-dist
pypitest-publish: release-python-testpypi
pypi-publish: release-python-pypi

format: format-native format-python format-js
