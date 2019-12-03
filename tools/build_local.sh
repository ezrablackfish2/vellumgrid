#!/bin/bash -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${1:-$ROOT/out-local}"
BUILD_DIR="$OUT_DIR/.objects"
mkdir -p "$OUT_DIR" "$BUILD_DIR"
rm -f "$BUILD_DIR"/*.o "$BUILD_DIR/libvellumgrid.a"
CXX_BIN="${CXX:-clang++}"
AR_BIN="${AR:-ar}"
COMPILE_FLAGS=( -std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined,fuzzer-no-link -I"$ROOT/include" )
LINK_FLAGS=( -std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined,fuzzer -I"$ROOT/include" )
for source in "$ROOT"/src/*.cpp; do
  object="$BUILD_DIR/$(basename "${source%.cpp}").o"
  "$CXX_BIN" "${COMPILE_FLAGS[@]}" -c "$source" -o "$object"
done
"$AR_BIN" rcs "$BUILD_DIR/libvellumgrid.a" "$BUILD_DIR"/*.o
for harness in "$ROOT"/fuzz/*_fuzzer.cc; do
  name="$(basename "${harness%.cc}")"
  "$CXX_BIN" "${LINK_FLAGS[@]}" "$harness" "$BUILD_DIR/libvellumgrid.a" -o "$OUT_DIR/$name"
done
