#!/usr/bin/env bash
# Usage: bash scripts/run_host_revalidation.sh [OUTPUT_DIR]
# Exact per-vertex checks and unbiased repeated end-to-end timings.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:-validation/host_round5_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
{
  date -Is; uname -a; lscpu; free -h; g++ --version | head -1
  echo '--cg cpu--'; cat /sys/fs/cgroup/cpu.max 2>/dev/null || true
  echo '--cg mem--'; cat /sys/fs/cgroup/memory.max 2>/dev/null || true
  echo '--cpu affinity--'; taskset -pc $$ 2>/dev/null || true
  echo '--compile--'; echo '-std=c++17 -O3 -pthread -MMD -MP -Wall -Wextra'
} > "$OUT/host_environment.txt"
sha256sum dataset/*.txt > "$OUT/dataset_sha256.txt"
sha256sum verified/hgd_optimized.hpp scc_all.cpp scc_reverse_csr_plus.cpp > "$OUT/source_sha256.txt"
# Paper reports GCC 11.1.0 with -O3. Record compiler version above; do not
# silently claim exact hardware/compiler equivalence if they are different.
make -j2 core CXXFLAGS='-std=c++17 -O3 -Wall -Wextra -MMD -MP -pthread'
make test-reverse
make test-sparse-thresholds
python3 verified/tests/benchmark_validated.py --datasets SC,HC,CP,CH \
  --methods scc_cached_plus,scc_reverse_csr_plus,dd_cached,dd_reverse_csr \
  --repeats 5 --timeout 180 --out "$OUT/small"
python3 verified/tests/benchmark_validated.py --datasets SB,HB \
  --methods scc_cached_plus,scc_reverse_csr_plus \
  --repeats 5 --timeout 300 --out "$OUT/large"
echo "PASS; inspect output hashes, censored trials and timings in $OUT"
