#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_DIR="$(cd "$TEST_DIR/../.." && pwd)"

BINARY="${1:-$REPO_DIR/build/ramulator2}"
BINARY="$(realpath "$BINARY")"

if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: Executable not found: $BINARY"
    exit 2
fi

if ! command -v valgrind >/dev/null 2>&1; then
    echo "ERROR: Valgrind is not installed."
    exit 2
fi

python3 "$SCRIPT_DIR/prepare_configs.py"

OUT="$TEST_DIR/results/valgrind"
mkdir -p "$OUT"

passed=0
failed=0

for name in \
    small unique_smoke write_smoke \
    unique_long mixed_long write_long \
    rank1 rank4_fast two_core
do
    config="$TEST_DIR/generated/${name}.yaml"
    log="$OUT/${name}.log"

    echo "Testing: $name"

    if valgrind \
        --quiet \
        --leak-check=full \
        --show-leak-kinds=all \
        --error-exitcode=97 \
        "$BINARY" -f "$config" \
        > "$OUT/${name}.stdout" \
        2> "$log"
    then
        echo "  VALGRIND: PASS"
        passed=$((passed + 1))
    else
        echo "  VALGRIND: FAIL"
        echo "  Log: $log"
        failed=$((failed + 1))
    fi
done

echo
echo "================================"
echo "Valgrind: $passed/9 PASS"
echo "Failures: $failed"
echo "================================"

if [[ "$failed" -ne 0 ]]; then
    exit 1
fi
