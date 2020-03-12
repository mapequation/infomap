CXXFLAGS = -Wall -Wextra -Wno-unused-parameter -std=c++14
# http://www.network-theory.co.uk/docs/gccintro/gccintro_70.html -msse2 -mfpmath=sse -DDOUBLE
# CXXFLAGS = -Wall -std=c++14 -DPYTHON -Wno-deprecated-register
LDFLAGS =
CXX_CLANG := $(shell $(CXX) --version 2>/dev/null | grep clang)
ifeq "$(findstring debug, $(MAKECMDGOALS))" "debug"
	CXXFLAGS += -O0 -g
else
	ifeq "$(CXX_CLANG)" ""
		CXXFLAGS += -O4
		ifneq "$(findstring noomp, $(MAKECMDGOALS))" "noomp"
			CXXFLAGS += -fopenmp
			LDFLAGS += -fopenmp
		endif
	else
		CXXFLAGS += -O3
	endif
endif

##################################################
# General file dependencies
##################################################

HEADERS := $(shell find src -name "*.h")
SOURCES := $(shell find src -name "*.cpp")
OBJECTS := $(SOURCES:src/%.cpp=build/Infomap/%.o)

##################################################
# Stand-alone C++ targets
##################################################

.PHONY: all noomp test debug format

all: Infomap
	@true

Infomap: $(OBJECTS)
	@echo "Linking object files to target $@..."
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "-- Link finished --"

## Generic compilation rule for object files from cpp files
build/Infomap/%.o : src/%.cpp $(HEADERS) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

noomp: Infomap
	@true

debug: clean Infomap
	@true

format:
	clang-format -i $(HEADERS) $(SOURCES)


##################################################
# JavaScript through Emscripten
##################################################

.PHONY: js js-worker js-clean js-test

WORKER_FILENAME := infomap.worker.js
PRE_WORKER_MODULE := interfaces/js/pre-worker-module.js

js: build/js/Infomap.js
	@echo "Built $^"

js-worker: build/js/$(WORKER_FILENAME) Infomap
	@echo "Built $^"
	@mkdir -p interfaces/js/src/worker
	cp build/js/* interfaces/js/src/worker/
	npm run build
	cp interfaces/js/README.md .

js-test:
	$(RM) -r package mapequation-infomap-*.tgz
	npm pack
	tar -xzvf mapequation-infomap-*.tgz
	cp package/dist/index.js examples/js
	sed -i.'backup' -e 's/src=".*"/src="index.js"/' \
		examples/js/infomap-worker.html
	open examples/js/infomap-worker.html
	sleep 5
	$(RM) -r package
	$(RM) -r mapequation-infomap-*.tgz
	$(RM) -r examples/js/index.js
	$(RM) -r examples/js/infomap-worker.html
	mv examples/js/infomap-worker.html{.backup,}

build/js/infomap.worker.js: $(SOURCES) $(PRE_WORKER_MODULE)
	@echo "Compiling Infomap to run in a worker in the browser..."
	@mkdir -p $(dir $@)
	em++ -std=c++14 -O3 -s WASM=0 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=0 -s ENVIRONMENT=worker --pre-js $(PRE_WORKER_MODULE) -o build/js/$(WORKER_FILENAME) $(SOURCES)

build/js/Infomap.js: $(SOURCES)
	@echo "Compiling Infomap for Node.js..."
	@mkdir -p $(dir $@)
	em++ -O0 -o build/js/Infomap.js $^

js-clean:
	$(RM) -r build/js interfaces/js/src/worker/* dist/* README.md


##################################################
# Static C++ library
##################################################

# Use separate object files to compile with definitions
# NS_INFOMAP: Wrap code in namespace infomap
# AS_LIB: Skip main function
LIB_DIR = build/lib
LIB_HEADERS := $(HEADERS:src/%.h=include/%.h)
LIB_OBJECTS := $(SOURCES:src/%.cpp=build/lib/%.o)

.PHONY: lib

lib: lib/libInfomap.a $(LIB_HEADERS)
	@echo "Wrote static library to lib/ and headers to include/"

lib/libInfomap.a: $(LIB_OBJECTS) Makefile
	@echo "Creating static library..."
	@mkdir -p lib
	ar rcs $@ $^

# Rule for $(LIB_HEADERS)
include/%.h: src/%.h
	@mkdir -p $(dir $@)
	@cp -a $^ $@

# Rule for $(LIB_OBJECTS)
build/lib/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DNS_INFOMAP -DAS_LIB -c $< -o $@


##################################################
# C bindings
##################################################

C_BUILD_DIR = build/c/infomap

.PHONY: c

c: Makefile
	# Put all C++ files in a flat directory
	# rsync src/* src/core/* src/io/* src/utils/* $(C_BUILD_DIR)/ &&\
	# cd $(C_BUILD_DIR) &&\
	# find . -type f -name "*.h" -or -name "*.cpp" -or -name "*.cxx" | xargs sed -i '' -E 's|(#include \")(.*/)*(.+)|\1\3 |'
	@mkdir -p $(C_BUILD_DIR)
	rsync src/* src/core/* src/io/* src/utils/* $(C_BUILD_DIR)/
	find $(C_BUILD_DIR) -type f -name "*.h" -or -name "*.cpp" -or -name "*.cxx" | xargs sed -i '' -E 's|(#include \")(.*/)*(.+)|\1\3 |'
	@true


