#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  C-ImageDB search-similar Tests"
echo "========================================="

echo ""
echo "[1/4] Building..."
make clean > /dev/null 2>&1 || true
make
echo "  Build: PASS"

echo ""
echo "[2/4] Preparing test images..."
rm -rf data output
mkdir -p output

cat > /tmp/sim_tie_a.ppm << 'PPMEOF'
P6
2 1
255
PPMEOF
perl -e 'print pack("CCC",255,0,0); print pack("CCC",0,0,255)' >> /tmp/sim_tie_a.ppm

cat > /tmp/sim_tie_b.ppm << 'PPMEOF'
P6
2 1
255
PPMEOF
perl -e 'print pack("CCC",0,0,255); print pack("CCC",255,0,0)' >> /tmp/sim_tie_b.ppm

cat > /tmp/sim_far.ppm << 'PPMEOF'
P6
2 1
255
PPMEOF
perl -e 'print pack("CCC",0,255,0) x 2' >> /tmp/sim_far.ppm

cat > /tmp/sim_query.ppm << 'PPMEOF'
P6
2 1
255
PPMEOF
perl -e 'print pack("CCC",255,0,0); print pack("CCC",0,0,255)' >> /tmp/sim_query.ppm

printf 'P3\n2 1\n255\n255 0 0 0 0 255\n' > /tmp/sim_bad.ppm
echo "  Images: PASS"

echo ""
echo "[3/4] Importing database images..."
./cimagedb init > /dev/null
./cimagedb import /tmp/sim_tie_a.ppm > /dev/null
./cimagedb import /tmp/sim_tie_b.ppm > /dev/null
./cimagedb import /tmp/sim_far.ppm > /dev/null
echo "  Import x3: PASS"

echo ""
echo "[4/4] Running search-similar checks..."

OUT="$(./cimagedb search-similar /tmp/sim_query.ppm --topk 2)"
echo "$OUT" | head -1 | grep -q "rank,image_path,distance" &&
    echo "  output(header): PASS" || { echo "  output(header): FAIL"; exit 1; }

FIRST_PATH="$(echo "$OUT" | awk -F, 'NR==2 {print $2}')"
SECOND_PATH="$(echo "$OUT" | awk -F, 'NR==3 {print $2}')"
[ "$FIRST_PATH" = "data/images/1.ppm" ] && [ "$SECOND_PATH" = "data/images/2.ppm" ] &&
    echo "  stable ordering: PASS" || { echo "  stable ordering: FAIL"; echo "$OUT"; exit 1; }

FIRST_DIST="$(echo "$OUT" | awk -F, 'NR==2 {print $3}')"
SECOND_DIST="$(echo "$OUT" | awk -F, 'NR==3 {print $3}')"
[ "$FIRST_DIST" = "0.00" ] && [ "$SECOND_DIST" = "0.00" ] &&
    echo "  l1 distance: PASS" || { echo "  l1 distance: FAIL"; echo "$OUT"; exit 1; }

ALL_COUNT="$(./cimagedb search-similar /tmp/sim_query.ppm --topk 99 | awk 'NR>1 {count++} END {print count+0}')"
[ "$ALL_COUNT" = "3" ] &&
    echo "  topk larger than db: PASS" || { echo "  topk larger than db: FAIL"; exit 1; }

set +e
./cimagedb search-similar /tmp/does_not_exist.ppm --topk 1 2>&1 | grep -q "\[ERROR\]" &&
    echo "  missing query: PASS" || { echo "  missing query: FAIL"; exit 1; }

./cimagedb search-similar /tmp/sim_bad.ppm --topk 1 2>&1 | grep -q "\[ERROR\]" &&
    echo "  bad ppm: PASS" || { echo "  bad ppm: FAIL"; exit 1; }

./cimagedb search-similar /tmp/sim_query.ppm --topk 0 2>&1 | grep -q "\[ERROR\]" &&
    echo "  bad topk: PASS" || { echo "  bad topk: FAIL"; exit 1; }

rm -rf data
./cimagedb init > /dev/null
./cimagedb search-similar /tmp/sim_query.ppm --topk 1 2>&1 | grep -q "\[ERROR\]" &&
    echo "  empty db: PASS" || { echo "  empty db: FAIL"; exit 1; }
set -e

echo ""
echo "========================================="
echo "  search-similar tests passed."
echo "========================================="
