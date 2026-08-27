# Developer guide

## Prerequisites

- CMake 3.24 or newer
- C++17 compiler
- Qt 6.10.3 or newer: Core, Concurrent, Gui, Network, Widgets,
  OpenGLWidgets, and Test
- OpenGL development/runtime support
- Ninja for Unix presets; Visual Studio 2022 with MSVC v143 for Windows presets
- Python 3 for repository gates
- Optional: Doxygen and Graphviz, clang-format, clang-tidy, gcovr, Mull

The checked-in workflow currently verifies a strict Windows/MSVC release build
with Qt 6.11.2. GCC, Clang, AppleClang, sanitizers, coverage, fuzzing, mutation,
and GPU tests remain configured local/preset capabilities, but they are not all
automated by the current workflow. Treat compiler/Qt pins as security posture
decisions; review the [threat model](THREAT_MODEL.md) and regenerate/scan the
componentized SBOM when changing either.

## Configure, build, and test

```bash
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev --output-on-failure
```

Release and strict builds use the corresponding preset:

```bash
cmake --preset release
cmake --build --preset release --parallel
ctest --preset release --output-on-failure

cmake --preset ci-clang
cmake --build --preset ci-clang --parallel
ctest --preset ci-clang --output-on-failure
```

Do not edit a `build-*` tree or generated `qrc_`, MOC, map, manifest, SBOM, or
Doxygen output. Reconfigure from maintained source.

## Windows MSVC workflow

The Windows presets target Visual Studio 2022 x64. Install Visual Studio 2022
or Build Tools with **Desktop development with C++**, MSVC v143, and a Windows
10 or 11 SDK. Install Qt's **MSVC 2022 64-bit** kit; MinGW, LLVM-MinGW, Android,
WebAssembly, and ARM64 kits are not interchangeable with this preset.

A typical minimum-version kit is
`C:\Qt\6.10.3\msvc2022_64`. Confirm the CMake package before configuring:

```powershell
Test-Path "C:\Qt\6.10.3\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake"
```

If the location is unknown:

```powershell
Get-ChildItem C:\Qt -Recurse -Filter Qt6Config.cmake -ErrorAction SilentlyContinue
```

Two independent settings are required:

- `Qt6_DIR` lets CMake locate Qt while configuring.
- The kit's `bin` directory on `PATH` lets build-time Qt executables start.

Setting only `Qt6_DIR` is insufficient when an MSBuild custom step launches a
Qt-linked helper. Configure and build a production release from Developer
PowerShell as follows:

```powershell
$env:Path = "C:\Qt\6.10.3\msvc2022_64\bin;$env:Path"

cmake --preset windows-release `
  -DQt6_DIR=C:/Qt/6.10.3/msvc2022_64/lib/cmake/Qt6

cmake --build --preset windows-release --parallel
```

Substitute another compatible version consistently in both paths. The
`windows-release` preset creates `build-windows-release`, selects Visual Studio
2022 x64, and disables tests and tool-based quality targets.

### Install a deployable Windows package

Qt's generated deployment script requires an absolute installation prefix.
Construct it from PowerShell's current absolute path instead of passing a bare
relative `--prefix`:

```powershell
$packageDir = Join-Path $PWD.Path "build-windows-package"

cmake --install build-windows-release `
  --config Release `
  --prefix "$packageDir"

.\build-windows-package\bin\lar-viewer.exe
```

The install step copies the executable, map package, Plane assets, mappings,
and required Qt runtime DLLs/plugins into the package layout. DTED trees remain
external by design.

### Run strict Windows tests

Use `ci-msvc` rather than `windows-release` when tests and warnings-as-errors
are required:

```powershell
$env:Path = "C:\Qt\6.10.3\msvc2022_64\bin;$env:Path"

cmake --preset ci-msvc `
  -DQt6_DIR=C:/Qt/6.10.3/msvc2022_64/lib/cmake/Qt6

cmake --build --preset ci-msvc --parallel
ctest --preset ci-msvc
```

### Diagnose Windows build failures

| Symptom | Cause and action |
| --- | --- |
| CMake cannot find `Qt6Config.cmake` | Verify the actual `msvc2022_64` package and pass its `lib/cmake/Qt6` directory as `Qt6_DIR`. A Qt version directory may exist without that kit. |
| MSBuild custom step exits `-1073741515` | A required runtime DLL was not found. Add the matching Qt `bin` directory to the current PowerShell `PATH`, then rebuild. |
| Linker reports `__imp_glViewport` or another native GL symbol | Reconfigure with the maintained CMake files. `lar-presentation-widgets` must publicly link the portable `OpenGL::GL` target so its static-library dependency reaches the executable. |
| Install/deploy rejects a relative `qt.conf` path | Use an absolute prefix such as `$packageDir = Join-Path $PWD.Path "build-windows-package"`. |

