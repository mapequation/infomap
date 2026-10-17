CMAKE_TEST_BUILD_DIR ?= build/cmake
CMAKE_DEV_BUILD_DIR ?= build/cmake-dev
CMAKE_DEV_GENERATOR ?= Ninja
CMAKE_DEV_OPENMP ?= 0
CMAKE_DEV_CXX_COMPILER ?=
CMAKE_DEV_OSX_SYSROOT ?= $(if $(filter Darwin,$(UNAME_S)),$(shell xcrun --show-sdk-path 2>/dev/null || true),)
CLANG_TIDY_WARNINGS_AS_ERRORS ?=
SANITIZER_BUILD_DIR ?= build/cmake-sanitizers
DEFAULT_CMAKE_BUILD_TYPE := $(if $(filter debug,$(MODE)),Debug,RelWithDebInfo)
CMAKE_BUILD_TYPE ?= $(DEFAULT_CMAKE_BUILD_TYPE)
CMAKE_TEST_TARGET ?= infomap_cpp_tests
CMAKE_GENERATOR ?=
CMAKE_CXX_COMPILER ?=
TEST_CMAKE_ARGS ?=
CTEST_ARGS ?=
SANITIZER_CXX ?= clang++
SANITIZER_CMAKE_ARGS ?=
TSAN_BUILD_DIR ?= build/cmake-tsan
TSAN_CXX ?= clang++
TSAN_CMAKE_ARGS ?=
TSAN_OMP_NUM_THREADS ?= 4
TSAN_SUPPRESSIONS ?= $(CURDIR)/test/tsan.supp
# Archer (libarcher, from LLVM's openmp/tools/archer) tells TSan about OpenMP's
# own synchronisation. Without it every barrier, reduction and task wait reads
# as a race. Linux LLVM packages ship it next to libomp; Homebrew does not, see
# test-tsan below.
TSAN_ARCHER_LIB ?= $(shell p=$$($(TSAN_CXX) -print-file-name=libarcher.so 2>/dev/null); [ -f "$$p" ] && echo "$$p")
FUZZ_BUILD_DIR ?= build/fuzz
FUZZ_SECONDS ?= 60
FUZZ_CXX ?= clang++
FUZZ_CC ?= clang
FUZZ_SEED_DIRS ?= test/fixtures/networks test/fixtures/networks/json examples/networks
BENCHMARK_OUTPUT ?= build/benchmarks/python-benchmarks.json
BENCHMARK_SUMMARY ?=
NATIVE_BENCHMARK_TARGET ?= infomap_native_benchmark
NATIVE_BENCHMARK_OUTPUT ?= build/benchmarks/native-benchmarks.json
NATIVE_BENCHMARK_SUMMARY ?=
NATIVE_BENCHMARK_PROFILE ?= baseline
NATIVE_BENCHMARK_REPEATS ?= 3
NATIVE_BENCHMARK_WARMUP_RUNS ?= 0
NATIVE_BENCHMARK_ITERATIONS ?= 1
NATIVE_BENCHMARK_FLAGS ?=
NATIVE_BENCHMARK_GENERATED_DIR ?=

define warn_cmake_build_type_mismatch
	@if [ "$(MODE)" = "debug" ] && [ "$(CMAKE_BUILD_TYPE)" != "Debug" ]; then \
		printf "%s\n" "Warning: MODE=debug controls Infomap compile flags; CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) only affects generator metadata and output layout."; \
	fi
	@if [ "$(MODE)" = "release" ] && [ "$(CMAKE_BUILD_TYPE)" = "Debug" ]; then \
		printf "%s\n" "Warning: MODE=release controls Infomap compile flags; CMAKE_BUILD_TYPE=Debug only affects generator metadata and output layout."; \
	fi
endef

.PHONY: configure-cpp-dev tidy-native dev-cpp-check test-cpp-stream-policy test-native test-binding-options-freshness test-fast test-sanitizers test-tsan fuzz bench-python bench-native

