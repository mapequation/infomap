CXXFLAGS = -Wall -O3

# Set INFOMAP_DIR to your Infomap directory
INFOMAP_DIR = ../..
PY_BUILD_DIR = $(INFOMAP_DIR)/build/py
PY3_BUILD_DIR = $(INFOMAP_DIR)/build/py3
PY_WRAPPER = $(PY_BUILD_DIR)/infomap.py
PY3_WRAPPER = $(PY3_BUILD_DIR)/infomap.py
INFOMAP_PY_LIB = $(PY_BUILD_DIR)/_infomap.so
INFOMAP_PY3_LIB = $(PY3_BUILD_DIR)/_infomap.so

.PHONY: clean distclean python python3

python: $(PY_WRAPPER) Makefile
	@echo Copying Infomap library and the python wrapper to a local module folder...
	@mkdir -p infomap
	@cp -av $(PY_WRAPPER) $(INFOMAP_PY_LIB) infomap
	touch infomap/__init__.py

python3: $(PY3_WRAPPER) Makefile
	@echo Copying Infomap library and the python wrapper to a local module folder...
	@mkdir -p infomap
	@cp -av $(PY3_WRAPPER) $(INFOMAP_PY3_LIB) infomap
	touch infomap/__init__.py

$(PY_WRAPPER):
	$(MAKE) -C $(INFOMAP_DIR) python

$(PY3_WRAPPER):
	$(MAKE) -C $(INFOMAP_DIR) python3

clean:
	$(RM) -r infomap

distclean:
	$(MAKE) -C $(INFOMAP_DIR) clean
