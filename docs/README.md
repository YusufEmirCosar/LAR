# LAR Packet Monitor documentation

This documentation describes the behavior implemented by the current source
tree: mapped UDP capture, validated state publication, bounded recording,
deterministic replay, geographic and 3D presentation, and the fictional DLZ
teaching model.

LAR Packet Monitor is an engineering and visualization tool. It is not a
certified flight, navigation, fire-control, or weapon-employment system.

## Choose a path

| If you want to... | Start with... |
| --- | --- |
| Run the application and understand its controls | [User guide](USER_GUIDE.md) |
| Build it or submit a change | [Developer guide](DEVELOPER_GUIDE.md) |
| Integrate a UDP producer | [Packet protocol and units](PROTOCOL_UNITS.md) |
| Read or produce a session file | [LAR1 session format](LAR1_FORMAT.md) |
| Understand component boundaries | [Architecture](ARCHITECTURE.md) |
| Trace an online, recording, or playback operation | [Runtime data flows](DATA_FLOWS.md) |
| Change worker or asynchronous code | [Concurrency model](CONCURRENCY_MODEL.md) |
| Work on Grid, Mercator, Sphere, or Plane rendering | [Visualization guide](VISUALIZATION.md) |
| Work on map packages, DTED, land masks, models, or cubemaps | [Terrain and asset pipeline](TERRAIN_AND_ASSETS.md) |
| Review security and resource limits | [Threat model](THREAT_MODEL.md) |
| Prepare release evidence | [Quality gates](QUALITY_GATES.md) |

## Five-minute start

The minimum supported toolchain is CMake 3.24, C++17, and Qt 6.10.3 with Core,
Concurrent, Gui, Network, Widgets, OpenGL, and OpenGLWidgets. Test builds also
need Qt Test.

```bash
cmake --preset release
cmake --build --preset release --parallel
ctest --preset release
```

Start the viewer from the configured output directory, load
`maps/full-state.json`, select UDP port `45454`, and start the listener. In
another terminal:

```bash
./build-release/lar-test-sender \
  --map maps/full-state.json \
  --scenario mixed-high-dynamics \
  --port 45454 \
  --count 300 \
  --interval 50
```

On macOS the viewer is an application bundle:

```bash
open build-release/lar-viewer.app
```

On Windows, use the Visual Studio 2022 x64 preset with a matching Qt
`msvc2022_64` kit. Set both `Qt6_DIR` for configuration and the kit's `bin`
directory on `PATH` for build-time helpers, then install with an absolute
package prefix so Qt can deploy its runtime correctly. Follow the complete
[developer guide](DEVELOPER_GUIDE.md), section **Windows MSVC workflow**; the
root [README](../README.md), section **Windows setup**, also includes first-time
installation and troubleshooting commands.

See the [user guide](USER_GUIDE.md) for recording, replay, Plane terrain, and
DLZ workflows. See the [developer guide](DEVELOPER_GUIDE.md) for Windows,
debug, sanitizer, and repository-check commands.

## System at a glance

```text
UDP datagram
  -> exact-address policy
  -> immutable packet mapping
  -> complete temporary DecodedState
  -> domain validation
  -> coalesced presentation
  -> optional bounded recording batches

LAR1 session
  -> complete structural validation
  -> timestamped sparse checkpoints and one record-location page
  -> two-level bounded timestamp lookup
  -> sampled 60 Hz playback
  -> the same DecodedState and presentation path
```

Source dependencies point inward:

```text
viewer/presentation -> application <- infrastructure
                              |
                              v
                            domain

tools/map_asset -> map-format boundary
```

The production runtime owns network, session, and persistence worker threads.
Plane terrain adds a presentation-owned worker only while terrain is in use.
The UI thread owns widgets and every OpenGL context.

## Reference set

### Product and user behavior

| Document | Contents |
| --- | --- |
| [Project specification](../ProjectSpecification.md) | Maintained product requirements, limits, and acceptance criteria |
| [User guide](USER_GUIDE.md) | Online capture, recording, replay, all view modes, errors, and common tasks |
| [Protocol and units](PROTOCOL_UNITS.md) | Mapping grammar, all 24 fields, units, validation, and availability |
| [LAR1 format](LAR1_FORMAT.md) | Exact binary layout, snapshots, validation, indexing, and compatibility |
| [DLZ model](DLZ_MODEL.md) | Inputs, equations, supported domain, temporal presentation, and caveats |

### Design and implementation

| Document | Contents |
| --- | --- |
| [Architecture](ARCHITECTURE.md) | Layer rules, target graph, component diagrams, and extension points |
| [Component/API reference](COMPONENT_REFERENCE.md) | Responsibilities and important public operations by subsystem |
| [Runtime data flows](DATA_FLOWS.md) | Command, packet, recording, replay, map, terrain, and DLZ sequences |
| [Concurrency model](CONCURRENCY_MODEL.md) | Ownership, queues, epochs, drain barriers, coalescing, and shutdown |
| [Visualization guide](VISUALIZATION.md) | Coordinate spaces, camera behavior, geodesic accuracy, GPU paths, and Plane composition |
| [Terrain and asset pipeline](TERRAIN_AND_ASSETS.md) | LRM1 map compilation, DTED parsing, vector land classification, glTF, cubemaps, and packaging |
| [SOLID compliance](SOLID_COMPLIANCE.md) | Design-principle evidence and structural review checklist |
| [File reference](FILE_REFERENCE.md) | Repository map and ownership of important files |

### Maintenance and assurance

| Document | Contents |
| --- | --- |
| [Developer guide](DEVELOPER_GUIDE.md) | Build workflows and change recipes |
| [Quality gates](QUALITY_GATES.md) | What is locally available, what current CI runs, and the release checklist |
| [Threat model](THREAT_MODEL.md) | Trust boundaries, abuse cases, controls, budgets, and residual risk |

The source asset directories also have focused notes:
[asset inventory](../assets/README.md),
[world-map attribution](../assets/map/README.md), and
[test-sender tutorial](../src/testsender/README.md).

## Generated API website

The Doxygen website combines these authored pages with the production C++ API.
Use its **Classes**, **Files**, **Modules**, and search views when you need a
symbol-level reference. Public and high-risk contracts are written next to the
declaration; the guides explain how those declarations collaborate over time.

Generate the site from the repository root:

```bash
doxygen Doxyfile
```

The entry page is `html/index.html`. A configured CMake build exposes the
same warning-as-error generation as the `check-docs` target.

## Documentation conventions

- Protocol values are named with their exact mapping key and unit.
- `SessionTimestamp` means integer milliseconds, never floating-point time.
- “Accepted command” means the runtime took ownership of the request; it does
  not mean the operation succeeded.
- “Available field” means the active mapping supplied it and validation passed.
- “Earth view” means Mercator or Sphere. Grid and Plane tactical overlays are
  local flat metric presentations.
- “Terrain” means the DTED mesh. “Target” in the Plane controls means the
  independently toggled tactical surface, target marker, grid, and LAR overlays.

Normative product requirements live in the
[project specification](../ProjectSpecification.md). Binary and wire contracts
live in their format references. The implementation and tests are the final
authority for behavior; any discrepancy is a documentation defect and should
be corrected in the same change.
