CMAKE_TEST_BUILD_DIR ?= build/cmake
SANITIZER_BUILD_DIR ?= build/cmake-sanitizers
CMAKE_BUILD_TYPE ?= RelWithDebInfo
CMAKE_TEST_TARGET ?= infomap_cpp_tests
CMAKE_GENERATOR ?=
CMAKE_CXX_COMPILER ?=
TEST_CMAKE_ARGS ?=
CTEST_ARGS ?=
SANITIZER_CXX ?= clang++
SANITIZER_CMAKE_ARGS ?=
BENCHMARK_OUTPUT ?= build/benchmarks/python-benchmarks.json
BENCHMARK_SUMMARY ?=
NATIVE_BENCHMARK_TARGET ?= infomap_native_benchmark
NATIVE_BENCHMARK_OUTPUT ?= build/benchmarks/native-benchmarks.json
NATIVE_BENCHMARK_SUMMARY ?=
NATIVE_BENCHMARK_PROFILE ?= baseline
NATIVE_BENCHMARK_REPEATS ?= 3
NATIVE_BENCHMARK_ITERATIONS ?= 1

.PHONY: test-native test-fast test-sanitizers bench-python bench-native

test-native:
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	$(CMAKE) -S . -B $(CMAKE_TEST_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		$(if $(CMAKE_CXX_COMPILER),-DCMAKE_CXX_COMPILER=$(CMAKE_CXX_COMPILER),) \
		$(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),-DCMAKE_CXX_COMPILER_LAUNCHER=$(CCACHE_BIN),) \
		-DINFOMAP_MODE=$(MODE) \
		-DINFOMAP_USE_OPENMP=$(if $(filter 1,$(OPENMP)),ON,OFF) \
		-DINFOMAP_EXTRA_CPPFLAGS="$(CPPFLAGS)" \
		-DINFOMAP_EXTRA_CXX_FLAGS="$(CXXFLAGS)" \
		-DINFOMAP_EXTRA_LINK_FLAGS="$(LDFLAGS)" \
		$(TEST_CMAKE_ARGS)
	@$(CMAKE) --build $(CMAKE_TEST_BUILD_DIR) --target $(CMAKE_TEST_TARGET) --parallel $(JOBS)
	@$(CTEST) --test-dir $(CMAKE_TEST_BUILD_DIR) --output-on-failure $(CTEST_ARGS)

test-fast: test-native test-python-unit
	@true

test-sanitizers:
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	$(CMAKE) -S . -B $(SANITIZER_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=Debug \
		$(if $(SANITIZER_CXX),-DCMAKE_CXX_COMPILER=$(SANITIZER_CXX),) \
		$(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),-DCMAKE_CXX_COMPILER_LAUNCHER=$(CCACHE_BIN),) \
		-DINFOMAP_MODE=debug \
		-DINFOMAP_USE_OPENMP=OFF \
		-DINFOMAP_ENABLE_SANITIZERS=ON \
		-DINFOMAP_EXTRA_CPPFLAGS="$(CPPFLAGS)" \
		-DINFOMAP_EXTRA_CXX_FLAGS="$(CXXFLAGS)" \
		-DINFOMAP_EXTRA_LINK_FLAGS="$(LDFLAGS)" \
		$(SANITIZER_CMAKE_ARGS)
	@$(CMAKE) --build $(SANITIZER_BUILD_DIR) --target $(CMAKE_TEST_TARGET) --parallel $(JOBS)
	@ASAN_OPTS="strict_string_checks=1"; \
	if [ "$$(uname -s)" = "Darwin" ]; then \
		ASAN_OPTS="$$ASAN_OPTS:detect_leaks=0"; \
	else \
		ASAN_OPTS="$$ASAN_OPTS:detect_leaks=1"; \
	fi; \
	ASAN_OPTIONS="$$ASAN_OPTS" UBSAN_OPTIONS=print_stacktrace=1 $(CTEST) --test-dir $(SANITIZER_BUILD_DIR) --output-on-failure $(CTEST_ARGS)

bench-python:
	@mkdir -p $(dir $(BENCHMARK_OUTPUT))
	@$(PYTHON) scripts/benchmarks/run_python_benchmarks.py --output $(BENCHMARK_OUTPUT) $(if $(BENCHMARK_SUMMARY),--summary $(BENCHMARK_SUMMARY),)

bench-native:
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	$(CMAKE) -S . -B $(CMAKE_TEST_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		$(if $(CMAKE_CXX_COMPILER),-DCMAKE_CXX_COMPILER=$(CMAKE_CXX_COMPILER),) \
		$(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),-DCMAKE_CXX_COMPILER_LAUNCHER=$(CCACHE_BIN),) \
		-DINFOMAP_MODE=$(MODE) \
		-DINFOMAP_USE_OPENMP=$(if $(filter 1,$(OPENMP)),ON,OFF) \
		-DINFOMAP_EXTRA_CPPFLAGS="$(CPPFLAGS)" \
		-DINFOMAP_EXTRA_CXX_FLAGS="$(CXXFLAGS)" \
		-DINFOMAP_EXTRA_LINK_FLAGS="$(LDFLAGS)" \
		$(TEST_CMAKE_ARGS)
	@$(CMAKE) --build $(CMAKE_TEST_BUILD_DIR) --target $(NATIVE_BENCHMARK_TARGET) --parallel $(JOBS)
	@mkdir -p $(dir $(NATIVE_BENCHMARK_OUTPUT))
	@$(PYTHON) scripts/benchmarks/run_native_benchmarks.py \
		--binary $(CMAKE_TEST_BUILD_DIR)/$(NATIVE_BENCHMARK_TARGET) \
		--output $(NATIVE_BENCHMARK_OUTPUT) \
		--profile $(NATIVE_BENCHMARK_PROFILE) \
		--repeats $(NATIVE_BENCHMARK_REPEATS) \
		--iterations $(NATIVE_BENCHMARK_ITERATIONS) \
		$(if $(NATIVE_BENCHMARK_SUMMARY),--summary $(NATIVE_BENCHMARK_SUMMARY),)
