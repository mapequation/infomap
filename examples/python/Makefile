CXXFLAGS = -Wall -O3

# Set INFOMAP_DIR to your Infomap directory
INFOMAP_DIR = ../..
PY_BUILD_DIR = $(INFOMAP_DIR)/build/py
PY2_BUILD_DIR = $(INFOMAP_DIR)/build/py2
PY_WRAPPER = $(PY_BUILD_DIR)/infomap.py
PY2_WRAPPER = $(PY2_BUILD_DIR)/infomap.py
INFOMAP_PY_LIB = $(PY_BUILD_DIR)/_infomap.so
INFOMAP_PY2_LIB = $(PY2_BUILD_DIR)/_infomap.so

.PHONY: clean distclean python python2

python: $(PY_WRAPPER) Makefile
	@echo Copying Infomap library and the python wrapper to a local module folder...
	@cp -av $(PY_WRAPPER) $(INFOMAP_PY_LIB) .
	touch __init__.py

python2: $(PY2_WRAPPER) Makefile
	@echo Copying Infomap library and the python wrapper to a local module folder...
	@cp -av $(PY2_WRAPPER) $(INFOMAP_PY2_LIB) .
	touch __init__.py

$(PY_WRAPPER):
	$(MAKE) -C $(INFOMAP_DIR) python

$(PY2_WRAPPER):
	$(MAKE) -C $(INFOMAP_DIR) python2


clean:
	$(RM) -r __init__.py infomap.py _infomap.so

distclean:
	$(MAKE) -C $(INFOMAP_DIR) clean
