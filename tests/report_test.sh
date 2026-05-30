#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  C-ImageDB HTML Report Tests"
echo "========================================="

echo ""
echo "[1/3] Running demo..."
bash scripts/demo.sh > /tmp/cimagedb_demo_report_test.log
echo "  demo: PASS"

echo ""
echo "[2/3] Checking report content..."
[ -f output/index.html ] &&
    echo "  report(file): PASS" || { echo "  report(file): FAIL"; exit 1; }

grep -q "C-ImageDB Demo Report" output/index.html &&
    echo "  report(title): PASS" || { echo "  report(title): FAIL"; exit 1; }

grep -q "demo_metadata.csv" output/index.html &&
    echo "  report(metadata csv): PASS" || { echo "  report(metadata csv): FAIL"; exit 1; }

grep -q "Original Images" output/index.html &&
    echo "  report(original images): PASS" || { echo "  report(original images): FAIL"; exit 1; }

grep -q "Top-K Similar Search" output/index.html &&
    echo "  report(topk section): PASS" || { echo "  report(topk section): FAIL"; exit 1; }

grep -q "distance" output/index.html &&
    echo "  report(distance): PASS" || { echo "  report(distance): FAIL"; exit 1; }

grep -q "data/images/2.ppm" output/index.html &&
    echo "  report(topk result): PASS" || { echo "  report(topk result): FAIL"; exit 1; }

echo ""
echo "[3/3] Checking error handling..."
set +e
./imagedb report /tmp/cimagedb_missing_output_dir /tmp/cimagedb_missing_report.html 2>&1 |
    grep -q "\[ERROR\]" &&
    echo "  report(missing output): PASS" || { echo "  report(missing output): FAIL"; exit 1; }
set -e

echo ""
echo "========================================="
echo "  HTML report tests passed."
echo "========================================="
