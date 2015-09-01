CXXFLAGS = -Wall -O3

# Set INFOMAP_DIR to your Infomap directory
INFOMAP_DIR = ../..
JS_BUILD_DIR = $(INFOMAP_DIR)/build/js
JS_WORKER = $(JS_BUILD_DIR)/Infomap-worker.js
JS_NODE = $(JS_BUILD_DIR)/Infomap.js

.PHONY: clean distclean

Infomap-worker.js: $(JS_WORKER) Makefile
	@cp -av $(JS_WORKER) .

Infomap.js: $(JS_NODE) Makefile
	@cp -av $(JS_NODE) .

$(JS_WORKER):
	cd $(INFOMAP_DIR) && $(MAKE) js-worker

$(JS_NODE):
	cd $(INFOMAP_DIR) && $(MAKE) js

clean:
	$(RM) -r Infomap-worker.js
	$(RM) -r Infomap.js

distclean:
	cd $(INFOMAP_DIR) && $(MAKE) clean
