#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# build.sh  —  Build the Fortran COMMON Block Analyzer (Part 1)
#
# Usage:
#   ./build.sh             → Release build
#   ./build.sh debug       → Debug build with symbols (-g -O0)
#   ./build.sh clean       → Remove build/ directory
# ─────────────────────────────────────────────────────────────────────────────
set -e

BUILD_DIR="build"
SRC_DIR="src"
INCLUDE_DIR="include"
CXX="${CXX:-g++}"
STD="-std=c++17"

# ── Flags ────────────────────────────────────────────────────────────────────
COMMON_FLAGS="$STD -Wall -Wextra -Wno-unused-parameter -I$INCLUDE_DIR"

if [[ "$1" == "debug" ]]; then
    OPT_FLAGS="-g -O0 -DDEBUG"
    echo "[build.sh] Mode: DEBUG"
elif [[ "$1" == "clean" ]]; then
    echo "[build.sh] Cleaning build/"
    rm -rf "$BUILD_DIR"
    exit 0
else
    OPT_FLAGS="-O2 -DNDEBUG"
    echo "[build.sh] Mode: RELEASE"
fi

mkdir -p "$BUILD_DIR"

# ── Build collector (single file processor) ──────────────────────────────────
echo "[build.sh] Compiling collector..."
$CXX $COMMON_FLAGS $OPT_FLAGS \
    -o "$BUILD_DIR/collector" \
    "$SRC_DIR/collector.cpp"
echo "[build.sh] ✓ build/collector"

# ── Build batch_collect (directory processor) ─────────────────────────────────
echo "[build.sh] Compiling batch_collect..."
$CXX $COMMON_FLAGS $OPT_FLAGS \
    -o "$BUILD_DIR/batch_collect" \
    "$SRC_DIR/batch_collect.cpp"
echo "[build.sh] ✓ build/batch_collect"

echo ""
echo "=== Build complete ==="
echo ""
echo "Usage examples:"
echo "  ./build/collector tests/fortran_samples/test01_file_a.f90 outputs/test01_a.json"
echo "  ./build/batch_collect tests/fortran_samples/ outputs/"
echo ""
echo "With custom Flang path:"
echo "  FLANG_PATH=/usr/bin/flang-18 ./build/collector myfile.f90"
echo ""
