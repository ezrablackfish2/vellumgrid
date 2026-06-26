# Document service layer

VellumGrid separates the stateful replay engine from the catalogs used by importers, editors, and exporters. The replay engine remains small enough to fuzz repeatedly, while the complete CMake build compiles the full service layer.

The catalog services cover document metadata, master pages, binary assets, font families, style rules, export presets, workspaces, locales, plugins, security policies, templates, and document properties. Every service supports stable IDs, revision tracking, typed metrics, dependency links, normalized attributes, graph traversal, validation, deterministic checksums, snapshots, restoration, merging, and structural diffs.

Catalog implementations are first-party C++ and live in `src/`; their interfaces are under `include/vellumgrid/catalog/`. They do not require a database or network service. This keeps offline batch conversion deterministic and allows the same library to be embedded in desktop, server, and test tools.

The ClusterFuzzLite targets intentionally link the replay-critical runtime units only. The ordinary CMake build compiles every source file and the catalog test suite links all catalog modules. This mirrors production systems where fuzz targets isolate a security boundary without omitting the rest of the repository from normal builds.
