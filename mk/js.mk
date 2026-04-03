WORKER_FILENAME := infomap.worker.js
JS_WORKER_TARGET := build/js/$(WORKER_FILENAME)
PRE_WORKER_MODULE := interfaces/js/pre-worker-module.js
JS_METADATA_DIR := interfaces/js/generated
JS_CHANGELOG_JSON := $(JS_METADATA_DIR)/changelog.json
JS_PARAMETERS_JSON := $(JS_METADATA_DIR)/parameters.json
JS_METADATA_FILES := $(JS_CHANGELOG_JSON) $(JS_PARAMETERS_JSON)

.PHONY: build-js build-js-metadata test-js-metadata test-js clean-js format-js

build-js: $(JS_WORKER_TARGET) $(JS_METADATA_FILES)
	@echo "Built $^"
	@mkdir -p interfaces/js/src/worker
	cp build/js/* interfaces/js/src/worker/
	$(NPM) run build
	cp interfaces/js/README.md .

build-js-metadata: build-native
	@$(PYTHON_FOR_BUILD_CONFIG) scripts/generate_js_metadata.py --infomap-bin ./Infomap --changelog CHANGELOG.md --output-dir $(JS_METADATA_DIR)
	@printf "Wrote JS metadata to %s\n" "$(JS_METADATA_DIR)"

test-js-metadata: build-native
	@tmpdir="$$(mktemp -d)"; \
	$(PYTHON_FOR_BUILD_CONFIG) scripts/generate_js_metadata.py --infomap-bin ./Infomap --changelog CHANGELOG.md --output-dir "$$tmpdir"; \
	diff -u "$(JS_CHANGELOG_JSON)" "$$tmpdir/changelog.json"; \
	diff -u "$(JS_PARAMETERS_JSON)" "$$tmpdir/parameters.json"; \
	rm -rf "$$tmpdir"

$(JS_WORKER_TARGET): $(SOURCES) $(HEADERS) $(PRE_WORKER_MODULE) $(MK_FILES) Makefile
	@echo "Compiling Infomap to run in a worker in the browser..."
	@mkdir -p $(dir $@)
	$(EMXX) -std=c++14 -O3 -s WASM=0 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=0 -s ENVIRONMENT=worker --pre-js $(PRE_WORKER_MODULE) -o $@ $(SOURCES)

test-js: build-js
	$(RM) -r package mapequation-infomap-*.tgz npm-pack.json
	$(NPM) pack
	tar -xzvf mapequation-infomap-*.tgz
	cp package/index.js examples/js
	@browser_target="examples/js/infomap-worker.html"; \
	if [ -n "$$CI" ]; then \
		echo "Built browser example at $$browser_target"; \
	elif command -v open >/dev/null 2>&1; then \
		open "$$browser_target" || echo "Open $$browser_target manually"; \
	elif command -v xdg-open >/dev/null 2>&1; then \
		xdg-open "$$browser_target" || echo "Open $$browser_target manually"; \
	else \
		echo "Open $$browser_target manually"; \
	fi

clean-js:
	$(RM) -r build/js interfaces/js/src/worker package mapequation-infomap-*.tgz npm-pack.json dist/npm

format-js:
	prettier --write interfaces/js
