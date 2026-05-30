#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  C-ImageDB Basic Test Suite"
echo "========================================="

# -- Clean build --
echo ""
echo "[1/5] Building..."
make clean > /dev/null 2>&1 || true
make
echo "  Build: PASS"

# -- Prepare --
echo ""
echo "[2/5] Preparing test environment..."
rm -rf data output
mkdir -p output

# Generate samples if not present
if [ ! -f samples/sample1.ppm ]; then
    bash scripts/generate_samples.sh > /dev/null 2>&1
    echo "  Samples generated"
fi

# Generate a PPM with comments in header for testing
cat > /tmp/test_comment.ppm << 'PPMEOF'
P6
# This is a comment
64 64
# Another comment after dimensions
255
PPMEOF
# Append pixel data (solid blue, 64x64) — different from BMP to avoid hash collision
perl -e 'print pack("CCC",0,0,255) x (64*64)' >> /tmp/test_comment.ppm
echo "  PPM with comments generated"

./imagedb init
echo "  Init: PASS"

# -- Import --
echo ""
echo "[3/5] Importing images..."
./imagedb import samples/sample1.ppm > /dev/null
./imagedb import samples/sample2.ppm > /dev/null
./imagedb import samples/sample3.ppm > /dev/null
echo "  Import x3: PASS"

# -- Basic commands --
echo ""
echo "[4/5] Running command tests..."

# list
./imagedb list | grep -q "sample1" && echo "  list: PASS" || { echo "  list: FAIL"; exit 1; }

# info
./imagedb info 1 | grep -q "sample1" && echo "  info: PASS" || { echo "  info: FAIL"; exit 1; }

# gray
./imagedb gray 1 output/test_gray.ppm > /dev/null
python3 -c "
with open('output/test_gray.ppm','rb') as f:
    magic=f.readline().strip()
    while f.readline().startswith(b'#'): pass
    f.readline()
    data=f.read()
    assert len(data)==64*64*3, 'bad size'
" && echo "  gray: PASS" || { echo "  gray: FAIL"; exit 1; }

# binary
./imagedb binary 1 128 output/test_binary.ppm > /dev/null
[ -f output/test_binary.ppm ] && echo "  binary: PASS" || { echo "  binary: FAIL"; exit 1; }

# blur
./imagedb blur 1 output/test_blur.ppm > /dev/null
[ -f output/test_blur.ppm ] && echo "  blur: PASS" || { echo "  blur: FAIL"; exit 1; }

# edge
./imagedb edge 1 output/test_edge.ppm > /dev/null
[ -f output/test_edge.ppm ] && echo "  edge: PASS" || { echo "  edge: FAIL"; exit 1; }

# hist
./imagedb hist 1 | grep -q "Average R" && echo "  hist: PASS" || { echo "  hist: FAIL"; exit 1; }

# search (default intersection)
./imagedb search 1 3 | grep -q "Metric: intersection" && echo "  search(intersection): PASS" || { echo "  search(intersection): FAIL"; exit 1; }

# search --metric l1
./imagedb search 1 3 --metric l1 | grep -q "Metric: l1" && echo "  search(l1): PASS" || { echo "  search(l1): FAIL"; exit 1; }

# search --metric l2
./imagedb search 1 3 --metric l2 | grep -q "Metric: l2" && echo "  search(l2): PASS" || { echo "  search(l2): FAIL"; exit 1; }

# resize
./imagedb resize 1 128 128 output/resize_test.ppm > /dev/null
[ -f output/resize_test.ppm ] && echo "  resize: PASS" || { echo "  resize: FAIL"; exit 1; }

# Verify resize dimensions
python3 -c "
with open('output/resize_test.ppm','rb') as f:
    magic = f.readline().strip()
    dims = f.readline().strip()
    while dims.startswith(b'#'): dims = f.readline().strip()
    f.readline()
    w, h = map(int, dims.split())
    data = f.read()
    assert w == 128 and h == 128, f'Expected 128x128, got {w}x{h}'
    assert len(data) == 128*128*3, f'Bad data size: {len(data)}'
" && echo "  resize(128x128): PASS" || { echo "  resize(128x128): FAIL"; exit 1; }

