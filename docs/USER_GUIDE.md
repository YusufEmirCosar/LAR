# User guide

## What the application does

LAR Packet Monitor receives UDP datagrams described by a JSON mapping, validates
the decoded aircraft/target state, displays the result, optionally records the
accepted raw datagrams, and replays saved sessions. The central content can be
shown as a local Grid, a Mercator map, a globe, a third-person Plane scene, or a
fictional Dynamic Launch Zone (DLZ) teaching display.

The application does not authenticate telemetry and does not determine whether
the values are physically plausible. Use it on a trusted network or through an
authenticated tunnel when source integrity matters.

## Start the viewer

After a release build, run the executable from `build-release`. On macOS use:

```bash
open build-release/lar-viewer.app
```

On Linux:

```bash
./build-release/lar-viewer
```

On Windows:

```powershell
.\build-windows-package\bin\lar-viewer.exe
```

That installed package contains the required Qt DLLs and plugins. Running the
uninstalled Visual Studio output directly is also possible while the matching
Qt kit is on the current process `PATH`:

```powershell
$env:Path = "C:\Qt\6.10.3\msvc2022_64\bin;$env:Path"
.\build-windows-release\Release\lar-viewer.exe
```

The exact output path depends on the generator and whether the project was
installed. See **Windows MSVC workflow** in the
[developer guide](DEVELOPER_GUIDE.md) for the field-tested configure, build,
deployment, and troubleshooting procedure.

## Window layout

The application keeps the operational controls visible:

- The left column switches between **Online** and **Offline** workflows.
- The top-center switch selects **LAR**, **PLANE**, or **DLZ** content.
- LAR content has separate Grid, Mercator, and Sphere viewport controls.
- The right column shows Current Values, Plane Telemetry, or DLZ controls,
  according to the selected content.
- The status bar reports mapping, runtime, asset, and rendering diagnostics.
- The **?** button beside **LAR PACKET MONITOR**, or **F1**, opens searchable
  help without stopping capture, recording, or replay. **Help for current
  screen** jumps directly to the relevant Online, Offline, LAR, Plane, or DLZ
  instructions.

The side columns are fixed. They cannot float, detach, close, or cover the
central visualization.

Selected mode and toggle buttons retain a pale-green background after the
pointer leaves them. A muted version remains visible when a selected control is
temporarily disabled, so the current workflow, policy, view, and overlay state
can still be identified.

## Online capture

### 1. Load a packet mapping

Select **Online**, then **Select JSON Mapping**. The supplied mappings are:

| File | Use |
| --- | --- |
| `maps/full-state.json` | All 21 Plane/Target fields |
| `maps/dlz-inputs.json` | Only the complete three-field DLZ input |
| `maps/full-state-dlz.json` | Plane/Target state plus all DLZ inputs |

A mapping defines byte offsets; the datagram contains no field names or
self-description. Sender and receiver must use the same mapping. The listener
button remains disabled until a mapping has loaded successfully.

After a successful selection, the Packet Mapping panel shows the mapping's
filename, mapped-field count, and minimum UDP packet size. Hover the filename
to see its full path. Cancelling or selecting an invalid replacement preserves
the last successfully loaded mapping and its displayed filename.

### 2. Select the sender policy

**Allow all** accepts any sender address and is the default. **Whitelist**
selects a UTF-8 text file containing one numeric IPv4 or IPv6 address per line.
Blank lines and lines whose first non-space character is `#` are ignored.
Addresses are canonicalized and duplicates collapse.

The whitelist is bounded to 1 MiB, 4,096 lines, 4,096 distinct addresses, and
256 bytes per line. It must contain at least one valid address. Policy controls
are disabled while listening, and a failed replacement leaves the last
successfully applied policy active.

A whitelist is routing policy, not proof of identity. IP spoofing and replay
remain possible on networks where an attacker can inject traffic.

### 3. Start the listener

Choose a port in `1..65535` (default `45454`) and select **Start Listener**.
The application binds UDP, filters the sender, decodes each allowed datagram,
and validates the entire candidate state before publishing it.

Packets shorter than the mapping's largest `offset + size`, non-finite or
out-of-range values, and schema-invalid DLZ combinations are rejected. A
rejected packet neither changes the visible state nor enters a recording.
Trailing bytes on a valid datagram are allowed and are preserved by recording.

