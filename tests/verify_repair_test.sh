#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  C-ImageDB Verify / Repair Tests"
echo "========================================="

echo ""
echo "[1/5] Building..."
make clean > /dev/null 2>&1 || true
make
echo "  Build: PASS"

prepare_db() {
    rm -rf data output
    mkdir -p output
    bash scripts/generate_samples.sh > /dev/null 2>&1 || true
    ./cimagedb init > /dev/null
    ./cimagedb import samples/sample1.ppm > /dev/null
    ./cimagedb import samples/sample2.ppm > /dev/null
}

echo ""
echo "[2/5] Normal verify..."
prepare_db
./cimagedb verify | grep -q "status=OK" &&
    echo "  verify(clean): PASS" || { echo "  verify(clean): FAIL"; exit 1; }

echo ""
echo "[3/5] Missing image file repair..."
rm -f data/images/2.ppm
set +e
VERIFY_OUT="$(./cimagedb verify 2>&1)"
VERIFY_CODE=$?
set -e
[ "$VERIFY_CODE" -ne 0 ] && echo "$VERIFY_OUT" | grep -q "missing_files=1" &&
    echo "  verify(missing file): PASS" || { echo "  verify(missing file): FAIL"; echo "$VERIFY_OUT"; exit 1; }

./cimagedb repair | grep -q "removed_records=1" &&
    echo "  repair(remove missing): PASS" || { echo "  repair(remove missing): FAIL"; exit 1; }

./cimagedb verify | grep -q "status=OK" &&
    echo "  verify(after remove): PASS" || { echo "  verify(after remove): FAIL"; exit 1; }

echo ""
echo "[4/5] Missing histogram repair..."
prepare_db
: > data/features.dat
set +e
VERIFY_OUT="$(./cimagedb verify 2>&1)"
VERIFY_CODE=$?
set -e
[ "$VERIFY_CODE" -ne 0 ] && echo "$VERIFY_OUT" | grep -q "missing_histograms=2" &&
    echo "  verify(missing hist): PASS" || { echo "  verify(missing hist): FAIL"; echo "$VERIFY_OUT"; exit 1; }

./cimagedb repair | grep -q "regenerated_histograms=2" &&
    echo "  repair(regen hist): PASS" || { echo "  repair(regen hist): FAIL"; exit 1; }

./cimagedb verify | grep -q "status=OK" &&
    echo "  verify(after regen): PASS" || { echo "  verify(after regen): FAIL"; exit 1; }

echo ""
echo "[5/5] Duplicate id detection..."
prepare_db
python3 - <<'PY'
from pathlib import Path
p = Path("data/metadata.dat")
data = p.read_bytes()
record_size = len(data) // 2
p.write_bytes(data + data[:record_size])
PY

set +e
DUP_OUT="$(./cimagedb verify 2>&1)"
DUP_CODE=$?
set -e
[ "$DUP_CODE" -ne 0 ] && echo "$DUP_OUT" | grep -q "duplicate_ids=1" &&
    echo "  verify(duplicate id): PASS" || { echo "  verify(duplicate id): FAIL"; echo "$DUP_OUT"; exit 1; }

set +e
REPAIR_OUT="$(./cimagedb repair 2>&1)"
REPAIR_CODE=$?
set -e
echo "$REPAIR_OUT" | grep -q "remaining_issues=1" &&
    echo "  repair(remaining issue): PASS" || { echo "  repair(remaining issue): FAIL"; echo "$REPAIR_OUT"; exit 1; }
[ "$REPAIR_CODE" -ne 0 ] || { echo "  repair(duplicate exit): FAIL"; exit 1; }
echo "  repair(duplicate exit): PASS"

echo ""
echo "========================================="
echo "  Verify / repair tests passed."
echo "========================================="
