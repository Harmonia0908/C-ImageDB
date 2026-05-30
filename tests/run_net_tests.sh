#!/usr/bin/env bash
# Optional: TCP server tests. Requires 'make server' first and 'nc' installed.
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

if ! command -v nc &> /dev/null; then
    echo "SKIP: 'nc' (netcat) not found. Install netcat to run network tests."
    exit 0
fi

echo "========================================="
echo "  C-ImageDB Network Tests (optional)"
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
echo "[3/3] Running network tests..."

./imagedb-server 9002 &
SERVER_PID=$!
sleep 1

printf 'LIST\nINFO 1\nSEARCH 1 2\nQUIT\n' | nc -w 2 127.0.0.1 9002 > /tmp/server_out.txt 2>&1 || true

grep -q "sample1" /tmp/server_out.txt && echo "  tcp(LIST): PASS" || { echo "  tcp(LIST): FAIL"; kill $SERVER_PID 2>/dev/null; exit 1; }
grep -q "Width:" /tmp/server_out.txt && echo "  tcp(INFO): PASS" || { echo "  tcp(INFO): FAIL"; kill $SERVER_PID 2>/dev/null; exit 1; }
grep -q "intersection" /tmp/server_out.txt && echo "  tcp(SEARCH): PASS" || { echo "  tcp(SEARCH): FAIL"; kill $SERVER_PID 2>/dev/null; exit 1; }
grep -q "BYE" /tmp/server_out.txt && echo "  tcp(QUIT): PASS" || { echo "  tcp(QUIT): FAIL"; kill $SERVER_PID 2>/dev/null; exit 1; }

kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

echo ""
echo "========================================="
echo "  All network tests passed."
echo "========================================="
