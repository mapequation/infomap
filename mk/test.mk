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
# as a race. Linux LLVM packages ship it in the LLVM lib dir, two levels above
# clang's resource dir; Homebrew does not ship it, see test-tsan below.
TSAN_ARCHER_LIB ?= $(shell p=$$($(TSAN_CXX) -print-resource-dir 2>/dev/null)/../../libarcher.so; [ -f "$$p" ] && echo "$$p")
FUZZ_BUILD_DIR ?= build/fuzz
FUZZ_SECONDS ?= 60
FUZZ_CXX ?= clang++
FUZZ_CC ?= clang
FUZZ_SEED_DIRS ?= test/fixtures/networks test/fixtures/networks/json examples/networks
# intake (the parsers), run (a full run with fuzzer-chosen options) or cluster
# (cluster data input on small fixed networks).
FUZZ_TARGET ?= intake
FUZZ_ARGS ?=
ifeq ($(FUZZ_TARGET),run)
FUZZ_CORPUS_DIR ?= $(FUZZ_BUILD_DIR)/corpus-run
FUZZ_ARTIFACT_PREFIX ?= $(FUZZ_BUILD_DIR)/run-
FUZZ_MAX_LEN ?= 8192
else ifeq ($(FUZZ_TARGET),cluster)
FUZZ_CORPUS_DIR ?= $(FUZZ_BUILD_DIR)/corpus-cluster
FUZZ_ARTIFACT_PREFIX ?= $(FUZZ_BUILD_DIR)/cluster-
FUZZ_MAX_LEN ?= 4096
else
FUZZ_CORPUS_DIR ?= $(FUZZ_BUILD_DIR)/corpus
FUZZ_ARTIFACT_PREFIX ?= $(FUZZ_BUILD_DIR)/
FUZZ_MAX_LEN ?= 1048576
endif
COVERAGE_BUILD_DIR ?= build/coverage
COVERAGE_CXX ?= clang++
COVERAGE_CC ?= clang
# The profile tools have to match the compiler's LLVM version, so ask it.
LLVM_PROFDATA ?= $(shell $(COVERAGE_CXX) -print-prog-name=llvm-profdata 2>/dev/null)
LLVM_COV ?= $(shell $(COVERAGE_CXX) -print-prog-name=llvm-cov 2>/dev/null)
# Seconds of fuzzing per fuzz target; 0 skips them (they need libFuzzer).
COVERAGE_FUZZ_SECONDS ?= 60
COVERAGE_FUZZ_ARGS ?= -fork=2 -ignore_crashes=1 -ignore_timeouts=1 -ignore_ooms=1
# 1 also runs test/python against an instrumented extension. The active Python
# needs setuptools and the test extras.
COVERAGE_PYTHON ?= 0
# 1 builds with OpenMP, which the parallel trial and move paths need.
COVERAGE_OPENMP ?= 0
COVERAGE_FLAGS := -fprofile-instr-generate -fcoverage-mapping
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

.PHONY: configure-cpp-dev tidy-native dev-cpp-check test-cpp-stream-policy test-native test-binding-options-freshness test-fast test-sanitizers test-tsan fuzz coverage bench-python bench-native

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
		-DINFOMAP_FP_TRAPS=ON \
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
# On Linux, install clang and the matching libomp-dev; Archer is found in the
# LLVM lib dir (pass LDFLAGS="-L<that dir>" if -lomp does not resolve). On
# macOS, Homebrew's llvm/libomp do not ship Archer. Build it from
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

# Build and briefly run a libFuzzer harness (Clang only; opt-in via
# INFOMAP_BUILD_FUZZERS, kept out of the default build/ctest).
# FUZZ_TARGET=intake (default) feeds bytes through buildNetworkFromInput, which
# routes JSON vs Pajek, so the single target covers both parsers.
# FUZZ_TARGET=run reads an option header and a network and runs Infomap with the
# flow invariants on; its seeds are the same fixtures behind a few option
# headers (test/fuzz/seed_run_corpus.sh).
# FUZZ_TARGET=cluster reads an option header and a .clu, .tree or .ftree file and
# runs it on one of a few fixed networks; its seeds are the cluster fixtures and
# Infomap's own output for those networks (test/fuzz/seed_cluster_corpus.sh).
# New inputs land in FUZZ_CORPUS_DIR,
# findings (crash-*/timeout-*/leak-*, prefixed run- or cluster- for those targets) under
# FUZZ_BUILD_DIR. Tune the run length with FUZZ_SECONDS (CI uses a larger budget
# than the default smoke) and pass extra libFuzzer flags in FUZZ_ARGS, e.g.
# FUZZ_ARGS=-fork=4.
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
	@"$(CMAKE)" --build $(FUZZ_BUILD_DIR) --target fuzz_$(FUZZ_TARGET) --parallel $(JOBS)
	@mkdir -p $(FUZZ_CORPUS_DIR)