### 4. Send test traffic

Run the maintained deterministic sender in another terminal:

```bash
./build-release/lar-test-sender \
  --map maps/full-state.json \
  --scenario figure-eight \
  --host 127.0.0.1 \
  --port 45454 \
  --count 300 \
  --interval 50
```

List every built-in scenario:

```bash
./build-release/lar-test-sender --list-scenarios
```

The [test-sender tutorial](../src/testsender/README.md) describes each scenario
and how to add one.

## Recording

Recording is independent of whether a new frame is currently being painted.
Every accepted raw datagram remains eligible for the recording pipeline even
though high-rate visual updates are coalesced.

The three icon controls under **Save Session** are:

| Control | Behavior |
| --- | --- |
| Play/Pause | Start a recording, pause active-time accumulation, or resume |
| Save | Choose a final `.lar` destination and end the online session after a successful commit |
| Trash | Confirm and discard the working recording |

The duration label shows active recording time as
`minutes:seconds:milliseconds`; paused wall time does not advance it. “Saved
packages” is the number of raw accepted datagrams already appended to the
working transaction.

The transaction streams to temporary file storage, so the application does not
keep every packet in memory. Saving uses an immutable prefix and atomic
replacement. If a final save fails, the existing destination is preserved and
the working session remains available for retry.

Stopping the listener with an unsaved recording asks for confirmation. Cancel
keeps the listener running. Confirm discards the session before the source is
stopped.

## Offline replay

Select **Offline**, then **Select .lar File**. A valid file supplies its own
embedded mapping; an online JSON mapping is not used for replay.

Playback controls provide:

- Play/Pause.
- Stop, which returns the cursor to the beginning.
- Timeline seeking.
- Any finite positive playback rate; invalid input is reset to `1x`.
- Repeat, which keeps overshoot when wrapping at the final timestamp.

Playback presents at 60 ticks per second. Each tick advances the exact
millisecond cursor and selects the applicable stored record with a binary
search. It does not decode every record between two presentation ticks, so
high-rate recordings remain responsive at high replay rates. Ordinary files up
to 512 MiB are replayed from an immutable RAM snapshot, and a complete record
index is retained within a separate 128 MiB budget. Very large sessions remain
supported through the file-backed and sparse-page fallbacks. A changed state is
published on its originating replay tick, and only the currently visible
viewport page performs the live scene update.

The **Burst** control is intentionally disabled. It is a placeholder for future
whole-file analysis and currently dispatches no operation.

## LAR views

### Grid

Grid is a local flat Euclidean tactical plane. It uses metric east/north
projection around the selected origin and does not model Earth curvature.
Choose it for nearby relative geometry and an uncomplicated scale grid.

### Mercator

Mercator draws the compiled world map and geodesic IR/IZ geometry. It clips the
map at the supported Mercator latitude and explicitly handles the antimeridian
and polar footprints. Distances are constructed on the sphere before
projection.

### Sphere

Sphere renders the same geographic definitions on a globe. Tracked centers stay
in binary64 on the CPU and are sent to shaders as compensated high/low values
to reduce close-zoom jitter.

### Camera controls

- **Follow plane** centers the aircraft.
- Selecting **Plane** follows the aircraft north-up. Clicking the already
  selected **Plane** camera button toggles whether viewport-up turns with the
  aircraft; the stronger selected tint identifies heading-follow.
- **Follow target** centers the IZ position and remains north-up.
- **Free movement** retains the current center as new packets arrive.
- Dragging a followed view changes it to Free movement.
- The wheel zooms.
- Double-click fits the available LAR content without changing the selected
  tracking mode.

Geometry appears only when every required field is available. An unmapped value
is shown as `N/A`; it is never inferred from zero-initialized storage.

## Plane view

Plane keeps the aircraft model at the scene origin and applies current
heading/yaw, pitch, and roll. It is an attitude and relative-environment view,
not a translational flight simulator.

A Plane-only mapping is supported; target and DLZ fields are not required to
draw the aircraft. The three `euler` components independently control yaw,
pitch, and roll. Missing attitude components use a neutral zero rotation and
produce the **ATTITUDE DATA INCOMPLETE** warning until all three are mapped.
Aircraft latitude and longitude are required together only for geographic
anchoring, terrain, and relative target/LAR placement. Altitude controls the
aircraft-to-ground distance. Velocity values appear in telemetry but do not
move or rotate the model.

