#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_DIR="$(cd "$TEST_DIR/../.." && pwd)"

BASELINE="${1:-}"
OPTIMIZED="${2:-$REPO_DIR/build/ramulator2}"

if [[ -z "$BASELINE" ]]; then
    echo "Usage: $0 /path/to/original/ramulator2 [optimized_binary]"
    exit 2
fi

BASELINE="$(realpath "$BASELINE")"
OPTIMIZED="$(realpath "$OPTIMIZED")"

for binary in "$BASELINE" "$OPTIMIZED"; do
    if [[ ! -x "$binary" ]]; then
        echo "ERROR: Executable not found: $binary"
        exit 2
    fi
done

python3 "$SCRIPT_DIR/prepare_configs.py"

OUT="$TEST_DIR/results/correctness"
mkdir -p "$OUT"

passed=0
failed=0

for name in small unique_smoke write_smoke \
            unique_long mixed_long write_long \
            rank1 rank4_fast two_core; do

    config="$TEST_DIR/generated/$name.yaml"

    if ! "$BASELINE" -f "$config" \
        > "$OUT/${name}_original.txt" \
        2> "$OUT/${name}_original.err"; then
        echo "$name: FAIL (baseline execution)"
        failed=$((failed + 1))
        continue
    fi

    if ! "$OPTIMIZED" -f "$config" \
        > "$OUT/${name}_optimized.txt" \
        2> "$OUT/${name}_optimized.err"; then
        echo "$name: FAIL (optimized execution)"
        failed=$((failed + 1))
        continue
    fi

    if cmp -s \
        "$OUT/${name}_original.txt" \
        "$OUT/${name}_optimized.txt"; then
        echo "$name: PASS"
        passed=$((passed + 1))
    else
        echo "$name: FAIL (output mismatch)"
        failed=$((failed + 1))
    fi
done

echo
echo "Correctness: $passed/9 PASS, $failed FAIL"

if [[ "$failed" -ne 0 ]]; then
    exit 1
fi
