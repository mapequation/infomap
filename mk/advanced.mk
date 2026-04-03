##################################################
# Advanced / secondary targets
##################################################

R_BUILD_DIR := build/R
R_HEADERS := $(HEADERS:src/%.h=$(R_BUILD_DIR)/src/%.h)
R_SOURCES := $(SOURCES:src/%.cpp=$(R_BUILD_DIR)/src/%.cpp)
TAG_NAME := mapequation/infomap

.PHONY: \
	R \
	R-build \
	docker-build \
	docker-run \
	docker-build-notebook \
	docker-run-notebook \
	docker-build-rstudio \
	docker-run-rstudio \
	docker-build-ubuntu-test-python \
	docker-run-ubuntu-test-python \
	docker-build-python \
	docker-build-r \
	docker-run-r

R: R-build
	@mkdir -p $(R_BUILD_DIR)/Infomap
	@cp -a examples/R/load-infomap.R $(R_BUILD_DIR)/Infomap/
	cd $(R_BUILD_DIR) && CXX="$(CXX)" PKG_CPPFLAGS="$(NATIVE_CXXFLAGS) -DAS_LIB" PKG_LIBS="$(NATIVE_LDFLAGS)" R CMD SHLIB infomap_wrap.cpp $(SOURCES)
	@cp -a $(R_BUILD_DIR)/infomap.R $(R_BUILD_DIR)/Infomap/
	@cp -a $(R_BUILD_DIR)/infomap_wrap.so $(R_BUILD_DIR)/Infomap/infomap.so
	@cp -a examples/R/example-minimal.R $(R_BUILD_DIR)/Infomap/

R-build: $(R_HEADERS) $(R_SOURCES) $(MK_FILES) Makefile
	@mkdir -p $(R_BUILD_DIR)
	@cp -a $(SWIG_FILES) $(R_BUILD_DIR)/
	$(SWIG) -c++ -r -outdir $(R_BUILD_DIR) -o $(R_BUILD_DIR)/infomap_wrap.cpp $(R_BUILD_DIR)/Infomap.i

$(R_BUILD_DIR)/src/%: src/%
	@mkdir -p $(dir $@)
	@cp -a $^ $@

docker-build: docker/infomap.Dockerfile
	docker-compose build infomap

docker-run:
	docker-compose run --rm infomap

docker-build-notebook: docker/notebook.Dockerfile
	docker-compose build notebook

docker-run-notebook:
	docker-compose up notebook

docker-build-rstudio: docker/rstudio.Dockerfile
	docker build \
	-f docker/rstudio.Dockerfile \
	-t $(TAG_NAME):rstudio .

docker-run-rstudio:
	docker run --rm \
	$(TAG_NAME):rstudio

docker-build-ubuntu-test-python:
	docker build -f docker/ubuntu.Dockerfile -t infomap:python-test .

docker-run-ubuntu-test-python:
	docker run --rm infomap:python-test

docker-build-python: docker/python.Dockerfile
	docker build -f docker/python.Dockerfile -t infomap-python .

docker-build-r:
	docker build -f docker/rstudio.Dockerfile -t infomap:r .

docker-run-r:
	docker run --rm -p 8787:8787 -e PASSWORD=InfomapR infomap:r
