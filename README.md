# LAR Packet Monitor

Qt 6 and C++17 desktop application for mapped UDP reception, LAR and 3D aircraft
visualization, raw-value monitoring, `.lar` session recording, and offline
replay. The packet-monitor domain model is the `Plane` and `Target` structures
in `src/domain/state.h`; the isolated DLZ teaching model is documented below.

The complete implementation guide starts at the
[documentation hub](docs/README.md), with architecture and UML, SOLID evidence,
component/API and file references, runtime sequences, concurrency, protocols,
format details, developer workflows, and quality gates.

## Architecture

The active source tree has one ports-and-adapters architecture:

```text
presentation -> application <- infrastructure
                       |
                       v
                     domain
```

- `src/domain` owns state, field identity, validation, and immutable packet mappings.
- `src/application` owns use-case services, lifecycle state, runtime contracts, and the recording pipeline state machine.
- `src/infrastructure` owns Qt UDP, JSON, clocks, session files, persistence, worker hosts, and thread wiring.
- `src/viewer` owns Grid, Mercator, spherical, and third-person Plane
  presentation, camera policy, GPU layers, and packaged visual assets.
- `src/domain/dlz` and `src/viewer/hud` contain the fictional DLZ teaching
  model and its QWidget/QPainter presentation. The three dedicated DLZ input
  fields are mapped atomically without reinterpreting ground-LAR `Target`
  fields.
- `tools/map_asset` owns the build-time world-boundary compiler and is not
  linked into the viewer.

The production runtime has network, session, and persistence worker threads in
addition to the UI thread. The direct runtime implements the same
command-acceptance/completion contract for deterministic tests. See the
[project specification](ProjectSpecification.md),
[architecture](docs/ARCHITECTURE.md),
[SOLID compliance record](docs/SOLID_COMPLIANCE.md), fixed
[protocol unit contract](docs/PROTOCOL_UNITS.md),
[concurrency model](docs/CONCURRENCY_MODEL.md), exact
[`LAR1` format](docs/LAR1_FORMAT.md),
[threat model](docs/THREAT_MODEL.md),
[map asset guide](assets/map/README.md), and current
[quality gates](docs/QUALITY_GATES.md).

## Build

Requirements on every platform: CMake 3.24+, Qt 6.10.3+ with Core, Concurrent,
Gui, Network, Widgets, OpenGL, and OpenGLWidgets, an OpenGL-capable system, and
a C++17 compiler. The Qt kit must match the target platform and architecture.
Presets with `BUILD_TESTING=ON` additionally require Qt Test. Presets with
`LAR_ENABLE_QUALITY_TARGETS=ON` require Python 3; this is the default for the
Unix release/developer/CI presets and for `ci-msvc`, while `windows-release`
disables both options. The Unix presets use Ninja; the Windows presets use
Visual Studio 2022 and its x64 MSVC toolchain.

Platform prerequisites:

- Windows: Windows 10 or 11 x64; Visual Studio 2022 or Visual Studio 2022
  Build Tools with the **Desktop development with C++** workload, MSVC v143,
  and a Windows 10/11 SDK. Install the Qt `msvc2022_64` kit. A Developer
  PowerShell or Native Tools command prompt for VS 2022 is recommended. Ninja
  is not required by the Windows presets.
- macOS: macOS 15 for the release-certified matrix; Xcode 15 or newer with
  AppleClang, the macOS SDK, and Command Line Tools. Install a Qt macOS kit
  matching the machine architecture (`arm64` or `x86_64`) and make Ninja
  available on `PATH`.
- Linux: Ubuntu 24.04 for the release-certified matrix, or a Linux system
  with a supported GCC or Clang C++17 toolchain. Install Ninja, Python 3, the
  Qt 6.10.3+ Linux development kit, and the system OpenGL plus Qt platform
  plugin dependencies required by that kit. Install both GCC and Clang when
  running the `ci-gcc` and `ci-clang` presets.

For the optional full verification targets, also install the tools named by the
target: `clang-format` for `check-format`, `clang-tidy` for `check-tidy`, and
Doxygen for `check-docs`. These tools are not needed for the production
`windows-release` build.