Mouse controls:

- Drag to orbit.
- Use the wheel to zoom.
- Double-click to restore the default chase view.

The lower-right controls are:

| Control | Behavior |
| --- | --- |
| **Terrain** | Show the active DTED patch when a valid source, map land index, and aircraft position are available |
| **Target** | Show the local metric grid, target marker, and IR/IZ overlays; this is independent of Terrain |
| **Skybox** | Advance through valid packaged six-face cubemap sets |

The packaged F-16 is the default scale reference. Its normalized forward extent
represents 15 metres; ground, target, and zone geometry scale around the
unchanged model. The first valid aircraft coordinate anchors the ground grid,
so subsequent movement slides the environment under the centered aircraft.

### Load another model

Use **Upload > Jet Model** to select `.gltf` or `.glb`. The supported subset
requires static triangle meshes with float positions and normals; UVs,
base-color materials, and PNG/JPEG textures are optional. A failed or
unsupported model leaves the current model active.

External glTF resources must remain under the selected model directory.
Absolute paths, traversal, symlinks, query/fragment syntax, and oversized
resources are rejected.

### Use terrain

DTED Level 0 is discovered at startup. If it is unavailable, use
**Upload > DTED Folder**, choose Level 1 or Level 2, then select a tree whose
files follow:

```text
{e|w}DDD/{n|s}DD.dt1
{e|w}DDD/{n|s}DD.dt2
```

The chosen directory is validated and used in place for this session. It is not
copied into the application. Cancelling either dialog or choosing an invalid
tree leaves the active source unchanged. Windows paths may use normal native
backslashes; the application normalizes them before checking that each tile
remains inside the chosen directory.

Terrain requires aircraft latitude and longitude. Mapped aircraft altitude is
interpreted as metres above a mean-sea-level datum compatible with the DTED
dataset. Without altitude, the view preserves a visual clearance relative to
the sampled center; it does not claim a recovered absolute altitude.

Terrain is prepared asynchronously. While it is loading or unavailable at the
current coordinate, no land-colored fallback is drawn. The aircraft and
skybox remain usable and the diagnostic explains the missing input or asset.
If **Target** is also selected, its grid and overlays remain independently
available.

See [Terrain and asset pipeline](TERRAIN_AND_ASSETS.md) for DTED validation,
cache bounds, land/water classification, and packaging.

## DLZ view

DLZ is a deliberately fictional teaching model. It renders range ticks, the
no-escape band, a filtered range caret, and a stateful SHOOT cue. It is not a
real weapon model or cockpit display.

The right column offers two exclusive sources:

- **UDP / Offline Replay** uses a complete mapped triple:
  `dlz_range_nm`, `dlz_aspect_deg`, and `dlz_altitude_ft`.
- **Calculation Test** uses only the visible range, aspect, and altitude
  sliders. Incoming external values are cached but cannot alter the drawing.

Invalid or unsupported input keeps the readouts visible, clears the symbology,
and shows a persistent diagnostic. Switching input mode never blends values
from the two sources. Exact equations and temporal behavior are documented in
the [DLZ model reference](DLZ_MODEL.md).

## Common problems

| Symptom | Check |
| --- | --- |
| Start Listener is disabled | Load a valid JSON mapping first |
| No state changes | Confirm sender/receiver mappings, UDP port, address policy, and packet length |
| Values show `N/A` | The active mapping omitted those fields |
| LAR area is absent | Required center/range/angle fields are missing or outside renderer bounds |
| Session will not load | Read the status diagnostic; the file may be truncated, corrupt, non-monotonic, or over a format limit |
| Plane says attitude incomplete | Map all three `euler` components |
| Terrain button is disabled | Configure a valid DT0 root or upload a valid DT1/DT2 folder |
| Terrain stays loading/unavailable | Confirm aircraft coordinates, the addressed tile, the DTED level, and packaged world-map availability |
| Uploaded model is rejected | Use the supported static glTF subset and keep resources contained below the model directory |
| GPU view is unavailable | Use a native OpenGL-capable display and current drivers |

Detailed error and security behavior is in the
[threat model](THREAT_MODEL.md); build and test troubleshooting is in the
[developer guide](DEVELOPER_GUIDE.md).