# resize to BMP output
./imagedb resize 1 32 32 output/resize_out.bmp > /dev/null
[ -f output/resize_out.bmp ] && echo "  resize(BMP): PASS" || { echo "  resize(BMP): FAIL"; exit 1; }

# rotate 90
./imagedb rotate 1 90 output/rotate90.ppm > /dev/null
[ -f output/rotate90.ppm ] && echo "  rotate(90): PASS" || { echo "  rotate(90): FAIL"; exit 1; }

# rotate 180
./imagedb rotate 1 180 output/rotate180.ppm > /dev/null
[ -f output/rotate180.ppm ] && echo "  rotate(180): PASS" || { echo "  rotate(180): FAIL"; exit 1; }

# rotate 270
./imagedb rotate 1 270 output/rotate270.ppm > /dev/null
[ -f output/rotate270.ppm ] && echo "  rotate(270): PASS" || { echo "  rotate(270): FAIL"; exit 1; }

# Verify rotate 90 swaps dimensions (64x64 -> 64x64, no change for square)
python3 -c "
with open('output/rotate90.ppm','rb') as f:
    magic = f.readline().strip()
    dims = f.readline().strip()
    while dims.startswith(b'#'): dims = f.readline().strip()
    f.readline()
    w, h = map(int, dims.split())
    data = f.read()
    assert w == 64 and h == 64, f'Expected 64x64, got {w}x{h}'
    assert len(data) == 64*64*3, f'Bad data size'
" && echo "  rotate(dims): PASS" || { echo "  rotate(dims): FAIL"; exit 1; }

# delete
./imagedb delete 3 > /dev/null
./imagedb info 3 2>&1 | grep -q "not found" && echo "  delete: PASS" || { echo "  delete: FAIL"; exit 1; }

# -- BMP tests --
echo ""
echo "--- BMP import and processing ---"

# Generate BMP samples if not present
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
" 2>/dev/null
fi

# BMP import
./imagedb import samples/sample_bmp1.bmp > /dev/null
./imagedb info 4 2>&1 | grep -q "sample_bmp1" && echo "  import(BMP): PASS" || { echo "  import(BMP): FAIL"; exit 1; }

# BMP grayscale
./imagedb gray 4 output/bmp_gray.ppm > /dev/null
[ -f output/bmp_gray.ppm ] && echo "  gray(BMP->PPM): PASS" || { echo "  gray(BMP->PPM): FAIL"; exit 1; }

# BMP edge detection, output as BMP
./imagedb edge 4 output/bmp_edge.bmp > /dev/null
[ -f output/bmp_edge.bmp ] && echo "  edge(BMP->BMP): PASS" || { echo "  edge(BMP->BMP): FAIL"; exit 1; }

# Verify BMP edge output is a valid BMP
python3 -c "
with open('output/bmp_edge.bmp','rb') as f:
    sig = f.read(2)
    assert sig == b'BM', 'not a BMP'
print('  BMP valid: PASS')
" || { echo "  BMP valid: FAIL"; exit 1; }

# -- Error handling tests --
echo ""
echo "--- Error handling tests ---"
set +e

./imagedb info 9999 2>&1 | grep -q "\[ERROR\]" && echo "  info(9999): PASS" || { echo "  info(9999): FAIL"; exit 1; }

./imagedb binary 1 -1 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  binary(-1): PASS" || { echo "  binary(-1): FAIL"; exit 1; }

./imagedb binary 1 300 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  binary(300): PASS" || { echo "  binary(300): FAIL"; exit 1; }

./imagedb import nonexistent.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  import(bad): PASS" || { echo "  import(bad): FAIL"; exit 1; }

./imagedb unknown 2>&1 | grep -q "\[ERROR\]" && echo "  unknown: PASS" || { echo "  unknown: FAIL"; exit 1; }

./imagedb search 9999 3 2>&1 | grep -q "\[ERROR\]" && echo "  search(9999): PASS" || { echo "  search(9999): FAIL"; exit 1; }

# duplicate import
./imagedb import samples/sample1.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  import(dup): PASS" || { echo "  import(dup): FAIL"; exit 1; }

# -- NEW: search deleted image must fail --
echo ""
echo "--- New tests: deleted-query search ---"
./imagedb search 3 3 2>&1 | grep -q "\[ERROR\]" && echo "  search(deleted): PASS" || { echo "  search(deleted): FAIL"; exit 1; }

