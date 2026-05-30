#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  C-ImageDB Benchmark Test"
echo "========================================="

rm -rf bench/results bench/tmp

echo ""
echo "[1/2] Running reduced benchmark..."
BENCH_SIZES="3" BENCH_REPEATS=1 BENCH_TOPK=2 bash bench/benchmark.sh > /tmp/cimagedb_benchmark_test.log
echo "  benchmark(run): PASS"

echo ""
echo "[2/2] Checking benchmark.csv..."
[ -f bench/results/benchmark.csv ] &&
    echo "  csv(file): PASS" || { echo "  csv(file): FAIL"; exit 1; }

head -1 bench/results/benchmark.csv | grep -q "operation,dataset_size,topk,elapsed_ms" &&
    echo "  csv(header): PASS" || { echo "  csv(header): FAIL"; exit 1; }

grep -q "^search_similar,3,2," bench/results/benchmark.csv &&
    echo "  csv(search_similar): PASS" || { echo "  csv(search_similar): FAIL"; exit 1; }

grep -q "^histogram,3,0," bench/results/benchmark.csv &&
    echo "  csv(histogram): PASS" || { echo "  csv(histogram): FAIL"; exit 1; }

echo ""
echo "========================================="
echo "  Benchmark test passed."
echo "========================================="
