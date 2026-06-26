#!/bin/bash -eu

ROOT="${SRC:-$(pwd)}"
OUT_DIR="${OUT:-$ROOT/out}"
WORK_DIR="${WORK:-/tmp/vellumgrid-build}"
CXX_BIN="${CXX:-clang++}"
AR_BIN="${AR:-ar}"

mkdir -p "$OUT_DIR" "$WORK_DIR/objects"
rm -f "$WORK_DIR/objects"/*.o "$WORK_DIR/libvellumgrid_runtime.a"

COMMON_FLAGS=( -std=c++17 -I"$ROOT/include" )
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
  "$CXX_BIN" $CXXFLAGS "${COMMON_FLAGS[@]}" \
    -c "$ROOT/src/$unit.cpp" \
    -o "$WORK_DIR/objects/$unit.o"
done

"$AR_BIN" rcs "$WORK_DIR/libvellumgrid_runtime.a" "$WORK_DIR"/objects/*.o

for harness in "$ROOT"/fuzz/*_fuzzer.cc; do
  name="$(basename "${harness%.cc}")"
  "$CXX_BIN" $CXXFLAGS "${COMMON_FLAGS[@]}" "$harness" \
    "$WORK_DIR/libvellumgrid_runtime.a" $LIB_FUZZING_ENGINE \
    -o "$OUT_DIR/$name"
done