# -- NEW: PPM with comments --
echo ""
echo "--- New tests: PPM with comments ---"
./imagedb import /tmp/test_comment.ppm > /dev/null 2>&1
./imagedb list | grep -q "test_comment" && echo "  import(comments): PASS" || { echo "  import(comments): FAIL"; exit 1; }

# -- NEW: invalid numeric arguments --
echo ""
echo "--- New tests: invalid numeric args ---"
./imagedb info abc 2>&1 | grep -q "\[ERROR\]" && echo "  info(abc): PASS" || { echo "  info(abc): FAIL"; exit 1; }
./imagedb info 1xyz 2>&1 | grep -q "\[ERROR\]" && echo "  info(1xyz): PASS" || { echo "  info(1xyz): FAIL"; exit 1; }
./imagedb search 1 abc 2>&1 | grep -q "\[ERROR\]" && echo "  search(abc): PASS" || { echo "  search(abc): FAIL"; exit 1; }
./imagedb binary 1 128abc output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  binary(128abc): PASS" || { echo "  binary(128abc): FAIL"; exit 1; }

# invalid metric
./imagedb search 1 3 --metric cosine 2>&1 | grep -q "\[ERROR\]" && echo "  search(bad_metric): PASS" || { echo "  search(bad_metric): FAIL"; exit 1; }

# resize with 0 dimension
./imagedb resize 1 0 100 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  resize(0): PASS" || { echo "  resize(0): FAIL"; exit 1; }

# rotate 45 (invalid)
./imagedb rotate 1 45 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  rotate(45): PASS" || { echo "  rotate(45): FAIL"; exit 1; }

# -- Phase 5: find-name / query / stats / compact / export smoke tests --
echo ""
echo "--- Phase 6: image ops smoke tests ---"

./imagedb equalize 1 output/eq.ppm > /dev/null 2>&1
[ -f output/eq.ppm ] && echo "  equalize: PASS" || { echo "  equalize: FAIL"; exit 1; }

./imagedb median 1 3 output/med.ppm > /dev/null 2>&1
[ -f output/med.ppm ] && echo "  median: PASS" || { echo "  median: FAIL"; exit 1; }

./imagedb gaussian 1 output/gauss.ppm > /dev/null 2>&1
[ -f output/gauss.ppm ] && echo "  gaussian: PASS" || { echo "  gaussian: FAIL"; exit 1; }

./imagedb adjust 1 0 1.0 output/adj.ppm > /dev/null 2>&1
[ -f output/adj.ppm ] && echo "  adjust: PASS" || { echo "  adjust: FAIL"; exit 1; }

./imagedb resize-bilinear 1 64 64 output/bil.ppm > /dev/null 2>&1
[ -f output/bil.ppm ] && echo "  resize-bilinear: PASS" || { echo "  resize-bilinear: FAIL"; exit 1; }

echo ""
echo "--- Phase 5: database command smoke tests ---"

./imagedb find-name sample1 | grep -q "sample1" && echo "  find-name: PASS" || { echo "  find-name: FAIL"; exit 1; }
./imagedb query width gt 0 | grep -q "sample1" && echo "  query: PASS" || { echo "  query: FAIL"; exit 1; }
./imagedb stats | grep -q "Total records" && echo "  stats: PASS" || { echo "  stats: FAIL"; exit 1; }
./imagedb export output/metadata.csv > /dev/null 2>&1
[ -f output/metadata.csv ] && echo "  export: PASS" || { echo "  export: FAIL"; exit 1; }
./imagedb compact | grep -q "Compact complete" && echo "  compact: PASS" || { echo "  compact: FAIL"; exit 1; }

# -- Pre-release: PPM first-byte and oversized tests --
echo ""
echo "--- v1.0-hardening boundary tests ---"

# PPM whose first pixel is a whitespace-like byte (R=32 = space, R=10 = newline)
cat > /tmp/test_blank_pixel.ppm << 'PPMEOF'
P6
4 4
255
PPMEOF
# First pixel R=32 (space), G=64, B=128; rest black
perl -e 'print pack("CCC",32,64,128); print pack("CCC",0,0,0) x 15' >> /tmp/test_blank_pixel.ppm
./imagedb import /tmp/test_blank_pixel.ppm > /dev/null 2>&1
./imagedb list | grep -q "blank_pixel" && echo "  ppm(blank pixel): PASS" || { echo "  ppm(blank pixel): FAIL"; exit 1; }

