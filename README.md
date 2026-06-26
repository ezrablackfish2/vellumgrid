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

