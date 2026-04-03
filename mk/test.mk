CMAKE_TEST_BUILD_DIR ?= build/cmake
SANITIZER_BUILD_DIR ?= build/cmake-sanitizers
CMAKE_BUILD_TYPE ?= RelWithDebInfo
CMAKE_TEST_TARGET ?= infomap_cpp_tests
CMAKE_GENERATOR ?=
CMAKE_CXX_COMPILER ?=
TEST_CMAKE_ARGS ?=
SANITIZER_CXX ?= clang++
SANITIZER_CMAKE_ARGS ?=
BENCHMARK_OUTPUT ?= build/benchmarks/python-benchmarks.json
BENCHMARK_SUMMARY ?=

.PHONY: test-native test-fast test-sanitizers bench-python

test-native:
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	$(CMAKE) -S . -B $(CMAKE_TEST_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		$(if $(CMAKE_CXX_COMPILER),-DCMAKE_CXX_COMPILER=$(CMAKE_CXX_COMPILER),) \
		-DINFOMAP_USE_OPENMP=$(if $(filter 1,$(OPENMP)),ON,OFF) \
		-DINFOMAP_EXTRA_CXX_FLAGS="$(CMAKE_EXTRA_CXX_FLAGS)" \
		-DINFOMAP_EXTRA_LINK_FLAGS="$(CMAKE_EXTRA_LINK_FLAGS)" \
		$(TEST_CMAKE_ARGS)
	@$(CMAKE) --build $(CMAKE_TEST_BUILD_DIR) --target $(CMAKE_TEST_TARGET) --parallel
	@$(CTEST) --test-dir $(CMAKE_TEST_BUILD_DIR) --output-on-failure

test-fast: test-native test-python-unit
	@true

test-sanitizers:
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	$(CMAKE) -S . -B $(SANITIZER_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=Debug \
		$(if $(SANITIZER_CXX),-DCMAKE_CXX_COMPILER=$(SANITIZER_CXX),) \
		-DINFOMAP_USE_OPENMP=OFF \
		-DINFOMAP_ENABLE_SANITIZERS=ON \
		-DINFOMAP_EXTRA_CXX_FLAGS="$(CMAKE_EXTRA_CXX_FLAGS)" \
		-DINFOMAP_EXTRA_LINK_FLAGS="$(CMAKE_EXTRA_LINK_FLAGS)" \
		$(SANITIZER_CMAKE_ARGS)
	@$(CMAKE) --build $(SANITIZER_BUILD_DIR) --target $(CMAKE_TEST_TARGET) --parallel
	@ASAN_OPTS="strict_string_checks=1"; \
	if [ "$$(uname -s)" = "Darwin" ]; then \
		ASAN_OPTS="$$ASAN_OPTS:detect_leaks=0"; \
	else \
		ASAN_OPTS="$$ASAN_OPTS:detect_leaks=1"; \
	fi; \
	ASAN_OPTIONS="$$ASAN_OPTS" UBSAN_OPTIONS=print_stacktrace=1 $(CTEST) --test-dir $(SANITIZER_BUILD_DIR) --output-on-failure

bench-python:
	@mkdir -p $(dir $(BENCHMARK_OUTPUT))
	@$(PYTHON) scripts/benchmarks/run_python_benchmarks.py --output $(BENCHMARK_OUTPUT) $(if $(BENCHMARK_SUMMARY),--summary $(BENCHMARK_SUMMARY),)