# PPM with R=10 (newline byte) as first pixel
cat > /tmp/test_nl_pixel.ppm << 'PPMEOF'
P6
4 4
255
PPMEOF
perl -e 'print pack("CCC",10,64,128); print pack("CCC",0,0,0) x 15' >> /tmp/test_nl_pixel.ppm
./imagedb import /tmp/test_nl_pixel.ppm > /dev/null 2>&1
./imagedb list | grep -q "nl_pixel" && echo "  ppm(newline pixel): PASS" || { echo "  ppm(newline pixel): FAIL"; exit 1; }

# Oversized PPM header (width > MAX_IMAGE_WIDTH) should be rejected
cat > /tmp/test_oversized.ppm << 'PPMEOF'
P6
99999 64
255
PPMEOF
perl -e 'print pack("CCC",0,0,0) x 100' >> /tmp/test_oversized.ppm
./imagedb import /tmp/test_oversized.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  ppm(oversized): PASS" || { echo "  ppm(oversized): FAIL"; exit 1; }

# BMP with wrong bit_count (not 24-bit)
python3 -c "
import struct
def wbmp(p,w,h,bits):
    rr=w*3; rp=(rr+3)&~3; off=54; fs=off+rp*h
    with open(p,'wb') as f:
        f.write(struct.pack('<HIHHI',0x4D42,fs,0,0,off))
        f.write(struct.pack('<IiiHHIIiiII',40,w,h,1,bits,0,rp*h,0,0,0,0))
        for _ in range(h):
            for _ in range(w): f.write(struct.pack('BBB',0,0,255))
            f.write(b'\x00'*(rp-rr))
wbmp('/tmp/test_8bit.bmp',16,16,8)
" 2>/dev/null
./imagedb import /tmp/test_8bit.bmp 2>&1 | grep -q "\[ERROR\]" && echo "  bmp(8bit): PASS" || { echo "  bmp(8bit): FAIL"; exit 1; }

# BMP with compression != 0 (RLE)
python3 -c "
import struct
def wbmp_rle(p,w,h):
    rr=w*3; rp=(rr+3)&~3; off=54; fs=off+rp*h
    with open(p,'wb') as f:
        f.write(struct.pack('<HIHHI',0x4D42,fs,0,0,off))
        f.write(struct.pack('<IiiHHIIiiII',40,w,h,1,24,1,rp*h,0,0,0,0))
        for _ in range(h):
            for _ in range(w): f.write(struct.pack('BBB',0,0,255))
            f.write(b'\x00'*(rp-rr))
wbmp_rle('/tmp/test_rle.bmp',16,16)
" 2>/dev/null
./imagedb import /tmp/test_rle.bmp 2>&1 | grep -q "\[ERROR\]" && echo "  bmp(compressed): PASS" || { echo "  bmp(compressed): FAIL"; exit 1; }

# PPM width overflow (4294967297 > INT_MAX)
python3 -c "
with open('/tmp/test_overflow_w.ppm','wb') as f:
    f.write(b'P6\n4294967297 64\n255\n')
    f.write(b'\x00' * 100)
" 2>/dev/null
./imagedb import /tmp/test_overflow_w.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  ppm(overflow w): PASS" || { echo "  ppm(overflow w): FAIL"; exit 1; }

# PPM height overflow
python3 -c "
with open('/tmp/test_overflow_h.ppm','wb') as f:
    f.write(b'P6\n64 4294967297\n255\n')
    f.write(b'\x00' * 100)
" 2>/dev/null
./imagedb import /tmp/test_overflow_h.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  ppm(overflow h): PASS" || { echo "  ppm(overflow h): FAIL"; exit 1; }

# Invalid maxval
printf 'P6\n4 4\n128\n' > /tmp/test_bad_maxval.ppm
perl -e 'print pack("CCC",0,0,0) x 16' >> /tmp/test_bad_maxval.ppm
./imagedb import /tmp/test_bad_maxval.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  ppm(bad maxval): PASS" || { echo "  ppm(bad maxval): FAIL"; exit 1; }