configure-cpp-dev:
	@generator_args=""; \
	if [ -n "$(CMAKE_DEV_GENERATOR)" ]; then generator_args="-G $(CMAKE_DEV_GENERATOR)"; fi; \
	"$(CMAKE)" -S . -B $(CMAKE_DEV_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=Debug \
		$(if $(CMAKE_DEV_CXX_COMPILER),-DCMAKE_CXX_COMPILER=$(CMAKE_DEV_CXX_COMPILER),) \
		$(if $(CMAKE_DEV_OSX_SYSROOT),-DCMAKE_OSX_SYSROOT=$(CMAKE_DEV_OSX_SYSROOT),) \
		-DINFOMAP_MODE=debug \
		-DINFOMAP_USE_OPENMP=$(if $(filter 1,$(CMAKE_DEV_OPENMP)),ON,OFF) \
		-DINFOMAP_NATIVE_ARCH=$(if $(filter 1,$(NATIVE_ARCH)),ON,OFF) \
		-DINFOMAP_FEATURES="$(FEATURES)" \
		-DINFOMAP_EXTRA_CPPFLAGS="$(CPPFLAGS)" \
		-DINFOMAP_EXTRA_CXX_FLAGS="$(CXXFLAGS)" \
		-DINFOMAP_EXTRA_LINK_FLAGS="$(LDFLAGS)" \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@ln -sfn $(CMAKE_DEV_BUILD_DIR)/compile_commands.json compile_commands.json

tidy-native: configure-cpp-dev
	$(CLANG_TIDY) --quiet $(if $(CLANG_TIDY_WARNINGS_AS_ERRORS),--warnings-as-errors=$(CLANG_TIDY_WARNINGS_AS_ERRORS),) -p $(CMAKE_DEV_BUILD_DIR) $(SOURCES)

dev-cpp-check: format-native-check test-cpp-stream-policy
	@$(MAKE) test-native MODE=debug OPENMP=0 CMAKE_GENERATOR="$(CMAKE_DEV_GENERATOR)" CMAKE_TEST_BUILD_DIR=$(CMAKE_DEV_BUILD_DIR) CMAKE_BUILD_TYPE=Debug

test-cpp-stream-policy:
	@$(PYTHON) scripts/check_cpp_stream_policy.py

test-binding-options-freshness: build-native
	@$(PYTHON_FOR_BUILD_CONFIG) $(BINDING_OPTIONS_SCRIPT) --check --infomap-bin ./Infomap --output-root .

test-native:
	$(warn_cmake_build_type_mismatch)
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	"$(CMAKE)" -S . -B $(CMAKE_TEST_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		$(if $(CMAKE_CXX_COMPILER),-DCMAKE_CXX_COMPILER=$(CMAKE_CXX_COMPILER),) \
		$(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),-DCMAKE_CXX_COMPILER_LAUNCHER=$(CCACHE_BIN),) \
		-DINFOMAP_MODE=$(MODE) \
		-DINFOMAP_USE_OPENMP=$(if $(filter 1,$(OPENMP)),ON,OFF) \
		-DINFOMAP_NATIVE_ARCH=$(if $(filter 1,$(NATIVE_ARCH)),ON,OFF) \
		-DINFOMAP_FEATURES="$(FEATURES)" \
		-DINFOMAP_EXTRA_CPPFLAGS="$(CPPFLAGS)" \
		-DINFOMAP_EXTRA_CXX_FLAGS="$(CXXFLAGS)" \
		-DINFOMAP_EXTRA_LINK_FLAGS="$(LDFLAGS)" \
		$(TEST_CMAKE_ARGS)
	@"$(CMAKE)" --build $(CMAKE_TEST_BUILD_DIR) --target $(CMAKE_TEST_TARGET) --parallel $(JOBS)
	@"$(CTEST)" --test-dir $(CMAKE_TEST_BUILD_DIR) --output-on-failure $(CTEST_ARGS)

test-fast: test-cpp-stream-policy test-native test-python-unit
	@true

test-sanitizers:
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	"$(CMAKE)" -S . -B $(SANITIZER_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=Debug \
		$(if $(SANITIZER_CXX),-DCMAKE_CXX_COMPILER=$(SANITIZER_CXX),) \
		$(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),-DCMAKE_CXX_COMPILER_LAUNCHER=$(CCACHE_BIN),) \
		-DINFOMAP_MODE=debug \
		-DINFOMAP_USE_OPENMP=OFF \
		-DINFOMAP_ENABLE_SANITIZERS=ON \
		-DINFOMAP_FEATURES="$(FEATURES)" \
		-DINFOMAP_EXTRA_CPPFLAGS="$(CPPFLAGS)" \
		-DINFOMAP_EXTRA_CXX_FLAGS="$(CXXFLAGS)" \
		-DINFOMAP_EXTRA_LINK_FLAGS="$(LDFLAGS)" \
		$(SANITIZER_CMAKE_ARGS)
	@"$(CMAKE)" --build $(SANITIZER_BUILD_DIR) --target $(CMAKE_TEST_TARGET) --parallel $(JOBS)
	@ASAN_OPTS="strict_string_checks=1"; \
	if [ "$$(uname -s)" = "Darwin" ]; then \
		ASAN_OPTS="$$ASAN_OPTS:detect_leaks=0"; \
	else \
		ASAN_OPTS="$$ASAN_OPTS:detect_leaks=1"; \
	fi; \
	ASAN_OPTIONS="$$ASAN_OPTS" UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 "$(CTEST)" --test-dir $(SANITIZER_BUILD_DIR) --output-on-failure $(CTEST_ARGS)

# Run the C++ tests and the CLI's OpenMP paths (test/scripts/check_tsan_cli.py)
# under ThreadSanitizer with OpenMP on. Clang and LLVM's libomp only, with
# Archer loaded through OMP_TOOL_LIBRARIES. -O1 keeps TSan's slowdown bearable
# while the stacks stay readable. Suppressions for reports from the
# uninstrumented runtime live in test/tsan.supp.
#
# On Linux, install clang and the matching libomp-dev; Archer is found next to
# libomp. On macOS, Homebrew's llvm/libomp do not ship Archer. Build it from
# the one source file for your LLVM version and pass the path:
#   curl -fsSLO https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-<version>/openmp/tools/archer/ompt-tsan.cpp
#   clang++ -std=c++17 -O2 -shared -fPIC -undefined dynamic_lookup \
#     -I$$(brew --prefix libomp)/include ompt-tsan.cpp -o libarcher.dylib
#   make test-tsan TSAN_ARCHER_LIB=$$PWD/libarcher.dylib
test-tsan:
	@if [ -z "$(TSAN_ARCHER_LIB)" ] || [ ! -f "$(TSAN_ARCHER_LIB)" ]; then \
		printf "%s\n" "test-tsan: Archer not found; set TSAN_ARCHER_LIB (see mk/test.mk)." >&2; \
		exit 1; \
	fi
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	"$(CMAKE)" -S . -B $(TSAN_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		$(if $(TSAN_CXX),-DCMAKE_CXX_COMPILER=$(TSAN_CXX),) \
		$(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),-DCMAKE_CXX_COMPILER_LAUNCHER=$(CCACHE_BIN),) \
		-DINFOMAP_MODE=release \
		-DINFOMAP_USE_OPENMP=ON \
		-DINFOMAP_ENABLE_TSAN=ON \
		-DINFOMAP_FEATURES="$(FEATURES)" \
		-DINFOMAP_EXTRA_CPPFLAGS="$(CPPFLAGS)" \
		-DINFOMAP_EXTRA_CXX_FLAGS="-O1 -g $(CXXFLAGS)" \
		-DINFOMAP_EXTRA_LINK_FLAGS="$(LDFLAGS)" \
		$(TSAN_CMAKE_ARGS)
	@"$(CMAKE)" --build $(TSAN_BUILD_DIR) --target $(CMAKE_TEST_TARGET) --parallel $(JOBS)
	@OMP_TOOL_LIBRARIES="$(TSAN_ARCHER_LIB)" OMP_NUM_THREADS=$(TSAN_OMP_NUM_THREADS) \
		TSAN_OPTIONS="ignore_noninstrumented_modules=1 abort_on_error=0 second_deadlock_stack=1 suppressions=$(TSAN_SUPPRESSIONS)" \
		"$(CTEST)" --test-dir $(TSAN_BUILD_DIR) --output-on-failure $(CTEST_ARGS)

# Build and briefly run the libFuzzer harness for the network intake (Clang
# only; opt-in via INFOMAP_BUILD_FUZZERS, kept out of the default build/ctest).
# Feeds bytes through buildNetworkFromInput, which routes JSON vs Pajek, so the
# single target covers both parsers. Seeds from the tracked fixtures; new inputs
# and any findings (crash-*/timeout-*/leak-*) land under FUZZ_BUILD_DIR. Tune the
# run length with FUZZ_SECONDS (CI uses a larger budget than the default smoke).
fuzz:
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	"$(CMAKE)" -S . -B $(FUZZ_BUILD_DIR) $$generator_args \
		-DINFOMAP_BUILD_FUZZERS=ON \
		-DINFOMAP_USE_OPENMP=OFF \
		-DBUILD_TESTING=OFF \
		-DINFOMAP_MODE=debug \
		-DCMAKE_CXX_COMPILER=$(FUZZ_CXX) \
		-DCMAKE_C_COMPILER=$(FUZZ_CC)
	@"$(CMAKE)" --build $(FUZZ_BUILD_DIR) --target fuzz_intake --parallel $(JOBS)
	@mkdir -p $(FUZZ_BUILD_DIR)/corpus
	@ASAN_OPTIONS=strict_string_checks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
		$(FUZZ_BUILD_DIR)/fuzz_intake \
			-max_total_time=$(FUZZ_SECONDS) -max_len=1048576 -timeout=10 \
			-dict=test/fuzz/intake.dict \
			-artifact_prefix=$(FUZZ_BUILD_DIR)/ -print_final_stats=1 \
			$(FUZZ_BUILD_DIR)/corpus $(FUZZ_SEED_DIRS)

bench-python:
	@mkdir -p $(dir $(BENCHMARK_OUTPUT))
	@$(PYTHON) scripts/benchmarks/run_python_benchmarks.py --output $(BENCHMARK_OUTPUT) $(if $(BENCHMARK_SUMMARY),--summary $(BENCHMARK_SUMMARY),)

bench-native:
	$(warn_cmake_build_type_mismatch)
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	"$(CMAKE)" -S . -B $(CMAKE_TEST_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		$(if $(CMAKE_CXX_COMPILER),-DCMAKE_CXX_COMPILER=$(CMAKE_CXX_COMPILER),) \
		$(if $(and $(filter 1,$(USE_CCACHE)),$(CCACHE_BIN)),-DCMAKE_CXX_COMPILER_LAUNCHER=$(CCACHE_BIN),) \
		-DINFOMAP_MODE=$(MODE) \
		-DINFOMAP_USE_OPENMP=$(if $(filter 1,$(OPENMP)),ON,OFF) \
		-DINFOMAP_NATIVE_ARCH=$(if $(filter 1,$(NATIVE_ARCH)),ON,OFF) \
		-DINFOMAP_FEATURES="$(FEATURES)" \
		-DINFOMAP_EXTRA_CPPFLAGS="$(CPPFLAGS)" \
		-DINFOMAP_EXTRA_CXX_FLAGS="$(CXXFLAGS)" \
		-DINFOMAP_EXTRA_LINK_FLAGS="$(LDFLAGS)" \
		$(TEST_CMAKE_ARGS)
	@"$(CMAKE)" --build $(CMAKE_TEST_BUILD_DIR) --target $(NATIVE_BENCHMARK_TARGET) --parallel $(JOBS)
	@mkdir -p $(dir $(NATIVE_BENCHMARK_OUTPUT))
	@$(PYTHON) scripts/benchmarks/run_native_benchmarks.py \
		--binary $(CMAKE_TEST_BUILD_DIR)/$(NATIVE_BENCHMARK_TARGET) \
		--output $(NATIVE_BENCHMARK_OUTPUT) \
		--profile $(NATIVE_BENCHMARK_PROFILE) \
		--repeats $(NATIVE_BENCHMARK_REPEATS) \
		--warmup-runs $(NATIVE_BENCHMARK_WARMUP_RUNS) \
		--iterations $(NATIVE_BENCHMARK_ITERATIONS) \
		$(if $(NATIVE_BENCHMARK_FLAGS),--flags "$(NATIVE_BENCHMARK_FLAGS)",) \
		$(if $(NATIVE_BENCHMARK_GENERATED_DIR),--generated-dir $(NATIVE_BENCHMARK_GENERATED_DIR),) \
		$(if $(NATIVE_BENCHMARK_SUMMARY),--summary $(NATIVE_BENCHMARK_SUMMARY),)
