#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$ROOT_DIR/imagedb}"
WORK_DIR="$(mktemp -d /tmp/cimagedb-cli-compat.XXXXXX)"
CASES_DIR="$WORK_DIR/cases"

trap 'rm -rf "$WORK_DIR"' EXIT
mkdir -p "$CASES_DIR"

run_case() {
    local name="$1"
    local expected_status="$2"
    local expected_stdout="$3"
    local expected_stderr="$4"
    local actual_status
    shift 4

    printf '%s' "$expected_stdout" > "$CASES_DIR/$name.stdout.expected"
    printf '%s' "$expected_stderr" > "$CASES_DIR/$name.stderr.expected"

    set +e
    (cd "$WORK_DIR" && "$BIN" "$@") \
        > "$CASES_DIR/$name.stdout.actual" \
        2> "$CASES_DIR/$name.stderr.actual"
    actual_status=$?
    set -e

    if [ "$actual_status" -ne "$expected_status" ]; then
        echo "cli-compat($name): expected exit $expected_status, got $actual_status" >&2
        exit 1
    fi
    if ! cmp -s "$CASES_DIR/$name.stdout.expected" \
              "$CASES_DIR/$name.stdout.actual"; then
        echo "cli-compat($name): stdout changed" >&2
        diff -u "$CASES_DIR/$name.stdout.expected" \
                "$CASES_DIR/$name.stdout.actual" >&2 || true
        exit 1
    fi
    if ! cmp -s "$CASES_DIR/$name.stderr.expected" \
              "$CASES_DIR/$name.stderr.actual"; then
        echo "cli-compat($name): stderr changed" >&2
        diff -u "$CASES_DIR/$name.stderr.expected" \
                "$CASES_DIR/$name.stderr.actual" >&2 || true
        exit 1
    fi
}

run_case help_extra 1 '' $'[ERROR] help takes no arguments\n' help extra
run_case init_extra 1 '' $'[ERROR] init takes no arguments\n' init extra
run_case import_usage 1 '' \
    $'[ERROR] Usage: ./imagedb import <file>\n' import
run_case invalid_id 1 '' $'[ERROR] Invalid ID: abc\n' info abc
run_case invalid_threshold 1 '' $'[ERROR] Invalid threshold: 300\n' \
    binary 1 300 out.ppm
run_case search_metric 1 '' \
    $'[ERROR] Unknown metric: cosine (use l1, l2, or intersection)\n' \
    search 1 2 --metric cosine
run_case search_similar_usage 1 '' \
    $'[ERROR] Usage: ./cimagedb search-similar <query.ppm> --topk K\n' \
    search-similar query.ppm --top 2
run_case invalid_angle 1 '' \
    $'[ERROR] Invalid angle: 361 (use 90, 180, or 270)\n' \
    rotate 1 361 out.ppm
run_case invalid_brightness 1 '' $'[ERROR] Invalid brightness: bad\n' \
    adjust 1 bad 1.0 out.ppm
run_case invalid_contrast 1 '' \
    $'[ERROR] Invalid contrast: nan (must be 0 < x <= 10)\n' \
    adjust 1 0 nan out.ppm
run_case query_field 1 '' \
    $'[ERROR] Unknown field: colour (use id, name, width, height, format, size)\n' \
    query colour eq red
run_case query_operator 1 '' $'[ERROR] Unknown operator: like\n' \
    query name like sample
run_case query_field_operator 1 '' \
    $'[ERROR] Operator \'contains\' not valid for field \'width\'\n' \
    query width contains 1
run_case query_number 1 '' \
    $'[ERROR] Invalid numeric value for field \'width\': 1x\n' \
    query width eq 1x
run_case hist_option 1 '' $'[ERROR] Unknown option: --raw\n' \
    hist-export 1 out.csv --raw
run_case search_export_metric 1 '' $'[ERROR] Unknown metric: cosine\n' \
    search-export 1 2 out.csv --metric cosine
run_case unknown 1 '' $'[ERROR] Unknown command: wat\n' wat

run_case init_success 0 $'Store initialized.\n' '' init
run_case import_success 0 \
    $'Import success.\nID: 1\nName: sample1.ppm\nWidth: 64\nHeight: 64\nPath: data/images/1.ppm\n' \
    '' import "$ROOT_DIR/samples/sample1.ppm"
run_case gray_success 0 $'Output written: gray.ppm\n' '' gray 1 gray.ppm
run_case search_empty 0 \
    $'Query image: 1\nMetric: intersection\nNo similar images found.\n' \
    '' search 1 3

echo "cli compatibility tests: PASS"
