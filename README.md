# VellumGrid

VellumGrid is an embeddable C++17 engine for validating and deterministically replaying document-edit workloads. It is intended for publishing tools, conversion services, desktop editors, and CI systems that need to reproduce the same page state from the same ordered set of edits without depending on a GUI, database, network service, or platform-specific rendering stack.

A VellumGrid workload can create a document, register fonts and styles, add pages and layers, edit text, attach images and vector paths, update color profiles, schedule render work, and finalize the resulting document state. The engine produces a stable digest for the completed workload, which makes it useful for regression testing, import/export verification, crash reproduction, and deterministic batch processing.

## Why this project exists

Document applications often apply hundreds of related mutations before a page is ready to export. Reproducing a production problem is difficult when those mutations are tied to a GUI session or an online service. VellumGrid separates the mutation log from the user interface so a document workflow can be saved, validated, replayed, inspected, and compared in an offline process.

Typical uses include:

- replaying an editor session in a bug report;
- validating imported document operations before applying them;
- checking that a layout or conversion change preserves the final document digest;
- exercising render planning under normal and resource-constrained modes;
- running deterministic document transformations in CI or a batch worker.

## Command-line utility

The normal CMake build produces a `vellumgrid` executable in addition to the static library.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Validate a workload without executing it:

```bash
./build/vellumgrid validate corpus/booklet_typography.bin
```

Inspect its metadata and operation counts:

```bash
./build/vellumgrid inspect corpus/booklet_typography.bin
```

Replay it and print the deterministic result digest:

```bash
./build/vellumgrid run corpus/booklet_typography.bin normal
./build/vellumgrid run corpus/scheduled_pages.bin render
./build/vellumgrid run corpus/clipboard_roundtrip.bin replay
./build/vellumgrid run corpus/image_contact_sheet.bin pressure
```

The four execution modes use the same validated workload but finish with different production operations:

- `normal` applies the edits and returns the document-state digest;
- `render` builds and evaluates pending render work;
- `replay` processes deferred edit and transaction state;
- `pressure` performs resource-pressure cleanup and reuse cycles.

## Library usage

Applications can parse and execute a workload directly:

```cpp
#include "vellumgrid/engine.h"
#include "vellumgrid/job_format.h"

#include <cstdint>
#include <vector>

uint64_t replay(const std::vector<uint8_t>& bytes) {
  auto job = vellumgrid::read_job_batch(bytes.data(), bytes.size());
  if (!job) {
    throw std::runtime_error("invalid VellumGrid workload");
  }
  return vellumgrid::run_job(*job, 0);
}
```

`read_job_batch` performs structural validation before commands reach the document engine. Callers retain control of file storage, authentication, and transport; VellumGrid only handles the in-memory document workflow.

## Workload format

The current on-disk format is **VGD2**. It contains a versioned batch header followed by ordered records. Each record stores an operation, flags, an epoch, and a bounded operation-specific payload. Epoch ordering lets the engine model multi-step editing sessions while preserving deterministic replay.

The format is deliberately compact because workloads are commonly attached to bug reports, stored as regression fixtures, or passed between an importer and a rendering worker. See [`docs/job_format.md`](docs/job_format.md) for the binary layout and supported operations.

## Architecture

The code is divided by document responsibility rather than by input record type:

- **Parsing and validation** — `byte_cursor.cpp` and `job_format.cpp` decode bounded VGD2 records.
- **Document execution** — `engine.cpp` applies validated commands and enforces document and epoch state.
- **Editing subsystems** — text, style, page, layer, image, path, annotation, transaction, clipboard, and dependency state live in `subsystems.cpp`.
- **Composition** — `layout_math.cpp` and `composition_rules.cpp` provide deterministic pagination and geometry helpers.
- **Document services** — the remaining `src/` modules manage reusable metadata, templates, assets, export presets, workspaces, locales, policy, and plugin records used by importers and exporters.
- **Diagnostics** — `diagnostics.cpp` exposes stable summaries suitable for tests and command-line tools.

The public headers are under `include/vellumgrid/`. The engine has no mandatory third-party runtime dependency.

## Repository layout

```text
include/vellumgrid/   Public C++ interfaces
src/                  Engine and document-service implementations
tools/                User-facing command-line utility
corpus/               Example VGD2 workloads and regression fixtures
tests/                Unit and integration smoke tests
docs/                 Architecture and workload-format documentation
fuzz/                 Security-oriented test drivers
.clusterfuzzlite/     Reproducible sanitizer build configuration
```

## Testing

```bash
ctest --test-dir build --output-on-failure
```

The tests cover structural parsing, core composition rules, catalog behavior, engine replay, and the public CLI. Files in `corpus/` are valid example workloads and may also be used as regression fixtures by downstream applications.

## Security testing

Document workloads can originate from importers or external systems, so parser and lifecycle safety are part of the project’s test strategy. The `fuzz/` directory contains separate libFuzzer drivers for document execution, render completion, deferred replay, and resource-pressure behavior. These are development tools; the library and CLI are the project’s user-facing products.

The ClusterFuzzLite-compatible build is deterministic and uses only files already present in the checkout:

```bash
./tools/build_local.sh
```

## Limitations

VellumGrid is not a graphical editor and does not rasterize final PDF or image output. It models document state, composition decisions, resource ownership, and render planning. A host application is responsible for UI, persistent storage, font discovery, image decoding, and final export.