# Zero-arg rejection
./imagedb list extra 2>&1 | grep -q "\[ERROR\]" && echo "  list(extra): PASS" || { echo "  list(extra): FAIL"; exit 1; }
./imagedb stats extra 2>&1 | grep -q "\[ERROR\]" && echo "  stats(extra): PASS" || { echo "  stats(extra): FAIL"; exit 1; }
./imagedb compact extra 2>&1 | grep -q "\[ERROR\]" && echo "  compact(extra): PASS" || { echo "  compact(extra): FAIL"; exit 1; }

# Adjust overflow brightness
./imagedb adjust 1 999999999999999999999 1.0 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  adjust(bad bright): PASS" || { echo "  adjust(bad bright): FAIL"; exit 1; }

# Adjust nan/inf/negative contrast
./imagedb adjust 1 0 nan output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  adjust(nan): PASS" || { echo "  adjust(nan): FAIL"; exit 1; }
./imagedb adjust 1 0 inf output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  adjust(inf): PASS" || { echo "  adjust(inf): FAIL"; exit 1; }
./imagedb adjust 1 0 -1 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  adjust(neg c): PASS" || { echo "  adjust(neg c): FAIL"; exit 1; }

# Delete then hist-export/hist-image must fail
TMPID=$(./imagedb list | head -3 | tail -1 | awk '{print $1}')
[ -n "$TMPID" ] && ./imagedb delete "$TMPID" > /dev/null 2>&1 || true
if [ -n "$TMPID" ]; then
    ./imagedb hist-export "$TMPID" output/x.csv 2>&1 | grep -q "\[ERROR\]" && echo "  hist-export(del): PASS" || { echo "  hist-export(del): FAIL"; exit 1; }
    ./imagedb hist-image "$TMPID" output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  hist-image(del): PASS" || { echo "  hist-image(del): FAIL"; exit 1; }
fi

set -e

# Intersection score test (must be after set -e)
echo ""
echo "--- Intersection normalization test ---"
cat > /tmp/test_red_small.ppm << 'PPMEOF'
P6
16 16
255
PPMEOF
perl -e 'print pack("CCC",255,10,10) x 256' >> /tmp/test_red_small.ppm
cat > /tmp/test_red_big.ppm << 'PPMEOF'
P6
64 64
255
PPMEOF
perl -e 'print pack("CCC",255,10,10) x 4096' >> /tmp/test_red_big.ppm
cat > /tmp/test_blue_small.ppm << 'PPMEOF'
P6
16 16
255
PPMEOF
perl -e 'print pack("CCC",10,10,255) x 256' >> /tmp/test_blue_small.ppm

./imagedb import /tmp/test_red_small.ppm > /dev/null
R1=$(./imagedb find-name red_small | awk 'NR>=3{print $1; exit}')
./imagedb import /tmp/test_red_big.ppm > /dev/null
R2=$(./imagedb find-name red_big | awk 'NR>=3{print $1; exit}')
./imagedb import /tmp/test_blue_small.ppm > /dev/null
B1=$(./imagedb find-name blue_small | awk 'NR>=3{print $1; exit}')

# Same color, different size: intersection should be close to 1.0
SAME=$(./imagedb search "$R1" 5 --metric intersection 2>&1 | grep "id=$R2" | grep -o 'score=[0-9.]*' | cut -d= -f2)
python3 -c "assert float('$SAME') > 0.9, 'same-color score too low: $SAME'" && echo "  intersection(same): PASS" || { echo "  intersection(same): FAIL (score=$SAME)"; exit 1; }

# Different color: should be clearly lower than same-color
DIFF=$(./imagedb search "$R1" 5 --metric intersection 2>&1 | grep "id=$B1" | grep -o 'score=[0-9.]*' | cut -d= -f2)
python3 -c "assert float('$DIFF') < float('$SAME'), 'diff-color should be lower: diff=$DIFF same=$SAME'" && echo "  intersection(diff): PASS" || { echo "  intersection(diff): FAIL (diff=$DIFF, same=$SAME)"; exit 1; }

# search-contact with oversized top_k must fail
./imagedb search-contact "$R1" 999999 output/x.ppm 2>&1 | grep -q "\[ERROR\]" && echo "  contact(overflow): PASS" || { echo "  contact(overflow): FAIL"; exit 1; }

echo ""
echo "========================================="
echo "  All tests passed."
echo "========================================="
