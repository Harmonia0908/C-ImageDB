#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  C-ImageDB Visual Tests (Phase 7)"
echo "========================================="

echo ""
echo "[1/3] Building..."
make clean > /dev/null 2>&1 || true
make
echo "  Build: PASS"

echo ""
echo "[2/3] Preparing..."
rm -rf data output
mkdir -p output
bash scripts/generate_samples.sh > /dev/null 2>&1 || true

# Generate BMP
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
" 2>/dev/null || true

./imagedb init > /dev/null
./imagedb import samples/sample1.ppm > /dev/null
./imagedb import samples/sample2.ppm > /dev/null
./imagedb import samples/sample3.ppm > /dev/null
./imagedb import samples/sample_bmp1.bmp > /dev/null 2>&1 || true
echo "  Init + Import: PASS"

echo ""
echo "[3/3] Running visual tests..."

# hist-export
./imagedb hist-export 1 output/hist.csv > /dev/null
[ -f output/hist.csv ] && echo "  hist-export: PASS" || { echo "  hist-export: FAIL"; exit 1; }
head -1 output/hist.csv | grep -q "bin,r,g,b" && echo "  hist-export(header): PASS" || { echo "  hist-export(header): FAIL"; exit 1; }

# hist-export --normalized
./imagedb hist-export 1 output/hist_norm.csv --normalized > /dev/null
[ -f output/hist_norm.csv ] && echo "  hist-export(norm): PASS" || { echo "  hist-export(norm): FAIL"; exit 1; }
head -1 output/hist_norm.csv | grep -q "r_norm" && echo "  hist-export(norm header): PASS" || { echo "  hist-export(norm header): FAIL"; exit 1; }

# hist-image PPM
./imagedb hist-image 1 output/hist.ppm > /dev/null
[ -f output/hist.ppm ] && echo "  hist-image(PPM): PASS" || { echo "  hist-image(PPM): FAIL"; exit 1; }

# Verify hist-image dimensions (768x256)
python3 -c "
with open('output/hist.ppm','rb') as f:
    magic = f.readline().strip()
    dims = f.readline().strip()
    while dims.startswith(b'#'): dims = f.readline().strip()
    f.readline()
    w, h = map(int, dims.split())
    data = f.read()
    assert w == 768 and h == 256, f'Expected 768x256, got {w}x{h}'
    assert len(data) == 768*256*3
" && echo "  hist-image(768x256): PASS" || { echo "  hist-image(768x256): FAIL"; exit 1; }

# hist-image BMP
./imagedb hist-image 1 output/hist.bmp > /dev/null
[ -f output/hist.bmp ] && echo "  hist-image(BMP): PASS" || { echo "  hist-image(BMP): FAIL"; exit 1; }

# search-export
./imagedb search-export 1 3 output/search.csv > /dev/null
[ -f output/search.csv ] && echo "  search-export: PASS" || { echo "  search-export: FAIL"; exit 1; }
head -1 output/search.csv | grep -q "rank,id,name" && echo "  search-export(header): PASS" || { echo "  search-export(header): FAIL"; exit 1; }

# search-export with metric
./imagedb search-export 1 3 output/search_l1.csv --metric l1 > /dev/null
grep -q "l1" output/search_l1.csv && echo "  search-export(l1): PASS" || { echo "  search-export(l1): FAIL"; exit 1; }

# search-contact PPM
./imagedb search-contact 1 2 output/contact.ppm > /dev/null
[ -f output/contact.ppm ] && echo "  search-contact(PPM): PASS" || { echo "  search-contact(PPM): FAIL"; exit 1; }

# Verify contact sheet dimensions: 128 x (1 + k) wide = 128*3 = 384
python3 -c "
with open('output/contact.ppm','rb') as f:
    magic = f.readline().strip()
    dims = f.readline().strip()
    while dims.startswith(b'#'): dims = f.readline().strip()
    f.readline()
    w, h = map(int, dims.split())
    data = f.read()
    assert w == 384 and h == 128, f'Expected 384x128, got {w}x{h}'
    assert len(data) == 384*128*3
" && echo "  search-contact(384x128): PASS" || { echo "  search-contact(384x128): FAIL"; exit 1; }

# search-contact BMP
./imagedb search-contact 1 2 output/contact.bmp > /dev/null
[ -f output/contact.bmp ] && echo "  search-contact(BMP): PASS" || { echo "  search-contact(BMP): FAIL"; exit 1; }

# -- Error tests --
echo ""
echo "--- Error handling ---"
set +e

# Invalid ID
./imagedb hist-export 999 output/x.csv 2>&1 | grep -q "\[ERROR\]" && echo "  hist-export(999): PASS" || { echo "  hist-export(999): FAIL"; exit 1; }
./imagedb hist-image 999 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  hist-image(999): PASS" || { echo "  hist-image(999): FAIL"; exit 1; }
./imagedb search-export 999 3 output/x.csv 2>&1 | grep -q "\[ERROR\]" && echo "  search-export(999): PASS" || { echo "  search-export(999): FAIL"; exit 1; }
./imagedb search-contact 999 3 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  search-contact(999): PASS" || { echo "  search-contact(999): FAIL"; exit 1; }

# Invalid metric
./imagedb search-export 1 3 output/x.csv --metric cosine 2>&1 | grep -q "\[ERROR\]" && echo "  search-export(bad metric): PASS" || { echo "  search-export(bad metric): FAIL"; exit 1; }

# top_k = 0
./imagedb search-export 1 0 output/x.csv 2>&1 | grep -q "\[ERROR\]" && echo "  search-export(k=0): PASS" || { echo "  search-export(k=0): FAIL"; exit 1; }
./imagedb search-contact 1 0 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  search-contact(k=0): PASS" || { echo "  search-contact(k=0): FAIL"; exit 1; }

# Bad output path
./imagedb hist-export 1 /nonexistent/x.csv 2>&1 | grep -q "\[ERROR\]" && echo "  hist-export(bad path): PASS" || { echo "  hist-export(bad path): FAIL"; exit 1; }

set -e

echo ""
echo "========================================="
echo "  All visual tests passed."
echo "========================================="
