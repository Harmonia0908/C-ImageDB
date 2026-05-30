#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  C-ImageDB Database Tests (Phase 5)"
echo "========================================="

# -- Clean build --
echo ""
echo "[1/4] Building..."
make clean > /dev/null 2>&1 || true
make
echo "  Build: PASS"

# -- Prepare --
echo ""
echo "[2/4] Preparing test environment..."
rm -rf data output
mkdir -p output

# Generate samples if needed
if [ ! -f samples/sample1.ppm ]; then
    bash scripts/generate_samples.sh > /dev/null 2>&1
fi

# Generate BMP if needed
if [ ! -f samples/sample_bmp1.bmp ]; then
    python3 -c "
import struct
def wbmp(p,w,h,r,g,b):
    rr=w*3; rp=(rr+3)&~3; off=54; fs=off+rp*h
    with open(p,'wb') as f:
        f.write(struct.pack('<HIHHI',0x4D42,fs,0,0,off))
        f.write(struct.pack('<IiiHHIIiiII',40,w,h,1,24,0,rp*h,0,0,0,0))
        for _ in range(h):
            for _ in range(w): f.write(struct.pack('BBB',b,g,r))
            f.write(b'\x00'*(rp-rr))
wbmp('samples/sample_bmp1.bmp',64,64,255,0,0)
wbmp('samples/sample_bmp2.bmp',128,128,0,255,0)
" 2>/dev/null || true
fi

./imagedb init > /dev/null
./imagedb import samples/sample1.ppm > /dev/null
./imagedb import samples/sample2.ppm > /dev/null
./imagedb import samples/sample3.ppm > /dev/null
./imagedb import samples/sample_bmp1.bmp > /dev/null 2>&1 || true
echo "  Init + Import: PASS"

# -- Find-name tests --
echo ""
echo "[3/4] Testing find-name..."

./imagedb find-name sample | grep -q "sample1" && echo "  find-name(sample): PASS" || { echo "  find-name(sample): FAIL"; exit 1; }
./imagedb find-name cat | grep -q "No matched" && echo "  find-name(no-match): PASS" || { echo "  find-name(no-match): FAIL"; exit 1; }

# Delete a record and verify find-name excludes it (BMP import may have failed, so check ID 3)
./imagedb info 3 > /dev/null 2>&1 && ./imagedb delete 3 > /dev/null 2>&1 || true
./imagedb find-name sample 2>&1 | grep -q "3" && { echo "  find-name(deleted): FAIL"; exit 1; } || echo "  find-name(deleted): PASS"

# -- Query tests --
echo ""
echo "--- Testing query ---"

./imagedb query width gt 0 | grep -q "sample1" && echo "  query(width gt 0): PASS" || { echo "  query(width gt 0): FAIL"; exit 1; }
./imagedb query format eq PPM | grep -q "sample1" && echo "  query(format eq PPM): PASS" || { echo "  query(format eq PPM): FAIL"; exit 1; }
./imagedb query name contains sample | grep -q "sample1" && echo "  query(name contains): PASS" || { echo "  query(name contains): FAIL"; exit 1; }
./imagedb query width le 128 | grep -q "sample1" && echo "  query(width le): PASS" || { echo "  query(width le): FAIL"; exit 1; }

# Invalid field/op
set +e
./imagedb query foo eq bar 2>&1 | grep -q "\[ERROR\]" && echo "  query(bad field): PASS" || { echo "  query(bad field): FAIL"; exit 1; }
./imagedb query width contains 100 2>&1 | grep -q "\[ERROR\]" && echo "  query(bad op): PASS" || { echo "  query(bad op): FAIL"; exit 1; }
./imagedb query width eq abc 2>&1 | grep -q "\[ERROR\]" && echo "  query(non-numeric): PASS" || { echo "  query(non-numeric): FAIL"; exit 1; }
set -e

# -- Stats test --
echo ""
echo "--- Testing stats ---"

./imagedb stats | grep -q "Total records" && echo "  stats: PASS" || { echo "  stats: FAIL"; exit 1; }

# Stats on empty DB should not crash
rm -rf data && ./imagedb init > /dev/null 2>&1
./imagedb stats | grep -q "Total records" && echo "  stats(empty): PASS" || { echo "  stats(empty): FAIL"; exit 1; }

# -- Export test --
echo ""
echo "--- Testing export ---"

# Rebuild data for export
rm -rf data output && mkdir -p output
./imagedb init > /dev/null
./imagedb import samples/sample1.ppm > /dev/null
./imagedb import samples/sample2.ppm > /dev/null

./imagedb export output/metadata.csv > /dev/null
[ -f output/metadata.csv ] && echo "  export(file): PASS" || { echo "  export(file): FAIL"; exit 1; }

head -1 output/metadata.csv | grep -q "id,name,path" && echo "  export(header): PASS" || { echo "  export(header): FAIL"; exit 1; }

# Export to non-existent directory
set +e
./imagedb export /nonexistent/out.csv 2>&1 | grep -q "\[ERROR\]" && echo "  export(bad path): PASS" || { echo "  export(bad path): FAIL"; exit 1; }
set -e

# -- Compact test --
echo ""
echo "[4/4] Testing compact..."

rm -rf data output && mkdir -p output
./imagedb init > /dev/null
./imagedb import samples/sample1.ppm > /dev/null
./imagedb import samples/sample2.ppm > /dev/null
./imagedb import samples/sample3.ppm > /dev/null

# Check initial state
./imagedb stats | grep -q "Active records:   3" && echo "  pre-compact: PASS" || { echo "  pre-compact: FAIL"; exit 1; }

# Delete one
./imagedb delete 2 > /dev/null

# Compact
./imagedb compact | grep -q "Compact complete" && echo "  compact(run): PASS" || { echo "  compact(run): FAIL"; exit 1; }

# After compact: active=2, deleted=0, ID 2 gone
./imagedb stats | grep -q "Active records:   2" && echo "  compact(count): PASS" || { echo "  compact(count): FAIL"; exit 1; }

# Deleted record should not appear
./imagedb info 2 2>&1 | grep -q "\[ERROR\]" && echo "  compact(gone): PASS" || { echo "  compact(gone): FAIL"; exit 1; }

# find-name should not return deleted record (ID 2 should be gone)
./imagedb find-name sample2 | grep -q "No matched" && echo "  compact(find-name): PASS" || { echo "  compact(find-name): FAIL"; exit 1; }

# search should not include deleted (ID 2 no longer exists)
./imagedb search 1 3 2>&1 | grep -q "id=2" && { echo "  compact(search): FAIL"; exit 1; } || echo "  compact(search): PASS"

# query should not include deleted
./imagedb query id eq 2 2>&1 | grep -q "No matched" && echo "  compact(query): PASS" || { echo "  compact(query): FAIL"; exit 1; }

echo ""
echo "========================================="
echo "  All database tests passed."
echo "========================================="
