CXXFLAGS = -Wall -O3

# Set INFOMAP_DIR to your Infomap directory
INFOMAP_DIR = ../..
PY_BUILD_DIR = $(INFOMAP_DIR)/build/py
PY_WRAPPER = $(PY_BUILD_DIR)/infomap.py
INFOMAP_LIB = $(PY_BUILD_DIR)/_infomap.so

.PHONY: clean distclean

infomap/infomap.py: $(PY_WRAPPER) Makefile
	@echo Copying Infomap library and the python wrapper to a local module folder...
	@mkdir -p infomap
	@cp -av $(PY_WRAPPER) $(INFOMAP_LIB) infomap
	touch infomap/__init__.py

$(PY_WRAPPER):
	cd $(INFOMAP_DIR) && $(MAKE) python

clean:
	$(RM) -r infomap

distclean:
	cd $(INFOMAP_DIR) && $(MAKE) clean
