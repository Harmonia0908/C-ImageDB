#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROFILE="${1:-debug}"

if [ "$#" -gt 1 ]; then
    echo "Usage: $0 [debug|release|strict]" >&2
    exit 2
fi

cd "$PROJECT_DIR"

case "$PROFILE" in
    debug|release|strict)
        exec make "$PROFILE"
        ;;
    *)
        echo "Usage: $0 [debug|release|strict]" >&2
        exit 2
        ;;
esac