Use `cmake --build --preset windows-release --verbose` to expose the failing
custom or link command. The complete prerequisite installation and clean
package-recreation commands are also kept in the root [README](../README.md).

## Repository checks

After configuring a build that has the required tools:

```bash
cmake --build build-dev --target check-format check-architecture \
  check-doc-links check-doc-coverage check-doc-quality check-docs
cmake --build build-dev --target check-tidy
```

On a machine without Doxygen, run all portable checks directly:

```bash
python3 tools/check_architecture.py .
python3 tools/check_doc_links.py .
python3 tools/check_doxygen_coverage.py .
python3 tools/check_doc_quality.py .
```

See [Quality gates](QUALITY_GATES.md) for sanitizer, coverage, fuzz, mutation,
performance, install, and soak workflows.

## Read before changing structure

1. [Architecture](ARCHITECTURE.md)
2. [SOLID compliance](SOLID_COMPLIANCE.md)
3. [Concurrency model](CONCURRENCY_MODEL.md) for runtime or viewport workers
4. [Threat model](THREAT_MODEL.md) for trust boundaries and resource limits
5. [Protocol units](PROTOCOL_UNITS.md) for mapping/state changes
6. [LAR1 format](LAR1_FORMAT.md) for session changes
7. [Visualization guide](VISUALIZATION.md) for coordinates, cameras, and GPU work
8. [Terrain and assets](TERRAIN_AND_ASSETS.md) for map, DTED, glTF, and cubemap work

The dependency rule is inward-only. Domain and application code must not solve
a short-term problem by constructing a concrete infrastructure or widget type.

## Add a packet field

Packet fields are registry changes and must be atomic across the stack:

1. Add storage to the appropriate domain envelope. Avoid changing the fixed
   `Plane`/`Target` layout unless the protocol requirement truly belongs there.
2. Add a stable `StateField::Id` before `Count`.
3. Add exactly one descriptor in `statefield.cpp`: mapping name/index, member
   name, presentation name, unit, validation category, getter, and setter.
4. Add scalar and cross-field validation if needed.
5. Update mapping completeness rules when several fields form an atomic group.
6. Add formatting/presentation behavior that respects `availableFields`.
7. Add or update sample mappings and sender scenarios.
8. Extend domain, mapping, sender, recording/replay, and UI tests.
9. Update `PROTOCOL_UNITS.md`, `COMPONENT_REFERENCE.md`, and generated API
   comments.

Compile-time assertions verify that every ID has a complete, unique descriptor.
Do not duplicate mapping-name switches in adapters or widgets.

## Add an application use case

1. Identify the single policy owner and keep the service focused.
2. Define the smallest application-owned port for required side effects.
3. Express inputs/outputs as validated domain or application values.
4. Inject ports through the constructor; do not include infrastructure headers.
5. Implement a concrete adapter in `src/infrastructure`.
6. Wire it in `main.cpp`, a runtime worker factory, or a test composition root.
7. Test the service with a fake and the adapter against its behavioral contract.
8. Run the architecture gate to verify include and CMake direction.

If the new command joins `IApplicationRuntime`, place it in the narrowest
command slice, add a request-correlated result, register queued metatypes, and
implement identical direct/threaded observable behavior.

## Add or replace an adapter

An adapter must honor more than its signature. Document and test:

- ownership and thread affinity;
- whether calls are idempotent;
- whether output is transactional on failure;
- resource and input bounds;
- atomicity/durability expectations;
- synchronous versus asynchronous completion;
- shutdown behavior.

Use constructor injection or a typed factory. A null test/production factory
must either be rejected deterministically or replaced with the documented inert
fallback and a construction diagnostic.

## Add a viewport page

1. Implement `ILarViewportPage`; use `IEarthLarViewportPage` only for
   map-specific capabilities.
2. Keep packet and application lifecycle logic outside the page. Consume
   `LarSceneState` and camera commands.
3. Extract projection/tracking/fit math into pure policy helpers where practical.
4. Bound untrusted geometry before allocation and keep OpenGL ownership on the
   context thread.
5. Wire selection in `LarViewport`, not in application services.
6. Add page, camera, worker, render-validation, and optional native GPU tests.

