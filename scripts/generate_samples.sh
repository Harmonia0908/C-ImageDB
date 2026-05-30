#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SAMPLES_DIR="$SCRIPT_DIR/../samples"

mkdir -p "$SAMPLES_DIR"

# Generate a simple red-green-blue gradient test image (64x64)
# PPM P6 format: magic, width height, maxval, binary RGB data
generate_ppm() {
    local name="$1"
    local width="$2"
    local height="$3"
    local out="$SAMPLES_DIR/$name.ppm"

    {
        printf "P6\n%d %d\n255\n" "$width" "$height"
        # Use perl to generate the pixel data (available on macOS/Linux)
        perl -e '
            $w = '"$width"'; $h = '"$height"';
            for ($y = 0; $y < $h; $y++) {
                for ($x = 0; $x < $w; $x++) {
                    $r = int(($x / $w) * 255);
                    $g = int(($y / $h) * 255);
                    $b = 128;
                    print pack("CCC", $r, $g, $b);
                }
            }
        '
    } > "$out"
    echo "Generated $out (${width}x${height})"
}

generate_ppm "sample1" 64 64
generate_ppm "sample2" 128 128
generate_ppm "sample3" 256 256

# Generate BMP samples using python3
if command -v python3 &> /dev/null; then
    python3 -c "
import struct
def wbmp(p, w, h, r, g, b):
    rr = w * 3
    rp = (rr + 3) & ~3
    off = 54
    fs = off + rp * h
    with open(p, 'wb') as f:
        f.write(struct.pack('<HIHHI', 0x4D42, fs, 0, 0, off))
        f.write(struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, rp * h, 0, 0, 0, 0))
        for _ in range(h):
            for _ in range(w):
                f.write(struct.pack('BBB', b, g, r))
            f.write(b'\x00' * (rp - rr))
wbmp('$SAMPLES_DIR/sample_bmp1.bmp', 64, 64, 255, 0, 0)
wbmp('$SAMPLES_DIR/sample_bmp2.bmp', 128, 128, 0, 255, 0)
"
    echo "Generated BMP samples"
fi

echo "All sample images generated."
