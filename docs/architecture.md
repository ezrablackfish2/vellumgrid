# Architecture

`read_job_batch` owns structural validation. It never exposes a record payload beyond its declared bounds. `DocumentEngine` then enforces document activation and nondecreasing epochs before routing commands.

The state is divided by lifetime and responsibility:

- `FontBook`, `StyleCatalog`, and `TextStore` implement shaping inputs and segmented text edits.
- `PageBook` and `AnnotationMap` own page/layer topology and cross-object references.
- `ImageStore` and `PathStore` own raster and vector resources.
- `RenderPlanner`, `RenderCache`, and `RenderScheduler` build and retain derived rendering work.
- `TransactionManager`, `ProfileStore`, `Clipboard`, and `DependencyMap` model edit history and secondary relationships.
- `ResourcePool` represents compact typed handles used by pressure-sensitive render resources.
- `ConstraintSet`, `PaginationPolicy`, and `BaselineGrid` provide deterministic page composition utilities.

The four fuzz targets deliberately share the same production entry point but perform different post-replay work. This exposes failures that occur only during rendering, replay, or pressure cycles while keeping each harness small and auditable.