##################################################
# General SWIG helpers
##################################################

SWIG_FILES := $(shell find interfaces/swig -name "*.i")


##################################################
# Python module
##################################################

PY_BUILD_DIR = build/py
PY_ONLY_HEADERS := $(HEADERS:%.h=$(PY_BUILD_DIR)/headers/%.h)
PY_HEADERS := $(HEADERS:src/%.h=$(PY_BUILD_DIR)/src/%.h)
PY_SOURCES := $(SOURCES:src/%.cpp=$(PY_BUILD_DIR)/src/%.cpp)

.PHONY: python py-build

# Use python distutils to compile the module
python: py-build Makefile
	@cp -a interfaces/python/setup.py $(PY_BUILD_DIR)/
	python utils/create-python-package-meta.py
	@cp -a interfaces/python/package_meta.py $(PY_BUILD_DIR)/
	@touch $(PY_BUILD_DIR)/__init__.py
	@cp -a $(PY_BUILD_DIR)/infomap.py $(PY_BUILD_DIR)/infomap_package.py
	cat $(PY_BUILD_DIR)/package_meta.py $(PY_BUILD_DIR)/infomap_api.py interfaces/python/infomap_cli.py > $(PY_BUILD_DIR)/infomap.py
	@cp -a interfaces/python/MANIFEST.in $(PY_BUILD_DIR)/
	@cp -a README.rst $(PY_BUILD_DIR)/
	@cp -a LICENSE_AGPLv3.txt $(PY_BUILD_DIR)/LICENSE
	cd $(PY_BUILD_DIR) && CC=$(CXX) python3 setup.py build_ext --inplace
	@true

# Generate wrapper files from source and interface files
py-build: $(PY_HEADERS) $(PY_SOURCES) $(PY_ONLY_HEADERS) interfaces/python/infomap.py
	@mkdir -p $(PY_BUILD_DIR)
	@cp -a $(SWIG_FILES) $(PY_BUILD_DIR)/
	swig -c++ -python -outdir $(PY_BUILD_DIR) -o $(PY_BUILD_DIR)/infomap_wrap.cpp $(PY_BUILD_DIR)/Infomap.i
	@cp -a $(PY_BUILD_DIR)/infomap.py $(PY_BUILD_DIR)/infomap_api.py

# Rule for $(PY_HEADERS) and $(PY_SOURCES)
$(PY_BUILD_DIR)/src/%: src/%
	@mkdir -p $(dir $@)
	@cp -a $^ $@

$(PY_BUILD_DIR)/headers/%: %
	@mkdir -p $(dir $@)
	@cp -a $^ $@

.PHONY: py-doc py-local-install
SPHINX_SOURCE_DIR = interfaces/python/source
SPHINX_TARGET_DIR = docs

py-local-install:
	# Run this to get 'import infomap' to always import the latest
	# locally built version, so no need to run this multiple times.
	pip install -e $(PY_BUILD_DIR)

py-doc:
	# Uses docstrings from the infomap available with 'import infomap'.
	# Run py-local-install if you don't have pip installed it with -e
	# and don't have the latest version installed
	@mkdir -p $(SPHINX_TARGET_DIR)
	@touch $(SPHINX_TARGET_DIR)/.nojekyll
	@cp -a README.rst ${SPHINX_SOURCE_DIR}/index.rst
	sphinx-build -b html $(SPHINX_SOURCE_DIR) $(SPHINX_TARGET_DIR)
	@npm run py-doc-prettier

