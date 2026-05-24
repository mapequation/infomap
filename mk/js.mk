WORKER_FILENAME := infomap.worker.js
JS_WORKER_TARGET := build/js/$(WORKER_FILENAME)
PRE_WORKER_MODULE := interfaces/js/pre-worker-module.js
JS_METADATA_DIR := interfaces/js/generated
JS_PARAMETERS_JSON := $(JS_METADATA_DIR)/parameters.json
JS_OUTPUT_FORMATS_JSON := $(JS_METADATA_DIR)/output-formats.json
JS_METADATA_FILES := $(JS_PARAMETERS_JSON)
JS_WORKER_OUTPUT_FORMATS := build/js/output-formats-worker.js
PRE_WORKER_MODULES := $(JS_WORKER_OUTPUT_FORMATS) $(PRE_WORKER_MODULE)
NPM_STAGE_DIR := dist/npm/package
NPM_UNPACK_DIR := dist/npm/unpacked
NPM_PACK_JSON := dist/npm/npm-pack.json

.PHONY: build-js build-js-metadata test-js-metadata test-js clean-js format-js

build-js: $(JS_WORKER_TARGET) $(JS_METADATA_FILES)
	$(RM) -r $(NPM_STAGE_DIR)
	@mkdir -p $(NPM_STAGE_DIR)
	$(NPM) run build:package
	@$(PYTHON_FOR_BUILD_CONFIG) scripts/prepare_npm_package.py --source-package package.json --readme interfaces/js/README.md --readme-rst README.rst --license LICENSE_GPLv3.txt --output-dir $(NPM_STAGE_DIR)
	@echo "Built $^ into $(NPM_STAGE_DIR)"

build-js-metadata: build-native
	@$(PYTHON_FOR_BUILD_CONFIG) scripts/generate_js_metadata.py --infomap-bin ./Infomap --output-dir $(JS_METADATA_DIR)
	@printf "Wrote JS metadata to %s\n" "$(JS_METADATA_DIR)"

test-js-metadata: build-native
	@tmpdir="$$(mktemp -d)"; \
	$(PYTHON_FOR_BUILD_CONFIG) scripts/generate_js_metadata.py --infomap-bin ./Infomap --output-dir "$$tmpdir"; \
	diff -u "$(JS_PARAMETERS_JSON)" "$$tmpdir/parameters.json"; \
	rm -rf "$$tmpdir"

$(JS_WORKER_OUTPUT_FORMATS): $(JS_OUTPUT_FORMATS_JSON) interfaces/js/scripts/write-worker-output-formats.mjs
	$(NPM) run build:worker-output-formats -- $@

$(JS_WORKER_TARGET): $(SOURCES) $(HEADERS) $(PRE_WORKER_MODULES) $(MK_FILES) Makefile
	@echo "Compiling Infomap to run in a worker in the browser..."
	@mkdir -p $(dir $@)
	$(EMXX) -std=c++14 -O3 -s SINGLE_FILE=1 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=0 -s ENVIRONMENT=worker $(foreach file,$(PRE_WORKER_MODULES),--pre-js $(file)) -o $@ $(SOURCES)

test-js: build-js
	$(RM) -r $(NPM_UNPACK_DIR) $(NPM_STAGE_DIR)/*.tgz $(NPM_PACK_JSON)
	@mkdir -p dist/npm
	$(NPM) run lint
	$(NPM) run typecheck
	$(NPM) run test:unit
	$(NPM) pack --json $(NPM_STAGE_DIR) > $(NPM_PACK_JSON)
	@pack_json="$(NPM_PACK_JSON)"; \
	pkg="$$(node --input-type=module -e "import fs from 'node:fs'; const [entry] = JSON.parse(fs.readFileSync(process.argv[1], 'utf8')); console.log(entry.filename);" "$$pack_json")"; \
	mv "$$pkg" dist/npm/; \
	mkdir -p $(NPM_UNPACK_DIR); \
	tar -xzf dist/npm/"$$pkg" -C $(NPM_UNPACK_DIR)
	$(NPM) run test:package
	$(NPM) run test:browser

clean-js:
	$(RM) -r build/js dist/npm

format-js:
	$(NPM) run format
