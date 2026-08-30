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
    local test_name="$1"
    shift
    echo "=== ${test_name} ==="
    local srcs=()
    for src_name in "$@"; do
        srcs+=("src/${src_name}.cpp")
    done
    "$CXX" -std=c++17 -Wall -Wextra -Isrc -Itest \
        "test/${test_name}.cpp" "${srcs[@]}" \
        -o "$OUT_DIR/${test_name}"
    "$OUT_DIR/${test_name}" || status=1
    echo
}

run_test test_easing SCEX_Easing
run_test test_yaml SCEX_Yaml
run_test test_servo_axis SCEX_ServoAxis SCEX_Easing

exit $status