.PHONY: pypitest_publish pypi_publish py_clean
PYPI_DIR = $(PY_BUILD_DIR)
PYPI_SDIST = $(shell find $(PYPI_DIR) -name "*.tar.gz" 2>/dev/null)

py_clean:
	$(RM) -r $(PY_BUILD_DIR)/dist

pypi_dist: py_clean python
	cd $(PY_BUILD_DIR) && python setup.py sdist bdist_wheel

# pip -vvv --no-cache-dir install --upgrade -I --index-url https://test.pypi.org/simple/ infomap
# pip install -e build/py/pypi/infomap/
pypitest_publish: pypi_dist
	# cd $(PYPI_DIR) && python setup.py sdist upload -r testpypi
	@[ "${PYPI_SDIST}" ] && echo "Publish dist..." || ( echo "dist files not built"; exit 1 )
	cd $(PYPI_DIR) && python -m twine upload -r testpypi dist/*

pypi_publish: pypi_dist
	@[ "${PYPI_SDIST}" ] && echo "Publish dist..." || ( echo "dist files not built"; exit 1 )
	cd $(PYPI_DIR) && python -m twine upload dist/*


##################################################
# R module
##################################################

R_BUILD_DIR = build/R
R_HEADERS := $(HEADERS:src/%.h=$(R_BUILD_DIR)/src/%.h)
R_SOURCES := $(SOURCES:src/%.cpp=$(R_BUILD_DIR)/src/%.cpp)

.PHONY: R R-build

# Use R to compile the module
R: R-build Makefile
	@mkdir -p $(R_BUILD_DIR)/Infomap
	@cp -a examples/R/load-infomap.R $(R_BUILD_DIR)/Infomap/
	cd $(R_BUILD_DIR) && CXX="$(CXX)" PKG_CPPFLAGS="$(CXXFLAGS) -DAS_LIB" PKG_LIBS="$(LDFLAGS)" R CMD SHLIB infomap_wrap.cpp $(SOURCES)
	@cp -a $(R_BUILD_DIR)/infomap.R $(R_BUILD_DIR)/Infomap/
	@cp -a $(R_BUILD_DIR)/infomap_wrap.so $(R_BUILD_DIR)/Infomap/infomap.so
	@cp -a examples/R/example-minimal.R $(R_BUILD_DIR)/Infomap/
	@true

# Generate wrapper files from source and interface files
R-build: Makefile $(R_HEADERS) $(R_SOURCES)
	@mkdir -p $(R_BUILD_DIR)
	@cp -a $(SWIG_FILES) $(R_BUILD_DIR)/
	swig -c++ -r -outdir $(R_BUILD_DIR) -o $(R_BUILD_DIR)/infomap_wrap.cpp $(R_BUILD_DIR)/Infomap.i

# Rule for $(R_HEADERS) and $(R_SOURCES)
$(R_BUILD_DIR)/src/%: src/%
	@mkdir -p $(dir $@)
	@cp -a $^ $@


##################################################
# Docker
##################################################

TAG_NAME = mapequation/infomap

docker-build: Makefile docker/infomap.Dockerfile
	docker-compose build infomap

docker-run: Makefile
	docker-compose run --rm infomap

docker-build-notebook: Makefile docker/notebook.Dockerfile
	docker-compose build notebook

docker-run-notebook: Makefile
	docker-compose up notebook

# rstudio
docker-build-rstudio: Makefile docker/rstudio.Dockerfile
	docker build \
	-f docker/rstudio.Dockerfile \
	-t $(TAG_NAME):rstudio .

docker-run-rstudio: Makefile
	docker run --rm \
	$(TAG_NAME):rstudio

# swig python
docker-build-swig-python: Makefile
	docker build -f docker/swig.python.Dockerfile -t infomap:python .

docker-run-swig-python: Makefile
	docker run --rm infomap:python

# ubuntu test python
docker-build-ubuntu-test-python: Makefile
	docker build -f docker/ubuntu.Dockerfile -t infomap:python-test .

docker-run-ubuntu-test-python: Makefile
	docker run --rm infomap:python-test

# R with RStudio
docker-build-r: Makefile
	docker build -f docker/rstudio.Dockerfile -t infomap:r .

docker-run-r: Makefile
	docker run --rm -p 8787:8787 -e PASSWORD=InfomapR infomap:r

# docker-run:
# 	docker run -it --rm -v $(pwd):/home/rstudio infomap \
	ninetriangles.net output

##################################################
# Clean
##################################################

clean: js-clean
	$(RM) -r Infomap build lib include
