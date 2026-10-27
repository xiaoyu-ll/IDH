CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -MMD -MP -pthread

SEARCH := path flow flow+
DECOMPOSITION := hgd_flow hgd_reverse
DYNAMIC := insert delete hgd_stream hgd_stream_reverse
AUXILIARY := casestudya casestudyb paper_dynamic_bench

.PHONY: all paper search decomposition dynamic auxiliary test test-quick \
	test-reverse test-dynamic-boundaries test-sandwich test-scale clean

all: paper

paper: search decomposition dynamic build/hgd_sandwich \
	build/paper_baselines build/paper_density

search: $(addprefix build/,$(SEARCH))
decomposition: $(addprefix build/,$(DECOMPOSITION))
dynamic: $(addprefix build/,$(DYNAMIC))
auxiliary: $(addprefix build/,$(AUXILIARY))

build:
	mkdir -p build

build/%: %.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/hgd_sandwich: verified/hgd_sandwich_cli.cpp verified/hgd_sandwich.hpp \
	verified/hgd_optimized.hpp verified/hgd_exact.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/paper_baselines: verified/paper_baselines.cpp verified/hgd_optimized.hpp \
	verified/hgd_exact.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/paper_density: verified/paper_density.cpp verified/hgd_optimized.hpp \
	verified/hgd_exact.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/threshold_sequence_test: verified/tests/threshold_sequence.cpp \
	verified/hgd_optimized.hpp verified/hgd_exact.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/test_reverse_reach: verified/tests/test_reverse_reach.cpp \
	verified/hgd_optimized.hpp verified/hgd_exact.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/test_reverse_reach_csr: verified/tests/test_reverse_reach.cpp \
	verified/hgd_optimized.hpp verified/hgd_exact.cpp | build
	$(CXX) $(CXXFLAGS) -DHGD_REVERSE_CSR $< -o $@

build/test_dynamic_boundaries: verified/tests/test_dynamic_boundaries.cpp \
	verified/hgd_optimized.hpp verified/hgd_exact.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/test_sandwich: verified/tests/test_sandwich.cpp verified/hgd_sandwich.hpp \
	verified/hgd_optimized.hpp verified/hgd_exact.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/test_scale_boundaries: verified/tests/test_scale_boundaries.cpp \
	verified/hgd_optimized.hpp verified/hgd_exact.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/test_scale_boundaries_reverse: verified/tests/test_scale_boundaries.cpp \
	verified/hgd_optimized.hpp verified/hgd_exact.cpp | build
	$(CXX) $(CXXFLAGS) -DHGD_REVERSE_REACH -DHGD_REVERSE_CSR \
		-DHGD_SPARSE_RESIDUAL_SYNC $< -o $@

test-quick: search decomposition dynamic build/threshold_sequence_test
	python3 verified/tests/test_scc_exhaustive.py
	python3 verified/tests/test_threshold_sequences.py
	python3 verified/tests/test_optimized_independent.py 4

test-reverse: build/test_reverse_reach build/test_reverse_reach_csr \
	build/hgd_stream_reverse
	./build/test_reverse_reach 7000
	./build/test_reverse_reach_csr 7000
	python3 verified/tests/test_round5_oracle.py 22

test-dynamic-boundaries: build/test_dynamic_boundaries build/hgd_stream \
	build/hgd_stream_reverse
	python3 verified/tests/test_dynamic_boundaries.py

test-sandwich: build/test_sandwich
	./build/test_sandwich

test-scale: build/test_scale_boundaries build/test_scale_boundaries_reverse
	./build/test_scale_boundaries
	./build/test_scale_boundaries_reverse

test: test-quick test-reverse test-dynamic-boundaries test-sandwich

clean:
	rm -rf build

-include $(wildcard build/*.d)
