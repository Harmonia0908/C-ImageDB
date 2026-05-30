#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

echo "========================================="
echo "  C-ImageDB Demo"
echo "========================================="

echo ""
echo "[1/5] Building..."
make clean > /dev/null 2>&1 || true
make

echo ""
echo "[2/5] Preparing environment..."
if [ ! -f samples/sample1.ppm ]; then
    bash scripts/generate_samples.sh
fi

rm -rf data output
mkdir -p output

echo ""
echo "[3/5] Setting up database and importing..."
./imagedb init
./imagedb import samples/sample1.ppm
./imagedb import samples/sample2.ppm

echo ""
echo "[4/5] Running demo commands..."

echo "  - Grayscale conversion"
./imagedb gray 1 output/demo_gray.ppm

echo "  - Edge detection"
./imagedb edge 1 output/demo_edge.ppm

echo "  - Histogram visualization"
./imagedb hist-image 1 output/demo_hist.ppm

echo "  - Similarity search"
./imagedb search 1 3 --metric intersection

echo "  - Similarity search CSV"
./imagedb search-export 1 3 output/demo_search.csv --metric l1

echo "  - Search result contact sheet"
./imagedb search-contact 1 3 output/demo_contact.ppm

echo "  - Metadata export"
./imagedb export output/demo_metadata.csv

echo "  - Database statistics"
./imagedb stats

echo "  - HTML report"
./imagedb report output output/index.html

echo ""
echo "[5/5] Generated demo artifacts:"
echo ""
echo "  output/demo_gray.ppm       - Grayscale image"
echo "  output/demo_edge.ppm       - Sobel edge detection result"
echo "  output/demo_hist.ppm       - RGB histogram visualization (768x256)"
echo "  output/demo_contact.ppm    - Search result contact sheet"
echo "  output/demo_metadata.csv   - Exported metadata"
echo "  output/demo_search.csv     - Top-K L1 search results"
echo "  output/index.html          - HTML demo report"
echo ""
echo "========================================="
echo "  Demo complete."
echo "========================================="
