CXXFLAGS = -Wall -O3

# Set INFOMAP_DIR to your Infomap directory
INFOMAP_DIR = ../..
PY_BUILD_DIR = $(INFOMAP_DIR)/build/py
MODULE_FILES = $(PY_BUILD_DIR)/infomap.py $(PY_BUILD_DIR)/_infomap.so

infomap-module: infomap/infomap.py Makefile
	@true

infomap/infomap.py: $(MODULE_FILES)
	@echo Copying Infomap library and the python wrapper to a local module folder...
	@mkdir -p infomap
	@cp -av $^ infomap
	touch infomap/__init__.py

$(PY_BUILD_DIR)/infomap.py:
	cd $(INFOMAP_DIR) && $(MAKE) python

clean:
	$(RM) -r infomap

distclean:
	cd $(INFOMAP_DIR) && $(MAKE) clean