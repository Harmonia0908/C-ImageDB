#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

SIZES="${BENCH_SIZES:-100 500 1000}"
REPEATS="${BENCH_REPEATS:-5}"
TOPK="${BENCH_TOPK:-5}"
TMP_ROOT="bench/tmp"
RUN_DIR="$TMP_ROOT/run_$$"
IMAGE_DIR="$RUN_DIR/images"
BACKUP_DIR="$TMP_ROOT/backup_$$"
RESULT_DIR="bench/results"
RESULT_CSV="$RESULT_DIR/benchmark.csv"
OUTPUT_BENCH="output/bench"
HAD_DATA=0
HAD_OUTPUT=0
CURRENT_SIZE=0

cleanup() {
    rm -rf data
    if [ "$HAD_DATA" -eq 1 ] && [ -d "$BACKUP_DIR/data" ]; then
        mv "$BACKUP_DIR/data" data
    fi

    rm -rf "$OUTPUT_BENCH"
    if [ "$HAD_OUTPUT" -eq 0 ]; then
        rmdir output 2>/dev/null || true
    fi

    rm -rf "$RUN_DIR" "$BACKUP_DIR"
}
trap cleanup EXIT

validate_positive_int() {
    case "$1" in
        ''|*[!0-9]*) return 1 ;;
        *) [ "$1" -gt 0 ] ;;
    esac
}

if ! validate_positive_int "$REPEATS"; then
    echo "[ERROR] BENCH_REPEATS must be a positive integer" >&2
    exit 1
fi

if ! validate_positive_int "$TOPK"; then
    echo "[ERROR] BENCH_TOPK must be a positive integer" >&2
    exit 1
fi

SORTED_SIZES="$(printf "%s\n" $SIZES | sort -n)"
MAX_SIZE="$(printf "%s\n" $SORTED_SIZES | tail -1)"
if ! validate_positive_int "$MAX_SIZE"; then
    echo "[ERROR] BENCH_SIZES must contain positive integers" >&2
    exit 1
fi

mkdir -p "$IMAGE_DIR" "$BACKUP_DIR" "$RESULT_DIR"

if [ -d data ]; then
    HAD_DATA=1
    mv data "$BACKUP_DIR/data"
fi

if [ -d output ]; then
    HAD_OUTPUT=1
fi

mkdir -p "$OUTPUT_BENCH"

echo "========================================="
echo "  C-ImageDB Benchmark"
echo "========================================="
echo "sizes:   $SIZES"
echo "repeats: $REPEATS"
echo "topk:    $TOPK"
echo ""

echo "[1/4] Building..."
make clean > /dev/null 2>&1 || true
make > /dev/null
echo "  build: PASS"

echo "[2/4] Generating synthetic PPM dataset..."
python3 - "$IMAGE_DIR" "$MAX_SIZE" <<'PY'
import os
import sys

out_dir = sys.argv[1]
count = int(sys.argv[2])
os.makedirs(out_dir, exist_ok=True)

def write_ppm(path, idx):
    w, h = 16, 16
    with open(path, "wb") as f:
        f.write(f"P6\n{w} {h}\n255\n".encode("ascii"))
        for y in range(h):
            for x in range(w):
                r = (idx * 37 + x * 11 + y * 3) % 256
                g = (idx * 67 + x * 5 + y * 17) % 256
                b = (idx * 97 + x * 13 + y * 7) % 256
                f.write(bytes((r, g, b)))

for i in range(1, count + 1):
    write_ppm(os.path.join(out_dir, f"bench_{i:04d}.ppm"), i)
write_ppm(os.path.join(out_dir, "query.ppm"), count + 1)
PY
echo "  generated: $MAX_SIZE images + query"

echo "[3/4] Running benchmark..."
printf "operation,dataset_size,topk,elapsed_ms\n" > "$RESULT_CSV"

rm -rf data
./imagedb init > /dev/null

elapsed_ms() {
    python3 - "$@" <<'PY'
import subprocess
import sys
import time

cmd = sys.argv[1:]
start = time.perf_counter()
result = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
elapsed = time.perf_counter() - start
if result.returncode != 0:
    sys.exit(result.returncode)
print(int(round(elapsed * 1000)))
PY
}

average_command_ms() {
    local sum=0
    local i
    local value

    for i in $(seq 1 "$REPEATS"); do
        value="$(elapsed_ms "$@")"
        sum=$((sum + value))
    done

    echo $((sum / REPEATS))
}

append_result() {
    local operation="$1"
    local dataset_size="$2"
    local topk="$3"
    local elapsed="$4"
    printf "%s,%s,%s,%s\n" "$operation" "$dataset_size" "$topk" "$elapsed" >> "$RESULT_CSV"
}

import_until() {
    local target="$1"
    local i

    if [ "$target" -lt "$CURRENT_SIZE" ]; then
        echo "[ERROR] Dataset sizes must be sorted ascending after normalization" >&2
        exit 1
    fi

    if [ "$target" -eq "$CURRENT_SIZE" ]; then
        return
    fi

    for i in $(seq $((CURRENT_SIZE + 1)) "$target"); do
        ./imagedb import "$IMAGE_DIR/bench_$(printf "%04d" "$i").ppm" > /dev/null
    done
    CURRENT_SIZE="$target"
}

for size in $SORTED_SIZES; do
    if ! validate_positive_int "$size"; then
        echo "[ERROR] Invalid dataset size: $size" >&2
        exit 1
    fi

    echo "  dataset size: $size"
    import_until "$size"

    gray_ms="$(average_command_ms ./imagedb gray 1 "$OUTPUT_BENCH/gray_${size}.ppm")"
    append_result "grayscale" "$size" 0 "$gray_ms"

    edge_ms="$(average_command_ms ./imagedb edge 1 "$OUTPUT_BENCH/edge_${size}.ppm")"
    append_result "edge" "$size" 0 "$edge_ms"

    hist_ms="$(average_command_ms ./imagedb hist 1)"
    append_result "histogram" "$size" 0 "$hist_ms"

    search_ms="$(average_command_ms ./cimagedb search-similar "$IMAGE_DIR/query.ppm" --topk "$TOPK")"
    append_result "search_similar" "$size" "$TOPK" "$search_ms"
done

echo "[4/4] Results written:"
echo "  $RESULT_CSV"
echo ""
cat "$RESULT_CSV"
