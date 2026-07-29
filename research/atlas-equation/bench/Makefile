# Standalone benchmark for the atlas equation's parallel top-level search.
# Not wired into the repository build; see ../README.md.

CXX ?= g++
CXXFLAGS ?= -O3 -march=native -std=c++17 -Wall -Wextra
OPENMP ?= -fopenmp

parallel_bench: parallel_bench.cpp
	$(CXX) $(CXXFLAGS) $(OPENMP) -o $@ $<

.PHONY: run clean
run: parallel_bench
	./parallel_bench --blocks 200 --block-size 1000 --threads 1,2,4

clean:
	rm -f parallel_bench
