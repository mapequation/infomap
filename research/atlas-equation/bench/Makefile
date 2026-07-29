# Standalone benchmark for the atlas equation's parallel top-level search.
# Not wired into the repository build; see ../README.md.

# `?=` cannot express "unless the user set it": make predefines CXX, so its
# builtin value would always win. Test the origin instead.
ifeq ($(origin CXX),default)
CXX = g++
endif

CXXFLAGS ?= -O3 -march=native -std=c++17 -Wall -Wextra

# Apple clang ships without -fopenmp; Homebrew libomp supplies it out of tree.
# Probe the compiler and fall back to a serial build rather than failing, so
# `make` works on a stock macOS checkout. Override with OPENMP= to force.
ifeq ($(origin OPENMP),undefined)
OPENMP := $(shell printf 'int main(){}' > /tmp/.atlas_omp_probe.cpp 2>/dev/null && \
	  ($(CXX) -fopenmp -x c++ /tmp/.atlas_omp_probe.cpp -o /dev/null >/dev/null 2>&1 && echo -fopenmp) || \
	  ($(CXX) -Xpreprocessor -fopenmp -lomp -x c++ /tmp/.atlas_omp_probe.cpp -o /dev/null >/dev/null 2>&1 && \
	   echo "-Xpreprocessor -fopenmp -lomp"))
endif

parallel_bench: parallel_bench.cpp
	@test -n "$(OPENMP)" || echo "note: no OpenMP support detected; building single-threaded"
	$(CXX) $(CXXFLAGS) $(OPENMP) -o $@ $<

.PHONY: run clean
run: parallel_bench
	./parallel_bench --blocks 200 --block-size 1000 --threads 1,2,4

clean:
	rm -f parallel_bench
