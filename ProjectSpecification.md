# LAR Packet Monitor project specification

## Purpose and status

LAR Packet Monitor is a desktop visualization and recording tool for mapped UDP
telemetry. It displays aircraft and target state in Grid, Earth, Plane, and DLZ
views; records the accepted datagrams in the `LAR1` format; and replays a saved
session deterministically. It is an engineering and teaching application, not a
certified flight, navigation, fire-control, or weapon-employment system.

This document defines the maintained product contract. Detailed implementation
and verification information lives in the [documentation hub](docs/README.md).

## Functional requirements

### Input and online operation

- The application shall load a bounded JSON mapping before accepting telemetry.
- Each accepted mapping entry shall identify a registered state field, an array
  index where applicable, a packet byte offset, and either a four-byte IEEE-754
  float or eight-byte IEEE-754 double representation.
- The UDP source shall support allow-all and exact-address whitelist policies.
  Neither policy constitutes authentication; deployments shall apply the
  controls in the [threat model](docs/THREAT_MODEL.md).
- A datagram shall update visible state only after mapping decode and domain
  validation both succeed. Fields absent from the mapping shall remain
  explicitly unavailable.
- Online publication shall be coalesced so rendering speed does not determine
  packet-ingestion throughput.

### Recording and playback

- Recording shall preserve the canonical mapping and the exact accepted UDP
  datagram boundaries in the versioned `LAR1` stream.
- Recording timestamps shall use active recording time; paused intervals shall
  not advance them, and adjacent records may have equal timestamps.
- A snapshot shall represent an immutable file prefix. Saving shall use atomic
  replacement and preserve an existing destination if commit fails.
- There is no product maximum for records per session. Capacity is governed by
  available storage, the `LAR1` byte layout, the supported duration, and the
  signed 64-bit application index range. The reader shall apply independent,
  explicit memory budgets: file sources no larger than 512 MiB may become an
  immutable resident snapshot, and a complete record-location index may occupy
  at most 128 MiB. It shall retain one timestamp-and-offset checkpoint per
  4,096 records and a single bounded location page as the fallback when the
  complete index budget is exceeded.
- Playback shall use exact millisecond timestamps, use the complete resident
  index when available or the bounded sparse fallback otherwise, and decode at
  most the selected record on each presentation tick. A changed sampled state
  and its exact position shall be published on that same tick.

### Visualization

- Packet latitude/longitude and attitude values use radians; distances and
  altitudes use metres unless the protocol reference states otherwise.
- Earth-zone boundaries shall use geodesic construction. The adaptive renderer's
  projected curve-error target is 0.65 pixel.
- The fixed-topology parametric GPU path may be selected only when its projected
  topology and float-coordinate error fit the same pixel budget. Numerically
  sensitive, polar, or extreme-zoom cases shall fall back to camera-relative,
  adaptively sampled CPU geometry.
- GPU resources shall be created, used, and destroyed only while their owning
  OpenGL context is current.
- Only the active viewport page shall consume each live scene-state update.
  Hidden pages shall synchronize from the latest retained state before they
  become visible.
- The DLZ view shall remain clearly identified as a deterministic fictional
  teaching model.

### Operator guidance and selection state

- The application shall provide searchable, resource-embedded help for Online,
  Offline, recording, replay, LAR, Plane, DLZ, protocol units, assets, and
  troubleshooting. Opening help shall not interrupt an active workflow.
- The Help control and the standard help shortcut shall reuse one non-modal
  window and provide context navigation for the current workflow or content.
- After a JSON mapping loads successfully, the Online panel shall display its
  filename, mapped-field count, minimum UDP packet size, and full-path tooltip.
  Cancellation or failure shall preserve the last successful presentation.
- Exclusive modes and persistent toggles shall retain a selected background.
  Application-driven recording and playback controls shall visibly indicate
  their active state, including a distinguishable muted state when disabled.

### Map, terrain, and Plane assets

- The Natural Earth source shall be compiled at build time into one bounded,
  versioned map package. The viewer shall not parse GeoJSON or link the source
  compiler at runtime.
- The current map package shall carry the projection meshes and a validated
  one-degree land-triangle index. Mercator, Sphere, and Plane terrain shall use
  that same immutable source rather than independent coastline data.
- Plane terrain shall accept directly addressed DTED Level 0, 1, and 2 cells,
  validate complete headers/dimensions/profile checksums, preserve no-data and
  signed elevation semantics, and keep its decoded tile cache bounded by both
  entry count and bytes.
