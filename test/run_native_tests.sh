#!/usr/bin/env bash
# Compiles and runs the host-side (platform-independent) unit tests for
# SCEX_Easing and SCEX_Yaml with the system g++/clang++ -- no PlatformIO/
# ESP-IDF toolchain required. See docs/porting_notes.md for why only these
# two modules are host-testable (everything else touches ESP-IDF drivers).
set -euo pipefail
cd "$(dirname "$0")/.."

CXX="${CXX:-c++}"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

status=0
run_test() {
    local test_name="$1" src_name="$2"
    echo "=== ${test_name} ==="
    "$CXX" -std=c++17 -Wall -Wextra -Isrc -Itest \
        "test/${test_name}.cpp" "src/${src_name}.cpp" \
        -o "$OUT_DIR/${test_name}"
    "$OUT_DIR/${test_name}" || status=1
    echo
}

run_test test_easing SCEX_Easing
run_test test_yaml SCEX_Yaml

exit $status