Keep tracked sphere centers as doubles through `MapCamera` and marker
projection. Sphere shaders receive the center as compensated high/low float
parts plus precomputed latitude sine/cosine; do not collapse the center back to
one float uniform, because that reintroduces close-zoom replay jitter.

## Change Plane visualization assets or rendering

- Keep the packaged runtime aircraft model under `assets/models` as the glTF
  2.0 binary `f16_3.glb`. The Plane UI also accepts user-uploaded `.gltf` JSON
  and `.glb` binary models for the current session; uploaded files are not
  staged or installed as application assets. Six-face cubemap sets remain
  under `assets/cubemaps` as PNG files.
- Keep DTED Level-0 cells under `assets/DTED0/{e|w}DDD/{n|s}DD.dt0`, or point
  `LAR_DTED0_ROOT` at an external tree. Do not add the 1.7 GB tree to the normal
  post-build copy or install path. User-selected Level-1/Level-2 trees use the
  same degree-directory convention with `.dt1`/`.dt2`, are accessed in place,
  and remain session-only.
- Preserve signed DTED elevation. Land/sea classification comes from the
  version-2 `.larmap` one-degree `MapLandIndex`, not from elevation sign or a
  separate water-mask pack. If coastline source data changes, rebuild the map
  asset and verify Earth/Plane classification together.
- The glTF reader accepts a bounded mesh/material/node subset with float
  `TEXCOORD_0`, data-URI or local external buffer resources, PNG/JPEG
  base-color images, and validated sampler state. The upload button is owned by
  `PlaneViewWorkspace` and delegates parsing/loading to
  `PlaneSceneWidget::loadModelFromFile`; a failed load retains the active model.
  Extend reader validation and tests before relying on another glTF feature.
- A cubemap set must use one shared filename prefix with `_rt`, `_lf`, `_up`,
  `_dn`, `_ft`, and `_bk` suffixes. All six images must have the same square
  dimensions. Preserve sortable prefixes so the Skybox action remains
  deterministic.
- Keep telemetry/model axis conversion in `PlaneAttitudeTransform` and orbit
  math in `PlaneOrbitCamera`; neither belongs in widget event handlers.
- Keep the 15 m F-16 surface-unit conversion in `PlaneAircraftScale`. The GLB
  reader must retain its normalized forward extent, the renderer must leave the
  model at scale 1 in both modes, and surface geometry must expand around it.
- Keep local geographic conversion, stable metric scaling, and grid spacing in
  `PlaneSurfaceProjection`, including telemetry-altitude placement,
  altitude/content-derived ground bounds, the fixed-origin grid phase, and
  IZ-first/IR-fallback bounded target-pyramid sizing.
  `PlaneSurfaceGpuLayer` owns only static topology, shader uniforms, draw order,
  and context-bound resources; it must not read protocol state directly.
- Match `GridLarView` ground anchoring: `PlaneSceneWidget` captures the first
  valid aircraft latitude/longitude after reset, while projection converts that
  persistent origin into aircraft-relative grid and unique-axis offsets.
- The optional surface uses east `+X`, up `+Y`, and north `-Z`. Preserve the
  opaque-ground then grid/fills/outlines/target/aircraft order so translucent
  LAR areas cannot make the surface appear transparent. Surface overlays draw
  in explicit painter order to avoid long-range depth precision artifacts. The
  target shader scales the pyramid footprint and height together.
- Keep all OpenGL resource creation, upload, draw, and destruction on the
  `PlaneSceneWidget` context thread.
- Keep DTED parsing, caching, interpolation, and patch construction in the
  latest-only Plane terrain worker. Readers must preserve level-specific
  count/file/spacing bounds, checksum validation, signed-magnitude/no-data
  handling, streaming profile reads, and variable polar profile widths.
  Terrain caches must retain both entry-count and byte limits. Land-mask
  construction must query bounded `MapLandIndex` candidates, retain adaptive
  raster limits, and classify negative land elevations as land. Workers publish
  immutable patches only; they never access a widget or OpenGL object.
- Keep the terrain patch aircraft-centered in east `+X`, up `+Y`, north `-Z`.
  Reuse it within the documented recenter threshold, retain bounded positive
  and negative tile cache entries, and do not draw a land-coloured flat fallback
  while terrain is the selected source but unavailable. Classified water
  geometry stays at mean sea level while its source negative elevation travels
  as shader depth; use the vector land mask so negative terrestrial depressions
  remain terrain. Tactical zones retain their coherent local presentation
  plane, while the target marker samples terrain height at its own coordinate.
