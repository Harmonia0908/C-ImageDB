#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

TMP_ROOT="${TMPDIR:-/tmp}/cimagedb-storage-$$"
cleanup() {
    rm -rf "$TMP_ROOT"
}
trap cleanup EXIT

rm -rf data output "$TMP_ROOT"
mkdir -p output "$TMP_ROOT/source.with.dot"
make > /dev/null

python3 - "$TMP_ROOT" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])

def ppm(path: Path, rgb: bytes) -> None:
    path.write_bytes(b"P6\n1 1\n255\n" + rgb)

ppm(root / 'image_"quoted,field".ppm', bytes((255, 0, 0)))
ppm(root / "plain.ppm", bytes((0, 255, 0)))
ppm(root / "source.with.dot" / "no_extension", bytes((0, 0, 255)))
ppm(root / (("x" * 130) + ".ppm"), bytes((10, 20, 30)))
ppm(root / "rollback.ppm", bytes((15, 25, 35)))
ppm(root / "overflow_id.ppm", bytes((20, 30, 40)))
PY

./imagedb init > /dev/null
./imagedb import "$TMP_ROOT/image_\"quoted,field\".ppm" > /dev/null
./imagedb import "$TMP_ROOT/plain.ppm" > /dev/null

./imagedb export output/metadata.csv > /dev/null
./imagedb search-export 2 1 output/search.csv --metric l1 > /dev/null

python3 - "$TMP_ROOT" <<'PY'
import csv
from pathlib import Path
import sys

root = Path(sys.argv[1])
expected = 'image_"quoted,field".ppm'

with Path("output/metadata.csv").open(newline="") as f:
    rows = list(csv.reader(f))
assert len(rows) == 3, rows
assert all(len(row) == 9 for row in rows), rows
assert rows[1][1] == expected, rows[1]

with Path("output/search.csv").open(newline="") as f:
    rows = list(csv.reader(f))
assert len(rows) == 2, rows
assert len(rows[1]) == 6, rows[1]
assert rows[1][2] == expected, rows[1]
PY
echo "  csv(quoted fields): PASS"

./imagedb import "$TMP_ROOT/source.with.dot/no_extension" > /dev/null
test -f data/images/3.ppm
echo "  import(canonical extension): PASS"

cp data/metadata.dat "$TMP_ROOT/metadata.before"
cp data/features.dat "$TMP_ROOT/features.before"
mkdir data/features.bak
set +e
./imagedb import "$TMP_ROOT/rollback.ppm" > /dev/null 2>&1
ROLLBACK_STATUS=$?
set -e
test "$ROLLBACK_STATUS" -ne 0
cmp -s data/metadata.dat "$TMP_ROOT/metadata.before"
cmp -s data/features.dat "$TMP_ROOT/features.before"
test ! -e data/images/4.ppm
test ! -e data/metadata.bak
test ! -e data/metadata.tmp
test ! -e data/features.tmp
rmdir data/features.bak
echo "  store(paired update rollback): PASS"

set +e
./imagedb import "$TMP_ROOT/$(printf 'x%.0s' {1..130}).ppm" > /dev/null 2>&1
LONG_STATUS=$?
set -e
test "$LONG_STATUS" -ne 0
echo "  import(long filename rejected): PASS"

mkdir -p output/protected
touch output/protected/sentinel
set +e
./imagedb export output/protected > /dev/null 2>&1
EXPORT_STATUS=$?
set -e
test "$EXPORT_STATUS" -ne 0
test -f output/protected/sentinel
test ! -e output/protected.tmp
echo "  csv(failed replace preserves target): PASS"

cp data/metadata.dat "$TMP_ROOT/metadata.good"
python3 - <<'PY'
from pathlib import Path

path = Path("data/metadata.dat")
data = bytearray(path.read_bytes())
data[4:132] = b"A" * 128
path.write_bytes(data)
PY
set +e
./imagedb export output/corrupt.csv > /dev/null 2>&1
CORRUPT_STATUS=$?
set -e
test "$CORRUPT_STATUS" -ne 0
echo "  store(unterminated field rejected): PASS"
mv "$TMP_ROOT/metadata.good" data/metadata.dat

printf '2147483647\n' > data/.next_id
set +e
./imagedb import "$TMP_ROOT/overflow_id.ppm" > /dev/null 2>&1
ID_STATUS=$?
set -e
test "$ID_STATUS" -ne 0
grep -qx '2147483647' data/.next_id
echo "  store(next id overflow rejected): PASS"

echo "storage integration tests: PASS"
