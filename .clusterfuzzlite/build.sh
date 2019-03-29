#!/bin/bash -eu
ROOT="${SRC:-$(pwd)}"
OUT_DIR="${OUT:-$ROOT/out}"
WORK_DIR="${WORK:-/tmp/vellumgrid-build}"
CXX_BIN="${CXX:-clang++}"
AR_BIN="${AR:-ar}"
mkdir -p "$OUT_DIR" "$WORK_DIR/objects"
rm -f "$WORK_DIR/objects"/*.o "$WORK_DIR/libvellumgrid.a"
COMMON_FLAGS=( -std=c++17 -I"$ROOT/include" )
for source in "$ROOT"/src/*.cpp; do
  object="$WORK_DIR/objects/$(basename "${source%.cpp}").o"
  "$CXX_BIN" $CXXFLAGS "${COMMON_FLAGS[@]}" -c "$source" -o "$object"
done
"$AR_BIN" rcs "$WORK_DIR/libvellumgrid.a" "$WORK_DIR"/objects/*.o
for harness in "$ROOT"/fuzz/*_fuzzer.cc; do
  name="$(basename "${harness%.cc}")"
  "$CXX_BIN" $CXXFLAGS "${COMMON_FLAGS[@]}" "$harness" \
    "$WORK_DIR/libvellumgrid.a" $LIB_FUZZING_ENGINE -o "$OUT_DIR/$name"
done
