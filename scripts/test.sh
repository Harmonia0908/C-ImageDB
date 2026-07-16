#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SUITE="${1:-all}"

if [ "$#" -gt 1 ]; then
    echo "Usage: $0 [all|unit|integration|benchmark|sanitizer]" >&2
    exit 2
fi

cd "$PROJECT_DIR"

case "$SUITE" in
    all)
        exec make test
        ;;
    unit)
        exec make test-unit
        ;;
    integration)
        exec make test-integration
        ;;
    benchmark)
        exec make benchmark-test
        ;;
    sanitizer)
        exec make sanitizer-test
        ;;
    *)
        echo "Usage: $0 [all|unit|integration|benchmark|sanitizer]" >&2
        exit 2
        ;;
esac