- Land/water classification shall come from the packaged vector map, not the
  sign of DTED elevation. Classified water shall render at mean sea level while
  retaining negative bathymetry as non-negative depth for color; terrestrial
  depressions shall remain land.
- Terrain preparation shall be latest-only on a presentation-owned CPU worker.
  It shall publish immutable bounded patches and perform no QWidget or OpenGL
  work. Missing or invalid terrain shall produce a diagnostic and shall not be
  replaced by a misleading land-colored flat surface.
- A user-selected DTED Level 1 or 2 directory shall become active only after a
  contained addressed tile fully validates. Cancellation or failure shall
  retain the previous session source and patch.

### User-selected Plane models

- The Plane view may load glTF 2.0 JSON (`.gltf`) or binary (`.glb`) models from
  a supported mesh/material/texture subset.
- External buffer and image paths shall be relative regular files canonically
  contained below the selected model directory. Absolute paths, traversal,
  query/fragment syntax, NULs, and symlinks are rejected.
- Resource accounting shall be transactional and apply the exact CPU/GPU
  budgets documented in the [threat model](docs/THREAT_MODEL.md). A failed load
  shall retain the previously active model.
- The large DTED source tree and user-selected model/terrain data shall not be
  silently copied into build or install outputs. Deployment and redistribution
  shall preserve asset provenance and attribution.

## Architecture and lifecycle requirements

Source dependencies shall point inward:

```text
viewer/presentation -> application -> domain
infrastructure ------> application -> domain
tools/map_asset --------------------> map-format boundary
```

The direct and threaded runtimes shall implement the same command acceptance
and typed completion protocol. Threaded-runtime public operations are confined
to the runtime affinity thread. Shutdown shall synchronously stop workers, move
their QObject trees back to the runtime thread, join all worker threads, and
destroy the workers explicitly; worker lifetime shall not depend on deferred
deletion after a thread event loop exits.

Untrusted parsers shall validate the complete candidate before publishing it.
Mutable recording and persistence operations shall be transactional. Resource
limits belong beside the relevant parser or rendering boundary and require
adversarial tests.

## Dependency and packaging requirements

- Build requirements are CMake 3.24+, C++17, Qt 6.10.3 or newer with Core,
  Concurrent, Gui, Network, Widgets, and OpenGLWidgets, and native OpenGL
  development/runtime support. Test builds also require Qt Test; quality and
  installation checks require Python 3.
- Automated release jobs that install Qt are pinned to Qt 6.11.2. Changing that
  pin requires strict, sanitizer, GPU, installation, and dependency-scan
  evidence.
- Windows installations shall use an absolute CMake installation prefix and
  deploy the required Qt runtime DLLs and platform plugins beside the installed
  application layout.
- The installed SPDX 2.3 SBOM shall identify the application, each directly
  used Qt module, the platform plugin, PNG/JPEG image handlers, and the C++
  runtime, including exact versions, package URLs where defined, and dependency
  relationships.
- Only the PNG and JPEG image formats required by the glTF contract may be
  selected for user model textures.

## Security and operational constraints

The application does not authenticate UDP telemetry or cryptographically sign
session files. A source whitelist is routing policy, not identity proof. Use an
authenticated tunnel or isolated trusted network when telemetry integrity
matters, and treat `.lar`, mapping, glTF, map, terrain, and image data as
untrusted inputs. See the [threat model](docs/THREAT_MODEL.md) for assets,
boundaries, abuse cases, mitigations, residual risks, and release review rules.

## Acceptance criteria

A release candidate is acceptable only when:

1. GCC, Clang, AppleClang, and MSVC evidence selected for the release compiles
   with warnings-as-errors and the supported strict diagnostics. The current
   checked-in workflow supplies Windows MSVC evidence only; the other preset
   capabilities require separate execution until their workflows are restored.
2. Deterministic tests, architecture checks, documentation semantic/link/API
   gates, ASan/UBSan, TSan contracts, native GPU accuracy tests, installation
   smoke, and the SPDX dependency scan pass as specified in
   [Quality gates](docs/QUALITY_GATES.md).
3. Parser resource limits, session paging, runtime teardown, and extreme-zoom
   fallback retain explicit regression tests.
4. Known residual risks are documented here or in the threat model and are not
   contradicted by the UI or deployment guidance.
