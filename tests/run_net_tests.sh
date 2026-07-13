#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  C-ImageDB Network Integration Tests"
echo "========================================="

echo ""
echo "[1/3] Building server..."
make server
echo "  Build: PASS"

echo ""
echo "[2/3] Preparing data..."
rm -rf data output && mkdir -p output
./imagedb init > /dev/null
./imagedb import samples/sample1.ppm > /dev/null
./imagedb import samples/sample2.ppm > /dev/null

echo ""
echo "[3/3] Running protocol tests on a dynamic loopback port..."
python3 tests/net_protocol_test.py

echo ""
echo "========================================="
echo "  All network tests passed."
echo "========================================="
