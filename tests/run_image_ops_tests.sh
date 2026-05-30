#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  C-ImageDB Image Ops Tests (Phase 6)"
echo "========================================="

# -- Clean build --
echo ""
echo "[1/3] Building..."
make clean > /dev/null 2>&1 || true
make
echo "  Build: PASS"

# -- Prepare --
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
echo "  Init + Import: PASS"

# -- Tests --
echo ""
echo "[3/3] Running image op tests..."

# equalize
./imagedb equalize 1 output/eq.ppm > /dev/null
[ -f output/eq.ppm ] && echo "  equalize: PASS" || { echo "  equalize: FAIL"; exit 1; }

# equalize to BMP
./imagedb equalize 1 output/eq.bmp > /dev/null
[ -f output/eq.bmp ] && echo "  equalize(BMP): PASS" || { echo "  equalize(BMP): FAIL"; exit 1; }

# median 3
./imagedb median 1 3 output/med3.ppm > /dev/null
[ -f output/med3.ppm ] && echo "  median(3): PASS" || { echo "  median(3): FAIL"; exit 1; }

# median 5
./imagedb median 1 5 output/med5.ppm > /dev/null
[ -f output/med5.ppm ] && echo "  median(5): PASS" || { echo "  median(5): FAIL"; exit 1; }

# gaussian
./imagedb gaussian 1 output/gauss.ppm > /dev/null
[ -f output/gauss.ppm ] && echo "  gaussian: PASS" || { echo "  gaussian: FAIL"; exit 1; }

# adjust
./imagedb adjust 1 10 1.2 output/adj.ppm > /dev/null
[ -f output/adj.ppm ] && echo "  adjust: PASS" || { echo "  adjust: FAIL"; exit 1; }

# resize-bilinear
./imagedb resize-bilinear 1 128 128 output/bil.ppm > /dev/null
[ -f output/bil.ppm ] && echo "  resize-bilinear: PASS" || { echo "  resize-bilinear: FAIL"; exit 1; }

# resize-bilinear BMP output
./imagedb resize-bilinear 1 32 64 output/bil.bmp > /dev/null
[ -f output/bil.bmp ] && echo "  resize-bilinear(BMP): PASS" || { echo "  resize-bilinear(BMP): FAIL"; exit 1; }

# Verify bilinear resize dimensions
python3 -c "
with open('output/bil.ppm','rb') as f:
    magic = f.readline().strip()
    dims = f.readline().strip()
    while dims.startswith(b'#'): dims = f.readline().strip()
    f.readline()
    w, h = map(int, dims.split())
    data = f.read()
    assert w == 128 and h == 128, f'Expected 128x128, got {w}x{h}'
    assert len(data) == 128*128*3
" && echo "  resize-bilinear(128x128): PASS" || { echo "  resize-bilinear(128x128): FAIL"; exit 1; }

# -- Error tests --
echo ""
echo "--- Error handling ---"
set +e

# median kernel 4
./imagedb median 1 4 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  median(4): PASS" || { echo "  median(4): FAIL"; exit 1; }

# adjust invalid contrast
./imagedb adjust 1 0 0 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  adjust(bad contrast): PASS" || { echo "  adjust(bad contrast): FAIL"; exit 1; }

./imagedb adjust 1 0 abc output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  adjust(abc): PASS" || { echo "  adjust(abc): FAIL"; exit 1; }

# resize-bilinear 0
./imagedb resize-bilinear 1 0 100 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  resize-bilinear(0): PASS" || { echo "  resize-bilinear(0): FAIL"; exit 1; }

# equalize invalid ID
./imagedb equalize 999 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  equalize(999): PASS" || { echo "  equalize(999): FAIL"; exit 1; }

# median invalid ID
./imagedb median 999 3 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  median(999): PASS" || { echo "  median(999): FAIL"; exit 1; }

# gaussian invalid ID
./imagedb gaussian 999 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  gaussian(999): PASS" || { echo "  gaussian(999): FAIL"; exit 1; }

set -e

echo ""
echo "========================================="
echo "  All image ops tests passed."
echo "========================================="
