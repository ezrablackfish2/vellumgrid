# VellumGrid

VellumGrid is a C++17 library for replaying offline document-edit jobs and producing deterministic page-composition plans. It models fonts, styles, segmented text, image tiles, paths, layers, annotations, transactions, color profiles, render scheduling, resource pressure, and page-breaking rules without requiring a GUI, database, network service, or external asset store.

## Repository layout

- `include/vellumgrid/` — public model and subsystem interfaces.
- `src/` — job ingestion, document state, composition, geometry, diagnostics, and rendering support.
- `fuzz/` — four libFuzzer entry points covering job execution, rendering, edit replay, and resource pressure.
- `corpus/` — valid stateful jobs used as mutation starting points.
- `.clusterfuzzlite/build.sh` — deterministic ClusterFuzzLite build entry point.
- `tests/` — smoke coverage for the binary job reader, geometry, constraints, pagination, and engine execution.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

For a local libFuzzer build with Clang:

```bash
./tools/build_local.sh
./out-local/document_job_fuzzer corpus
```

The ClusterFuzzLite script builds every `fuzz/*_fuzzer.cc` target into `$OUT`. It uses only files in the checkout and performs no package installation or network access.

## Binary job format

A job starts with the `VGD2` signature, version `2`, batch flags, a record count, and a nonce. Each record contains an opcode, flags, payload length, epoch, and opcode-specific payload. The reader rejects truncated or oversized records before dispatch. Valid records are applied in epoch order to a single in-memory document instance, which allows one input to exercise realistic edit lifecycles rather than isolated parsing helpers.

See `docs/job_format.md` and `docs/architecture.md` for the format and subsystem boundaries.

## Repository Reconstruction Notice

This repository was reconstructed from recovered local project files and available timestamp metadata after the original repository was lost. Commit dates reflect the recovered timestamp data where available. The history is intended to preserve the original project timeline as accurately as possible, not to fabricate or inflate contribution activity.