The release-certification matrix is:

| Platform | Toolchains | CI Qt |
| --- | --- | --- |
| Ubuntu 24.04 | distribution GCC and Clang, both warnings-as-errors | 6.11.2 |
| macOS 15 | Xcode AppleClang, warnings-as-errors | 6.11.2 |
| Windows | Visual Studio 2022 MSVC, warnings-as-errors | 6.11.2 |

Qt 6.10.3 is the source/API minimum; CI tracks the patched Qt 6.11 line. A
platform/version outside the matrix may build, but is not release evidence until
its strict and GPU jobs are added. See Qt's official
[release-support table](https://doc.qt.io/qt-6/qt-releases.html) and
[Qt 6.11.2 release notice](https://www.qt.io/blog/qt-6.11.2-released).

Configure from the `LAR/` project root. The build deterministically compiles
`assets/map/world_boundaries.geojson` into a validated
`lar_world_map.larmap`, packages it with the F-16 GLB and cubemap catalog beside
the executable, and keeps all source-parser/compiler code outside the runtime
link graph:

```bash
cmake --preset release
cmake --build --preset release --parallel
ctest --preset release
```

On macOS:

```bash
open build-release/lar-viewer.app
```

On Windows, install the matching `msvc2022_64` Qt kit and configure the
production-only preset. Pass `Qt6_DIR` when Qt is not already discoverable:

```powershell
cmake --preset windows-release -DQt6_DIR=C:/Qt/6.11.2/msvc2022_64/lib/cmake/Qt6
cmake --build --preset windows-release --parallel
cmake --install build-windows-release --config Release --prefix build-windows-package
./build-windows-package/bin/lar-viewer.exe
```

The install step deploys the required Qt runtime DLLs and plugins. The strict
test build uses `ci-msvc`; the Windows production preset disables tests and
tool-based quality targets:

```powershell
cmake --preset ci-msvc -DQt6_DIR=C:/Qt/6.11.2/msvc2022_64/lib/cmake/Qt6
cmake --build --preset ci-msvc --parallel
ctest --preset ci-msvc
```

For a debug build, use the developer preset:

```bash
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev
```

On macOS:

```bash
open build-dev/lar-viewer.app
```

Preset build directories contain different compiled outputs of the same source
tree. They are not separate projects. List all configure/build/test presets
with `cmake --list-presets=all`.

### Quality presets and gates

| Preset/target | Purpose |
| --- | --- |
| `ci`, `ci-gcc`, `ci-clang`, `ci-msvc` | hardening and warnings-as-errors for the selected compiler |
| `asan-ubsan` | memory and undefined-behavior contracts |
| `tsan` | runtime/source/recording/playback/map/viewport concurrency contracts |
| `coverage` / `coverage-report` | gcovr report and mandatory thresholds |
| `fuzz` | seven ASan/UBSan parser/domain fuzz targets |
| `mutation` / `check-mutation` | Mull-instrumented domain/application mutation gate |
| `check-repository` | format, architecture, semantic/link/API documentation, warning-free Doxygen |
| `check-tidy` | configured clang-tidy checks with zero warnings |
| `generate-sbom` | component-level SPDX 2.3 Qt/plugin/runtime manifest |

Typical strict verification is:

```bash
cmake --preset ci
cmake --build --preset ci --parallel
cmake --build build-ci --target check-repository generate-sbom
ctest --preset ci
```

The 30-minute real-loopback recording test is intentionally explicit:

```bash
LAR_SOAK_SECONDS=1800 ctest --test-dir build-ci \
  --output-on-failure -R '^laroperational-soak-tests$'
```

It exercises 100 Hz UDP, periodic snapshots, pause/resume, reset, final atomic
save, exact replay, and a resident-memory-growth ceiling. Full required PR,
nightly, GPU, fuzz, mutation, and release evidence is defined in
[QUALITY_GATES.md](docs/QUALITY_GATES.md).

Parser fuzz targets require Clang:

```bash
cmake --preset fuzz
cmake --build --preset fuzz --parallel
```

When the compiler provides libFuzzer, these are coverage-guided fuzz targets.
Apple toolchains that omit the libFuzzer runtime automatically build a
deterministic ASan/UBSan mutation runner instead:

```bash
ASAN_OPTIONS=detect_leaks=0 ./build-fuzz/lar-map-reader-fuzz 20000
ASAN_OPTIONS=detect_leaks=0 ./build-fuzz/lar-map-source-fuzz 20000
ASAN_OPTIONS=detect_leaks=0 ./build-fuzz/lar-session-reader-fuzz 20000
```

Mutation analysis requires a Mull IR frontend built for the selected Clang.
Set `LAR_MULL_IR_FRONTEND` if it is not in a standard system path:

```bash
cmake --preset mutation -DLAR_MULL_IR_FRONTEND=/path/to/mull-ir-frontend
cmake --build --preset mutation --target check-mutation
```

## Online Mode

1. Select **Online**.
2. Load a JSON packet mapping.
3. Select a UDP port and start the listener.
4. Choose the source policy. **Allow all** is the default; **Whitelist** opens a file picker for a newline-delimited list of sender addresses. The selected policy is highlighted green, and policy changes are disabled while listening.
5. Use the icon-only recording controls. The play/pause control starts, pauses, or resumes recording without stopping the listener. The save icon opens the final-session save flow; after a successful save, the online session ends. The trash icon clears the saved packages after confirmation.
6. Use **Stop Listener** to end the online stream. If no packages were saved, it stops immediately. If packages are saved but not persisted, the confirmation dialog explains that continuing will discard them; cancelling leaves the listener running.

The app starts with an empty working session and recording turned off. The Save Session widget shows elapsed duration as `minutes:seconds:milliseconds` and the count of saved packages. The working session is streamed to temporary storage to keep memory use bounded. A failed final save retains the working session so it can be retried.

The mapping root is an array:

```json
[
  {"name": "location", "index": 1, "offset": 0, "size": 8},
  {"name": "location", "index": 0, "offset": 8, "size": 8},
  {"name": "iz_pos", "index": 0, "offset": 16, "size": 8},
  {"name": "iz_r1", "index": 0, "offset": 24, "size": 8}
]
```

`name` is an exact `Plane` or `Target` member name. `index` selects an array element and is zero for scalar members. `offset` is the byte offset in the UDP package. `size: 4` means little-endian IEEE-754 float and `size: 8` means little-endian IEEE-754 double.

Entries may be reordered and members may be omitted. Omitted members appear as `N/A`. The supplied [full-state.json](maps/full-state.json) demonstrates a complete reordered mapping.

## Test Sender

```bash
./build/lar-test-sender \
  --map maps/full-state.json \
  --scenario mixed-high-dynamics \
  --port 45454 \
  --count 100 \
  --interval 100
```

Use `--list-scenarios` to list deterministic built-in motion, target, LAR, dateline, and high-dynamics scenarios. Existing options remain available: `--map`, `--host`, `--port`, `--count`, and `--interval`; omitting `--scenario` uses the compatible `default` scenario.

The `hundred-hz` scenario defaults to a 10 ms interval and sends 100 packets per second:

```bash
./build/lar-test-sender --map maps/full-state.json --scenario hundred-hz --count 1000
```

Use `polar-lar-stress` to move and resize oversized IR and IZ footprints near
the north pole. Use `pole-crossing-route` to exercise a route through both
geographic poles:

```bash
./build/lar-test-sender --map maps/full-state.json --scenario polar-lar-stress --count 500 --interval 50
./build/lar-test-sender --map maps/full-state.json --scenario pole-crossing-route --count 80 --interval 50
```

The maintained DLZ UDP scenarios use the dedicated three-field mappings:

```bash
./build/lar-test-sender --map maps/dlz-inputs.json \
  --scenario dlz-head-on-20 --count 100 --interval 100
./build/lar-test-sender --map maps/full-state-dlz.json \
  --scenario dlz-aspect-sweep --count 200 --interval 50
```

`lar-custom-test-sender` is a separately compiled, non-installed source template for custom state updates and malformed datagram tests. See the [test sender tutorial](src/testsender/README.md) for scenario details, units, mapping behavior, extension steps, and testing examples.

## Offline Mode

1. Select **Offline**.
2. Select a `.lar` file.
3. Use Play/Pause, Stop, timeline seek, and enter any positive floating-point playback rate.
4. Toggle the reset-icon **Repeat** control to wrap each newly calculated replay
   time modulo the final entry timestamp before selecting the displayed entry.
   Overshoot is retained even if one frame crosses multiple replay lengths.

Normal replay samples the session at 60 frames per second. Each sample advances
the exact cursor by `(1 / 60) * rate` seconds, binary-searches for the latest
entry strictly before that cursor, and displays only that entry. Stored entries
between presentation samples are not decoded in sequence.

The separate **Burst** panel is reserved for future whole-file statistics. Its
wide lightning-icon button is currently disabled and does not affect replay or
the displayed state.

The mapping is embedded in the `.lar` file. No external JSON mapping is used during replay.

## `.lar` Format

All fixed integers are little-endian.

```text
File:
Offset  Size  Value
0       4     ASCII "LAR1"
4       4     uint32 offset of the first UDP package record
8       N     UTF-8 JSON UDP package mapping
...     ...   Timed UDP package records through EOF

Record:
0       8     uint64 milliseconds from recording start
8       4     uint32 UDP package size
12      N     Original validated UDP package bytes
```

The package-section offset determines JSON length. Size-framed records preserve UDP boundaries, variable package lengths, trailing bytes, and original arrival timing.

## Current Values

All 21 legacy `Plane` and `Target` members plus the three mapped DLZ inputs are
listed in the **Current Values** column on the right while LAR content is
active. Plane mode retitles the same packet-value stack **Plane Telemetry**;
DLZ mode replaces it with the DLZ inputs and calculated values. Selecting LAR
restores Current Values. Controls remain permanently on the left;
neither side column can minimize, close, move, detach, or float.

## LAR Display

The LAR display occupies the central window. The **Viewport** controls select
one of three presentations:

- **Grid map** keeps the existing local metric grid. It is an intentionally flat
  Euclidean tactical plane (conceptually unbounded); Earth curvature and
  geodesic circle distortion are not modeled in this view. Mercator and Sphere
  are the geographic global views. The accepted radius ceiling is a rendering
  and resource bound, not a claim that Grid distances are Earth surface
  distances.
- **Mercator** draws the GPU world map and projection-correct geodesic IR/IZ
  footprints. Oversized polar footprints are tessellated across the projection
  cutoff and continue along both antimeridian map edges.
- **Sphere** draws the same geodesic footprints on the GPU globe. Tracked
  coordinates remain binary64 through CPU projection and use compensated
  high/low camera uniforms on the GPU, preventing close-zoom replay jitter.

The camera has three independent modes:

- **Follow plane** centers the aircraft. Its **Turn with plane** checkbox is
  the only configuration that rotates viewport-up away from north.
- **Follow target** centers `iz_pos` and remains north-up.
- **Free movement** keeps its current coordinate while new packets arrive and
  remains north-up.

Dragging a followed view changes it to Free movement. The mouse wheel changes
zoom, and double-click fits the current LAR data without changing the selected
camera mode. IR and IZ geometry is drawn whenever the active mapping provides
the required position, angle, and range values.

Diagnostics are reported through the status bar so no secondary window can cover the LAR display.

## Plane visualization

The top-centre **LAR | PLANE | DLZ** switch opens the Plane workspace without
changing the selected LAR projection or camera. It renders the packaged F-16
from behind and above while applying the current yaw, pitch, and roll
telemetry. The aircraft stays at the scene origin; this is an attitude and
orientation visualization, not a translational flight simulator.

The aircraft is surrounded by the selected cubemap. The **Skybox** button in
the lower-right display box advances through every valid packaged six-face set
and wraps back to the first set.

The checkable **Target** button in that box adds an opaque local tactical plane
beneath the aircraft and turns green while active. It projects available
target, in-range, and in-zone data relative to the aircraft onto east/north
metric axes and draws them over a quantized square grid. Minor grid lines,
stronger five-cell lines, origin axes, and the current cell size are shown
together. In this metric view the F-16 is the fixed scale reference: its
unchanged normalized nose-to-tail extent represents 15 m, so the ground,
target, grid, and LAR zones expand to their corresponding metric sizes around
it. The aircraft therefore keeps the same visible size when the surface is
toggled. Available aircraft altitude places the ground at its exact metric
distance below the aircraft, with ground bounds and grid spacing expanding for
a usable high-altitude view.

The adjacent checkable **Terrain** button replaces only that opaque flat ground
with a nearby DTED elevation mesh and turns green while active. DT0 is the
startup source. Use **DTED Folder** in the lower-left **Upload** box to select
DTED Level 1 or Level 2, then choose a folder whose cells use
`{e|w}DDD/{n|s}DD.dt1` or `.dt2`; the validated folder becomes the source for
the current session without being copied. The format prompt and folder picker
can both be cancelled without changing the active source. An invalid folder
also leaves the current source and patch intact.

DTED horizontal coordinates remain WGS84, elevations are interpreted as metres
above the tile's declared mean-sea-level datum, and the aircraft stays at the
scene origin while the stable terrain anchor moves underneath it. Tactical
grid, target, and LAR overlays remain available over the terrain. While a patch
is loading, a tile is absent, or a cell fails validation, Plane mode retains
the skybox/aircraft view without drawing a land-colored fallback, and reports a
diagnostic/status so an unclassified area cannot be mistaken for land.

Terrain vertices are sampled by inverting the same local flat east/north
projection used by the Plane overlays. When a patch is reused as the aircraft
moves, its east axis is rescaled for the change in reference latitude before it
is placed under the aircraft; this keeps a shoreline and its target marker in
the same geographic position instead of mixing spherical sampling with flat
rendering coordinates.

Ocean and sea posts are classified by the compact Natural Earth-derived mask.
Their negative DTED values remain bathymetric depth for a logarithmic
shallow-blue to deep-navy ramp, while their rendered geometry is held at mean
sea level. Land polygons, rather than elevation sign alone, keep depressions
such as the Dead Sea from being colored as ocean. The existing mask is sampled
by geographic position for DT1/DT2 terrain, so coastline classification remains
limited by its DTED0-aligned resolution even when elevations are more detailed.

For absolute vertical alignment, mapped `Plane.location[2]` must use metres
above a mean-sea-level datum compatible with the DTED files. If aircraft
altitude is unavailable, Plane mode preserves the tactical surface's small
default clearance above the sampled center elevation; it does not infer a true
MSL aircraft altitude.

Terrain preparation is latest-only on a dedicated CPU thread. It lazily opens
only addressed cells, bounds terrain caching by both 24 entries and 128 MiB,
and builds a 20–60 km half-extent patch using level-aware sample spacing with a
257-by-257 mesh cap. The water-mask cache remains capped at 24 entries. The
patch is reused until the aircraft moves beyond 35% of its radius; OpenGL
upload still occurs only in the widget's active context. DT0 is nominally
coarse terrain (roughly kilometre-scale), while DT1/DT2 improve sampled detail
within the mesh cap rather than forcing every source post onto the GPU.

The first valid aircraft position anchors the grid to the Earth. As subsequent
packets move the centered aircraft, the grid slides beneath it in the opposite
direction while retaining its ground-relative minor and major line positions.
Target and LAR overlays use the same aircraft-relative east/north projection.
The result remains a local flat presentation layer, not a globe.

The target is shown as a compact red four-sided pyramid. Its base half-width is
0.5% of the valid IZ outer radius, or IR when IZ is unavailable, and is clamped
between 3 m and 25 m. The pyramid height is 1.5 times that half-width, keeping
the marker recognizable without letting large zones produce a huge beacon.

Drag anywhere in the workspace to orbit around the aircraft, use the mouse
wheel to zoom, and double-click to restore the default chase view. While the
surface is enabled, camera elevation is constrained above it. The extended
zoom range allows inspection of the enlarged LAR geometry without changing the
default chase distance. There is currently no cockpit POV.

The model and cubemap files live under `assets/models` and `assets/cubemaps`.
Plane mode renders validated `TEXCOORD_0` UVs and PNG/JPEG base-color textures
from the packaged GLB or a user-uploaded `.gltf`/`.glb` model. The packaged
F-16 remains the default; use **Jet Model** in the lower-left **Upload** box to
replace it for the current session. **DTED Folder** similarly selects an
external DT1/DT2 tree for the session; restarting restores normal DT0 source
discovery.
They and the compact generated water mask are copied beside every executable
that renders or tests the Plane scene and installed beneath `bin/assets`. The
much larger `assets/DTED0` tree and user-selected DT1/DT2 trees are not copied
or installed by default. Plane mode checks `LAR_DTED0_ROOT`, then an
executable-adjacent `assets/DTED0`, then the CMake `LAR_DTED0_ROOT` development
path. The mask independently checks `LAR_DTED0_WATER_MASK`, its packaged path,
then the CMake-configured development mask file. Invalid or missing assets
leave the rest of the application usable.

Regenerate the mask after replacing DTED0 or the Natural Earth source:

```bash
python3 tools/build_dted_water_mask.py \
  --dted-root assets/DTED0 \
  --land assets/ne_10m_land/ne_10m_land.shp \
  --output assets/water/dted0_water_mask.bin \
  --force
```

Run the focused CPU/widget and opt-in native GPU checks with:

```bash
cmake --build build-release --target larplane-view-tests lar-plane-view-gpu-tests
QT_QPA_PLATFORM=offscreen build-release/larplane-view-tests
LAR_RUN_PLANE_GPU_TESTS=1 build-release/lar-plane-view-gpu-tests
```

## Dynamic Launch Zone (DLZ)

The top-centre **LAR | PLANE | DLZ** switch selects the central viewport content. The
`DLZ` page is a deliberately narrow air-to-air Dynamic Launch Zone lab:
it draws the vertical range staple, RMAX/RPI/RNE/RMIN ticks, the NEZ band, a
filtered current-range caret, and the stateful `SHOOT` cue. It is not a full
F-16 cockpit display and its ranges are fictional outputs from the exact toy equations in
the [DLZ model reference](docs/DLZ_MODEL.md). The mapped UDP/offline input
contract and two-source behavior are illustrated in the
[runtime data flows](docs/DATA_FLOWS.md).

When DLZ is selected, the right-hand **DLZ Values** column replaces Current
Values with two exclusive choices at the top: **UDP / Offline Replay** and
**Calculation Test**. The first choice shows the latest complete trio from
UDP or embedded replay data and has no visible sliders. The second exposes only
range/aspect/altitude sliders and draws exclusively from them; incoming packets
and replay operations are cached but cannot change that drawing. Both choices
use the same calculated geometry and DLZ ranges. Invalid combinations keep
readouts updated, clear the canvas, and show a persistent domain error until
valid again. Named UDP scenarios live in
[`src/testsender/scenarios.cpp`](src/testsender/scenarios.cpp).

Build and run the focused checks with:

```bash
cmake --build build-release --target lardlz-tests lardlz-view-tests
QT_QPA_PLATFORM=offscreen build-release/lardlz-tests
QT_QPA_PLATFORM=offscreen build-release/lardlz-view-tests
ctest --test-dir build-release --output-on-failure
```

The deterministic snapshot helper accepts any destination PNG and an optional
preset name (`head-on`, `beam`, `tail`, `minimum`, `far`, `sweep`, `fixture-a`,
or `fixture-b`):

```bash
build-release/lar-ui-snapshot /tmp/lar-head-on-20nm.png head-on
```

The native GPU integration test is skipped by default so headless CI remains
usable. Run it on a machine with a windowing/OpenGL context with:

```bash
LAR_RUN_MAP_GPU_TESTS=1 ctest --test-dir build-release \
  -R lar-earth-view-gpu-tests --output-on-failure
```
