#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

TMP_ROOT="${TMPDIR:-/tmp}/cimagedb-storage-$$"
cleanup() {
    rm -rf "$TMP_ROOT"
}
trap cleanup EXIT

set_first_record_path() {
    python3 - "$1" <<'PY'
import ctypes
from pathlib import Path
import sys

class Record(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_int),
        ("name", ctypes.c_char * 128),
        ("path", ctypes.c_char * 256),
        ("width", ctypes.c_int),
        ("height", ctypes.c_int),
        ("channels", ctypes.c_int),
        ("file_size", ctypes.c_long),
        ("import_time", ctypes.c_long),
        ("content_hash", ctypes.c_uint64),
        ("deleted", ctypes.c_int),
    ]

path = Path("data/metadata.dat")
data = bytearray(path.read_bytes())
offset = Record.path.offset
value = sys.argv[1].encode() + b"\0"
assert len(value) <= 256
data[offset:offset + 256] = value.ljust(256, b"\0")
path.write_bytes(data)
PY
}

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

# Each CLI invocation is a fresh process. Verify persisted data is available
# after the importing process has exited.
./imagedb list > "$TMP_ROOT/restart-list.txt"
./imagedb info 1 > "$TMP_ROOT/restart-info.txt"
grep -q 'image_"quoted,field".ppm' "$TMP_ROOT/restart-list.txt"
grep -q '^ID: 1$' "$TMP_ROOT/restart-info.txt"
echo "  store(restart persistence): PASS"

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

cp data/metadata.dat "$TMP_ROOT/metadata.good"
set_first_record_path "data/images/1.PNM"
mv data/images/1.ppm data/images/1.PNM
./imagedb gray 1 output/legacy_path.ppm > /dev/null
mv data/images/1.PNM data/images/1.ppm
echo "  store(legacy local extension accepted): PASS"
mv "$TMP_ROOT/metadata.good" data/metadata.dat

cp data/metadata.dat "$TMP_ROOT/metadata.good"
set_first_record_path "../../outside.ppm"
set +e
./imagedb info 1 > /dev/null 2>&1
PATH_STATUS=$?
set -e
test "$PATH_STATUS" -ne 0
echo "  store(path traversal rejected): PASS"
mv "$TMP_ROOT/metadata.good" data/metadata.dat

printf '2147483647\n' > data/.next_id
set +e
./imagedb import "$TMP_ROOT/overflow_id.ppm" > /dev/null 2>&1
ID_STATUS=$?
set -e
test "$ID_STATUS" -ne 0
grep -qx '2147483647' data/.next_id
echo "  store(next id overflow rejected): PASS"

LEGACY_ROOT="$TMP_ROOT/legacy-store"
LEGACY_FIXTURE="$PROJECT_DIR/tests/fixtures/storage-v1-lp64"
mkdir -p "$LEGACY_ROOT/data/images"
python3 - "$LEGACY_FIXTURE" "$LEGACY_ROOT/data" <<'PY'
import base64
import ctypes
import gzip
import hashlib
from pathlib import Path
import sys

fixture = Path(sys.argv[1])
data_dir = Path(sys.argv[2])

class Record(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_int),
        ("name", ctypes.c_char * 128),
        ("path", ctypes.c_char * 256),
        ("width", ctypes.c_int),
        ("height", ctypes.c_int),
        ("channels", ctypes.c_int),
        ("file_size", ctypes.c_long),
        ("import_time", ctypes.c_long),
        ("content_hash", ctypes.c_uint64),
        ("deleted", ctypes.c_int),
    ]

class Feature(ctypes.Structure):
    _fields_ = [
        ("image_id", ctypes.c_int),
        ("r_hist", ctypes.c_int * 256),
        ("g_hist", ctypes.c_int * 256),
        ("b_hist", ctypes.c_int * 256),
        ("avg_r", ctypes.c_double),
        ("avg_g", ctypes.c_double),
        ("avg_b", ctypes.c_double),
    ]

assert ctypes.sizeof(Record) == 432, ctypes.sizeof(Record)
assert ctypes.sizeof(Feature) == 3104, ctypes.sizeof(Feature)
assert sys.byteorder == "little", sys.byteorder

files = {
    "metadata.dat.gz.b64": (
        "metadata.dat",
        "17b907df86b031951a5e87364c8df2f4a3a51bae2645eb82f4a9fce978351200",
    ),
    "features.dat.gz.b64": (
        "features.dat",
        "3ac67ef0e9b6b830f7760a3caa53bc0495d0d6c707ad932188e62237f13d040c",
    ),
    "next_id.gz.b64": (
        ".next_id",
        "53c234e5e8472b6ac51c1ae1cab3fe06fad053beb8ebfd8977b010655bfdd3c3",
    ),
}
for source_name, (target_name, expected_hash) in files.items():
    encoded = (fixture / source_name).read_bytes()
    raw = gzip.decompress(base64.b64decode(encoded))
    assert hashlib.sha256(raw).hexdigest() == expected_hash
    (data_dir / target_name).write_bytes(raw)
PY

(
    cd "$LEGACY_ROOT"
    "$PROJECT_DIR/imagedb" list > legacy-list.txt
    "$PROJECT_DIR/imagedb" info 1 > legacy-info.txt
    "$PROJECT_DIR/imagedb" search 1 1 > legacy-search.txt
    "$PROJECT_DIR/imagedb" import "$PROJECT_DIR/samples/sample2.ppm" \
        > legacy-import.txt
)
grep -q 'sample1.ppm' "$LEGACY_ROOT/legacy-list.txt"
grep -q '^Width: 64 x Height: 64$' "$LEGACY_ROOT/legacy-info.txt"
grep -q '^Query image: 1$' "$LEGACY_ROOT/legacy-search.txt"
grep -q '^ID: 2$' "$LEGACY_ROOT/legacy-import.txt"
grep -qx '3' "$LEGACY_ROOT/data/.next_id"
echo "  store(legacy v1 files readable): PASS"

echo "storage integration tests: PASS"