ifeq ($(FUZZ_TARGET),run)
	@sh test/fuzz/seed_run_corpus.sh $(FUZZ_BUILD_DIR)/seeds-run $(foreach dir,$(FUZZ_SEED_DIRS),$(wildcard $(dir)/*.net $(dir)/*.json))
endif
ifeq ($(FUZZ_TARGET),cluster)
	@sh test/fuzz/seed_cluster_corpus.sh $(FUZZ_BUILD_DIR)/seeds-cluster test/fuzz/cluster_seeds/* $(wildcard test/fixtures/clusters/*)
endif
	@ASAN_OPTIONS=strict_string_checks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
		$(FUZZ_BUILD_DIR)/fuzz_$(FUZZ_TARGET) \
			-max_total_time=$(FUZZ_SECONDS) -max_len=$(FUZZ_MAX_LEN) -timeout=10 \
			-dict=test/fuzz/$(if $(filter cluster,$(FUZZ_TARGET)),cluster,intake).dict \
			-artifact_prefix=$(FUZZ_ARTIFACT_PREFIX) -print_final_stats=1 $(FUZZ_ARGS) \
			$(FUZZ_CORPUS_DIR) $(if $(filter run cluster,$(FUZZ_TARGET)),$(FUZZ_BUILD_DIR)/seeds-$(FUZZ_TARGET),$(FUZZ_SEED_DIRS))

# A one-off coverage map (#1171): which branches no test, CLI check, Python
# test or fuzz input reaches. Not a gate. Clang source-based coverage over a
# debug build, without OpenMP unless COVERAGE_OPENMP=1 (the fuzz targets want
# it off, so pair it with COVERAGE_FUZZ_SECONDS=0). The fuzz targets run for
# COVERAGE_FUZZ_SECONDS each from the same seeds as make fuzz; with them on,
# the build carries their ASan/UBSan too, so the tests run under the
# sanitizers. Failing tests do not stop the report. Writes
# $(COVERAGE_BUILD_DIR)/report/index.html and summary.txt (per file in src/).
coverage:
	@generator_args=""; \
	if [ -n "$(CMAKE_GENERATOR)" ]; then generator_args="-G $(CMAKE_GENERATOR)"; fi; \
	"$(CMAKE)" -S . -B $(COVERAGE_BUILD_DIR) $$generator_args \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_CXX_COMPILER=$(COVERAGE_CXX) \
		-DCMAKE_C_COMPILER=$(COVERAGE_CC) \
		-DINFOMAP_MODE=debug \
		-DINFOMAP_USE_OPENMP=$(if $(filter 1,$(COVERAGE_OPENMP)),ON,OFF) \
		-DINFOMAP_ENABLE_COVERAGE=ON \
		-DINFOMAP_BUILD_FUZZERS=$(if $(filter-out 0,$(COVERAGE_FUZZ_SECONDS)),ON,OFF) \
		$(TEST_CMAKE_ARGS)
	@"$(CMAKE)" --build $(COVERAGE_BUILD_DIR) --target $(CMAKE_TEST_TARGET) \
		$(if $(filter-out 0,$(COVERAGE_FUZZ_SECONDS)),fuzz_intake fuzz_run fuzz_cluster,) --parallel $(JOBS)
	@rm -rf $(COVERAGE_BUILD_DIR)/profiles $(COVERAGE_BUILD_DIR)/report
	@mkdir -p $(COVERAGE_BUILD_DIR)/profiles
	@LLVM_PROFILE_FILE="$(CURDIR)/$(COVERAGE_BUILD_DIR)/profiles/ctest-%p.profraw" \
		"$(CTEST)" --test-dir $(COVERAGE_BUILD_DIR) --output-on-failure $(CTEST_ARGS) \
		|| printf "%s\n" "coverage: some tests failed; the report covers what ran."
ifneq ($(COVERAGE_FUZZ_SECONDS),0)
	@sh test/fuzz/seed_run_corpus.sh $(COVERAGE_BUILD_DIR)/seeds-run $(foreach dir,$(FUZZ_SEED_DIRS),$(wildcard $(dir)/*.net $(dir)/*.json))
	@sh test/fuzz/seed_cluster_corpus.sh $(COVERAGE_BUILD_DIR)/seeds-cluster test/fuzz/cluster_seeds/* $(wildcard test/fixtures/clusters/*)
	@for target in intake run cluster; do \
		case $$target in \
			intake) seeds="$(FUZZ_SEED_DIRS)"; dict=intake; max_len=1048576 ;; \
			run) seeds=$(COVERAGE_BUILD_DIR)/seeds-run; dict=intake; max_len=8192 ;; \
			cluster) seeds=$(COVERAGE_BUILD_DIR)/seeds-cluster; dict=cluster; max_len=4096 ;; \
		esac; \
		mkdir -p $(COVERAGE_BUILD_DIR)/corpus-$$target $(COVERAGE_BUILD_DIR)/artifacts; \
		printf "%s\n" "coverage: fuzzing $$target for $(COVERAGE_FUZZ_SECONDS)s"; \
		LLVM_PROFILE_FILE="$(CURDIR)/$(COVERAGE_BUILD_DIR)/profiles/fuzz-$$target-%p.profraw" \
			$(COVERAGE_BUILD_DIR)/fuzz_$$target -max_total_time=$(COVERAGE_FUZZ_SECONDS) \
			-max_len=$$max_len -timeout=10 -dict=test/fuzz/$$dict.dict \
			-artifact_prefix=$(COVERAGE_BUILD_DIR)/artifacts/$$target- $(COVERAGE_FUZZ_ARGS) \
			$(COVERAGE_BUILD_DIR)/corpus-$$target $$seeds > $(COVERAGE_BUILD_DIR)/fuzz-$$target.log 2>&1 \
			|| printf "%s\n" "coverage: fuzz_$$target exited non-zero, see $(COVERAGE_BUILD_DIR)/fuzz-$$target.log"; \
	done
endif
ifeq ($(COVERAGE_PYTHON),1)
	@rm -rf $(COVERAGE_BUILD_DIR)/python && mkdir -p $(COVERAGE_BUILD_DIR)/python
	@cp -R interfaces/python/src/infomap $(COVERAGE_BUILD_DIR)/python/
	@rm -f $(COVERAGE_BUILD_DIR)/python/infomap/_infomap*.so
	@CC="$(COVERAGE_CC)" CXX="$(COVERAGE_CXX)" MODE=debug OPENMP=$(COVERAGE_OPENMP) INFOMAP_BUILD_JOBS="$(JOBS)" \
		CXXFLAGS="$(COVERAGE_FLAGS)" LDFLAGS="-fprofile-instr-generate" \
		$(PYTHON) setup.py -q build_ext --build-lib $(COVERAGE_BUILD_DIR)/python --build-temp $(COVERAGE_BUILD_DIR)/python-obj
	@PYTHONPATH="$(CURDIR)/$(COVERAGE_BUILD_DIR)/python" \
		LLVM_PROFILE_FILE="$(CURDIR)/$(COVERAGE_BUILD_DIR)/profiles/python-%p.profraw" \
		$(PYTEST) -q -p no:cacheprovider $(PYTEST_ARGS) $(PYTHON_TEST_DIR) \
		|| printf "%s\n" "coverage: some Python tests failed; the report covers what ran."
endif
	@"$(LLVM_PROFDATA)" merge -sparse $(COVERAGE_BUILD_DIR)/profiles/*.profraw -o $(COVERAGE_BUILD_DIR)/coverage.profdata
	@objects="$(COVERAGE_BUILD_DIR)/Infomap"; \
	for f in $(COVERAGE_BUILD_DIR)/infomap_cpp_*_tests \
		$(if $(filter-out 0,$(COVERAGE_FUZZ_SECONDS)),$(COVERAGE_BUILD_DIR)/fuzz_intake $(COVERAGE_BUILD_DIR)/fuzz_run $(COVERAGE_BUILD_DIR)/fuzz_cluster) \
		$(if $(filter 1,$(COVERAGE_PYTHON)),$(COVERAGE_BUILD_DIR)/python/infomap/_infomap*.so); do \
		if [ -f "$$f" ]; then objects="$$objects -object $$f"; fi; \
	done; \
	cov_args="$$objects -instr-profile=$(COVERAGE_BUILD_DIR)/coverage.profdata"; \
	"$(LLVM_COV)" show $$cov_args -format=html -show-branches=count -output-dir=$(COVERAGE_BUILD_DIR)/report $(CURDIR)/src && \
	"$(LLVM_COV)" report $$cov_args -show-branch-summary $(CURDIR)/src > $(COVERAGE_BUILD_DIR)/summary.txt && \
	tail -n 1 $(COVERAGE_BUILD_DIR)/summary.txt && \
	printf "%s\n" "coverage: $(COVERAGE_BUILD_DIR)/report/index.html and $(COVERAGE_BUILD_DIR)/summary.txt"

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
