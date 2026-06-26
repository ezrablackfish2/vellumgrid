#!/bin/bash -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${1:-$ROOT/out-local}"
BUILD_DIR="$OUT_DIR/.objects"
mkdir -p "$OUT_DIR" "$BUILD_DIR"
rm -f "$BUILD_DIR"/*.o "$BUILD_DIR/libvellumgrid_runtime.a"
CXX_BIN="${CXX:-clang++}"
AR_BIN="${AR:-ar}"
COMPILE_FLAGS=( -std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined,fuzzer-no-link -I"$ROOT/include" )
LINK_FLAGS=( -std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined,fuzzer -I"$ROOT/include" )
RUNTIME_SOURCES=(
  byte_cursor
  job_format
  diagnostics
  layout_math
  composition_rules
  subsystems
  engine
)
for unit in "${RUNTIME_SOURCES[@]}"; do
  "$CXX_BIN" "${COMPILE_FLAGS[@]}" -c "$ROOT/src/$unit.cpp" -o "$BUILD_DIR/$unit.o"
done
"$AR_BIN" rcs "$BUILD_DIR/libvellumgrid_runtime.a" "$BUILD_DIR"/*.o
for harness in "$ROOT"/fuzz/*_fuzzer.cc; do
  name="$(basename "${harness%.cc}")"
  "$CXX_BIN" "${LINK_FLAGS[@]}" "$harness" "$BUILD_DIR/libvellumgrid_runtime.a" -o "$OUT_DIR/$name"
done