- Treat `Plane.location[2]` as MSL-compatible metres when placing DTED terrain.
  If it is unavailable, preserve only the surface's default visual clearance;
  do not label that fallback as a recovered absolute altitude.
- Update `lar_copy_plane_assets`, install rules, attribution, parser/widget
  tests, and the opt-in native GPU test with any asset-format change.

## Change the LAR1 format

Read [LAR1 session format](LAR1_FORMAT.md) first. Changes to byte layout,
endianness, framing, or meaning are incompatible and require a new magic/version
and parser branch. Add corruption, old-version, snapshot, persistence, exact
replay, and fuzz coverage. Never make an older byte sequence silently mean
something new.

## Change the DLZ model

The exact fictional equations and renderer acceptance contract are in
[DLZ model](DLZ_MODEL.md). Keep raw diagnostic evaluation separate from the
guarded `solve()` result. A change must preserve value-or-error behavior and
strict ordering or explicitly version the teaching behavior and update tests,
sender scenarios, and docs.

## Map source and package changes

- Source GeoJSON belongs only under `assets/map`.
- Parsing and compilation belong in `tools/map_asset`.
- Runtime package parsing belongs in widget-free `lar-map-format`.
- OpenGL/camera behavior belongs in `lar-map-rendering` or viewport layers.
- Keep counts, bytes, coordinates, indices, and triangulation bounded.
- Preserve the version-2 360-by-180 land-triangle index; it is the common
  runtime coastline authority for Earth and Plane terrain.
- Update compiler, asset reader, map loading, and renderer validation tests.

The viewer must not acquire a GeoJSON parser or link the map compiler support
library.

## Doxygen style

Every production header requires one file block immediately after
`#pragma once`:

```cpp
/**
 * @file example_service.h
 * @brief Application use case for one concrete responsibility.
 */
```

Document every public type and externally meaningful operation. Also document
private/internal functions when their contract, units, ownership, threading,
failure behavior, or algorithm is not obvious. A useful block states the
contract, inputs/outputs, and return behavior when applicable:

```cpp
/**
 * @brief Parses one bounded encoded value.
 *
 * @details Replaces @p output only after the complete input validates.
 *
 * @param[in] input Untrusted encoded bytes bounded by the caller.
 * @param[out] output Destination retained unchanged on failure.
 * @param[out] error Optional human-readable diagnostic.
 *
 * @return True when a complete value was committed; false otherwise.
 */
bool parse(const QByteArray &input, Value *output, QString *error = nullptr);
```

Prefer comments that state units, ownership, invariants, failure effects,
threading, or rationale. Do not narrate obvious syntax. Implementation comments
should explain why ordering, bounds, or an unusual branch is required.

Run `check-doc-coverage` and `check-docs`. Keep this Markdown reference and
source comments aligned; Doxygen extracts the complete code surface.

To generate the configured website explicitly:

```bash
cmake --build build-release --target check-docs
open build-release/docs/html/index.html       # macOS
# xdg-open build-release/docs/html/index.html # Linux
```

The repository-root `Doxyfile` is also self-contained and writes `html/` when
run as `doxygen Doxyfile`. Both configurations treat warnings as errors.

## Test selection

Useful focused commands:

```bash
# Core and architecture
ctest --test-dir build-release --output-on-failure \
  -R 'domain|architecture|runtime-contract|source-lifecycle|request-gate'

# Recording and session
ctest --test-dir build-release --output-on-failure \
  -R 'recording-pipeline|session-adapter|session-time|playback-service'

# Map and viewport
ctest --test-dir build-release --output-on-failure \
  -R 'map|plane|zone|camera|viewport'

# Offscreen UI and DLZ
QT_QPA_PLATFORM=offscreen ctest --test-dir build-release --output-on-failure \
  -R 'viewer|dlz-view'
```

Run the full deterministic suite before handoff. The native GPU tests return
the configured skip code unless their `LAR_RUN_MAP_GPU_TESTS=1` or
`LAR_RUN_PLANE_GPU_TESTS=1` opt-in and a usable context are available.

## Safe-change checklist

- Behavior and units match the project specification.
- No source or CMake dependency points outward across a layer boundary.
- A changed class still has one reason to change.
- Adapter failures preserve prior valid output/data where promised.
- New cross-thread values are copyable/immutable and registered.
- Request IDs and source epochs are propagated and checked.
- Input sizes, counts, times, and geometry remain bounded.
- Cancellation, retry, and shutdown paths are tested.
- Public headers and non-obvious invariants have Doxygen comments.
- Markdown links pass; diagrams and file/component references are updated.
- Focused and full builds/tests pass under an appropriate preset.
