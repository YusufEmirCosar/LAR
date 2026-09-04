# Component and API reference

This is the hand-written semantic index for production types. It describes why
an API exists, its important entry points, and what it collaborates with. The
generated Doxygen site is the exhaustive signature-level reference, including
all getters, signals, slots, private members, include graphs, and inheritance
graphs.

## Domain

### Packet-monitor values

| Component | Public surface | Responsibility and collaborators |
| --- | --- | --- |
| `Plane` (`domain/state.h`) | `location`, `euler`, `velocity` | Trivial protocol value with fixed units; owned inside `DecodedState` |
| `Target` (`domain/state.h`) | IZ/IR positions, bearings, radii, producer time | Trivial ground-LAR protocol value; validated before publication |
| `DecodedState` (`domain/decoded_state.h`) | `plane`, `target`, `dlzInputs`, `availableFields` | Atomic frame envelope; availability bits and values always travel together |
| `FieldBinding` (`domain/packet_mapping.h`) | `fieldId`, `offset`, `size` | One resolved packet byte range |
| `PacketMapping` | `isValid`, `bindings`, `minimumPacketSize`, `availableFields`, `json`, `decode`, `encode` | Immutable mapping schema; uses `StateField` getters/setters and retains exact JSON for recordings |
| `StateField` | `descriptor`, `all`, `resolve`, `displayName`, `presentationName`, `value`, `tryValue`, `setValue` | Single registry for mapping identity, unit, validation category, display label, and scalar access |
| `StateValidator` | `validate(Plane, Target, mask)`, `validate(DecodedState)` | Applies scalar rules only to available fields, then cross-field invariants such as ordered IZ radii |

`PacketMapping::decode` reads little-endian IEEE-754 values into a temporary
frame. `MappedPacketDecoder` then invokes `StateValidator`. A failed operation
does not alter the caller's output object.

### DLZ teaching domain

| Component | Public surface | Responsibility |
| --- | --- | --- |
| `dlz::TelemetryInputs` | `rangeNm`, `aspectDegrees`, `altitudeFeet` | Atomic packet/replay input triple |
| `ShooterState`, `TargetState`, `Atmosphere`, `WeaponModel` | Plain values | Complete inputs for geometry and the toy solver |
| `Geometry` | Range, rate, LOS, aspect, ATA, off-boresight, altitude separation | Finite derived engagement state |
| `Solution` | RMAX, RPI, RNE, RTR, RMIN, time of flight, cues | Ordered result accepted by presentation |
| `HudState` | Filtered range, scale, mode, timing, flash | Render-ready temporal presentation state |
| `calculateGeometry` | `(shooter, target, atmosphere, weapon, error) -> optional<Geometry>` | Pure guarded geometry derivation |
| `supportsOrdering` | `(aspect, altitude, Mach, weapon) -> bool` | Checks the toy model's supported monotonic domain |
| `evaluateToyModel` | `(shooter, geometry, weapon) -> Solution` | Diagnostic-only raw equations; never a rendering contract |
| `solve` | `(shooter, target, atmosphere, geometry, weapon) -> DlzSolveResult` | Validates input, domain, finiteness, and ordered output |
| `ScenarioAdapter::build` | `(ScenarioInputs, ScenarioFrame, error) -> bool` | Converts compact UI inputs into one complete domain frame |

The DLZ types deliberately do not alias or reinterpret `Plane`/`Target`. Only
the three `TelemetryInputs` fields join `DecodedState`.

## Application values and policy

| Component | Important operations | Notes |
| --- | --- | --- |
| `SessionTimestamp` | `zero`, `maximum`, `fromMilliseconds`, `fromStoredMilliseconds`, `fromSeconds`, `fromNanoseconds`, `clampedMilliseconds`, comparisons | Exact millisecond value constrained to 365 days; invalid construction returns `optional` |
| `IpAccessPolicy` | `allowAll`, `whitelist`, `mode`, `addresses`, `allows` | Canonical exact-match policy; no hostname, CIDR, or wildcard semantics |
| `CapturedPacket` | `data`, `receivedAtNanoseconds` | Validated raw datagram plus process-wide monotonic receipt time |
| `ApplicationState` | lifecycle fields, `isRecording`, `isRecordingPaused` | Facade-owned command eligibility; separate from display values |
| `ApplicationViewModel` | state/metric/mode/playback getters and `set*` updates | Observable UI-thread projection; never owns transport or storage |
| `RecordingOperationState` | `Idle`, `SavingSnapshot`, `Finalizing`, `Resetting`, `FailedRetained` | Prevents overlapping destructive/persistence operations |
| `RequestResultGate<Result>` | `beginDispatch`, `finishDispatch`, `receive`, `clear`, `pendingRequest` | Latest-wins correlation that handles direct synchronous and queued completions identically |

## Application ports

Ports are owned by the application layer. Concrete classes that implement them
live in infrastructure or presentation.

| Port | Operations | Contract |
| --- | --- | --- |
| `IDatagramSource` | `start`, `stop`, `isListening`, `localPort`, `setIpAccessPolicy`; signals `datagramReceived`, `transportError` | Complete datagrams, monotonic receipt stamps, repeatable stop |
| `IMappingRepository` | `loadFile`, `loadJson` | Replace output only after complete mapping validation |
| `IPacketDecoder` | `setMapping`, `decode` | Publish one complete validated `DecodedState` or leave output unchanged |
| `IIpAccessPolicyRepository` | `load` | Parse an exact policy transactionally |
| `IRecordingClock` | `nowNanoseconds` | Process-wide monotonic clock domain |
| `IRecordingTransaction` | `begin`, `append`, `reset`, `createSnapshot`, `cancel`, `isActive`, `recordCount` | Append-only active session with immutable snapshots |
| `ISessionSnapshot` | `writeTo` | Type-erased immutable bytes; persistence must not downcast |
| `ISessionPersistence` | `save` | Complete atomic replace or preserve previous destination |
| `ISessionReader` | `loadFile`, `loadData`, `close`, `isValid`, `recordCount`, `duration`, `findRecordAtOrBefore`, `timestampAt`, `recordAt` | Fully validate/index before exposing random access and native timestamp selection |
| `IPlaybackClock` | `start`, `stop`, `isActive`; signal `tick` | Injectable fixed-rate replay tick scheduling |

### Runtime boundary

`IApplicationRuntime` combines five segregated command interfaces with a typed
Qt event protocol:

| Slice | Commands |
| --- | --- |
| `IMappingCaptureRuntime` | `loadMapping`, `startOnline`, `setIpAccessPolicy`, `stopOnline` |
| `IRecordingRuntime` | `startRecording`, `pauseRecording`, `resumeRecording`, `stopRecording`, `resetRecording`, `snapshotRecording`, `discardRecording` |
| `IPlaybackRuntime` | `loadSession`, `closeSession`, `play`, `pause`, `stop`, `seek`, `setPlaybackRate`, `setPlaybackRepeat` |
| `IMetricsRuntime` | `resetMetrics` |
| `IApplicationLifetime` | `shutdown` |

Every command returns `CommandDispatch`. A structurally valid dispatch has a
non-zero `RuntimeRequestId`; exactly one operational completion arrives in a
matching typed result. Commands without a dedicated payload use
`RuntimeCommandResult` and `RuntimeCommandKind`. `RuntimeFailure` is reserved for
unsolicited runtime diagnostics rather than command completion. State-producing
results and events also carry a `RuntimeSourceEpoch` so inactive queued work can
be discarded.

`runtime_messages.h` defines the complete message vocabulary:

- command results: `RuntimeCommandResult`, `MappingLoadResult`,
  `OnlineStartResult`, `OnlineStopResult`, `IpPolicyChangeResult`,
  `RecordingSaveResult`, `RecordingResetResult`, `SessionLoadResult`, and
  `SessionCloseResult`;
- source events: `OnlineStateEvent`, `StateEvent`, `MetricsEvent`,
  `PlaybackPositionEvent`, `PlaybackStateEvent`, and `PlaybackFinishedEvent`;
- source-independent recording state: `RecordingStateEvent`;
- categorized diagnostics: `RuntimeFailure` and `RuntimeFailureCode`.

`registerRuntimeMessageTypes()` registers all values used for queued delivery.

## Application services and coordinators

| Component | Public commands | Emits / collaborates |
| --- | --- | --- |
| `ApplicationFacade` | Mapping, online, policy, recording, session, playback, metric, and shutdown commands | Owns `ApplicationState`, result gates, and `SourceLifecycleCoordinator`; updates `ApplicationViewModel` |
| `ModeCoordinator` | `mode`, `modeString`, `transitionTo` | Rejects illegal top-level transitions; emits `modeChanged` |
| `SourceLifecycleCoordinator` | `startOnline`, `stopOnline`, `loadSession`, `closeSession`, playback controls, `resetMetrics`, `applyRecordingState`, `shutdown` | Sole online/playback source owner; filters request IDs and epochs |
| `IpAccessPolicyService` | `load` | Delegates policy loading to `IIpAccessPolicyRepository` |
| `OnlineCaptureService` | `start`, `stop`, `setIpAccessPolicy`, `setMapping` | Uses datagram/decoder ports; emits every accepted raw packet and coalesced UI frames |
| `MetricsService` | `recordDatagramAttempted`, `recordPacketProcessed`, `recordPlaybackPackets`, `reset` | Publishes totals and elapsed-time-normalized rate |
| `RecordingService` | Start/pause/resume/reset/snapshot/complete/cancel; single and batch record methods | Owns active-time segments; delegates bytes and clock to ports |
| `RecordingPipelineCoordinator` | Recording commands, input batches/drain callbacks, persistence callback, shutdown | Sole drain/persist state machine; publishes immutable snapshot requests |
| `PlaybackService` | `loadSession`, `loadData`, `closeSession`, play/pause/stop/seek/rate/repeat | Advances a 60 Hz exact cursor, requests a strict predecessor from the reader's native index, and decodes at most one selected record per tick |
| `DirectApplicationRuntime` | Full `IApplicationRuntime` | Synchronous adapter for tests/embedding; preserves the same request/result protocol |

## Infrastructure adapters

### Mapping and network

| Adapter | Implements / uses | Key operations |
| --- | --- | --- |
| `JsonMappingRepository` | `IMappingRepository` | `loadFile`, `loadJson`; parses bounded JSON entries and constructs `PacketMapping` |
| `MappedPacketDecoder` | `IPacketDecoder` | Delegates immutable decoding and domain validation |
| `QtIpAddressNormalizer` | Pure helper | Canonicalizes IPv4, IPv6, and IPv4-mapped IPv6 for exact policy comparison |
| `QtIpAccessPolicyRepository` | `IIpAccessPolicyRepository` | `load`, `parseText`; rejects hosts, CIDR, wildcards, invalid/duplicate input as defined by policy |
| `QtUdpDatagramSource` | `IDatagramSource` | Owns `QUdpSocket`, binds, drains pending datagrams, applies sender policy, timestamps receipt |

### Session and timing

| Adapter | Implements | Key operations |
| --- | --- | --- |
| `LarSessionWriter` | `IRecordingTransaction` | Streams LAR1 bytes to temporary storage and creates stable prefix snapshots |
| `FileSessionSnapshot` | `ISessionSnapshot` | Retains temporary-file lifetime and writes exactly a captured byte prefix |
| `QtSessionPersistence` | `ISessionPersistence` | Uses `QSaveFile` commit semantics |
| `LarSessionReader` | `ISessionReader` | Validates every header/mapping/record; uses a bounded immutable source snapshot and complete index when they fit, with checkpoint/page and file-backed fallbacks; decodes lazily without a product count cap |
| `QtRecordingClock` | `IRecordingClock` | Uses process-wide `steady_clock` nanoseconds |
| `QtPlaybackClock` | `IPlaybackClock` | Supplies fixed-rate ticks with a precise `QTimer` |

### Runtime workers

| Component | Thread ownership | Role |
| --- | --- | --- |
| `ThreadedApplicationRuntime` | UI/caller thread plus three owned `QThread`s | Constructs workers, assigns affinity, relays typed commands/results, owns shutdown/join |
| `NetworkRuntimeWorker` | Network thread | Owns mapping repository, UDP adapter, decoder, capture, metrics, recording batch buffering |
| `RecordingRuntimeWorker` | Session thread | Owns transaction, clock, recording service, pipeline coordinator |
| `PlaybackRuntimeWorker` | Session thread | Owns reader, clock, playback service, and metrics; forwards changed state and position on the originating replay tick |
| `PersistenceRuntimeWorker` | Persistence thread | Atomically saves immutable snapshots |
| `PlaybackMetrics` | Session thread | Counts processed records and rate independently from online metrics |

## Presentation shell

| Component | Public surface | Responsibility |
| --- | --- | --- |
| `MainWindow` | Constructor, close lifecycle, render/update slots | Composes panels, workflows, viewport, status diagnostics; contains no transport/session IO |
| `HelpWindow` | `showTopic`, current-topic resolver | Non-modal navigation, filtering, history, and rendering for resource-embedded operator documentation |
| `lar::help::currentTopic` | Focused widgets, viewport state, active mode | Selects the most relevant stable help topic without coupling the help browser to `MainWindow` internals |
| `OnlineWorkflowController` | `requestMapping`, `requestWhitelist`, `requestAllowAll` | Owns mapping/policy dialogs; Allow all is the default and the last successfully applied policy is exposed to the panel |
| `RecordingWorkflowController` | `requestSnapshot`, `requestFinalSave`, `requestReset`, `prepareToClose` | Converts user choices into facade commands |
| `IViewerFileDialog` / `QtViewerFileDialog` | Mapping, whitelist, session selections | Selection-only presentation port and Qt adapter |
| `IRecordingFileDialog` / `QtRecordingFileDialog` | Save paths, reset/discard confirmation | Recording-choice presentation port and Qt adapter |
| `OnlinePanel` | Constructor; private render callbacks | Mapping/listener/policy/recording controls |
| `OfflinePanel` | Constructor; session selection signal | Playback/repeat controls, exact timeline mapping, rate validation, and inert Burst placeholder |
| `ValuesPanel` | `setContentMode`, `render`, `hudControlPanel` | Switches the fixed right column among LAR values, Plane telemetry, and DLZ controls |
| `MetricsPanel` | Frame/packet setters, `render`, reset signal | Displays UI and runtime throughput |

Formatting helpers are pure: `StateValueFormatter`, `PlaybackTimeFormatter`,
`PlaybackTimelineMapper`, `GridGeometryBuilder`, `LarProjection`,
`LarGeometryBuilder`, and `LarGeodesicGeometry`.

## Map subsystem

| Component | Important operations | Responsibility |
| --- | --- | --- |
| `MapAssetReader` | `readFile`, `readData` | Bounded `.larmap` parsing, CRC, offsets, counts, coordinate/index checks |
| `IMapAssetSource` | `load` | Package-source abstraction for an Earth view |
| `PackagedMapAssetSource` | `load`, path resolution | Reads map and manifest located beside the executable |
| `MapProjection` | Geographic/Mercator conversion and longitude wrapping | Shared build/runtime pure projection policy |
| `MapChecksum` | `crc32` | Dependency-free IEEE CRC-32 |
| `MapLandIndex` | `classify`, candidate-range lookup | Immutable one-degree index over compiled land triangles; shared coastline authority for Earth and Plane |
| `MapCamera` | Presentation, binary64 sphere center, compensated GPU parameters, zoom, bearing, projection, pan, hit testing | Projection-independent Earth camera model with stable close-zoom tracking |
| `EarthMapGpuRenderer` | Initialize/install/render/release | Owns immutable OpenGL programs and buffers |
| `EarthMapWidget` | Mesh installation, camera operations, interaction | OpenGL widget delegating actual drawing to the renderer |

`map_asset_format.h`, `map_asset_limits.h`, `map_mesh.h`, `map_palette.h`,
and `map_shaders.h` centralize binary constants, resource bounds, CPU mesh data,
colors, and embedded GLSL respectively.

## LAR viewport subsystem

| Component | Important operations | Responsibility |
| --- | --- | --- |
| `ILarViewportPage` | `widget`, `events`, `setSceneState`, camera methods | Common Grid/Earth page contract |
| `IEarthLarViewportPage` | Earth-specific camera/map operations | Narrow extension for Mercator/Sphere pages |
| `LarViewport` | Content mode, LAR view mode, scene state, camera delegation | Retains the latest scene, updates only the active Grid/Earth/Plane/DLZ page, and synchronizes a destination page before switching to it |
| `ViewportControls` | Render/select view and camera modes | Emits user camera/content commands |
| `ViewportCameraController` | Tracking mode, rotation policy, free center | Projection-neutral camera state owner |
| `GridCameraTransform` | Screen/world conversions and pan/zoom | Pure local-grid camera math |
| `GridLarView` | `ILarViewportPage` implementation | Local metric grid drawing and input |
| `EarthLarView` | `IEarthLarViewportPage` implementation | Map, marker, zones, camera, async preparation, OpenGL integration |
| `EarthCameraPolicy` | Field checks/change checks, tracking, fit points, zoom anchor, scale | Pure policy extracted from the widget |
| `EarthMapLoadController` | `ensureLoaded`, `completeInstallation`, `invalidate` | Single-flight revision-safe asynchronous asset loading |
| `EarthOverlayCoordinator` | Dirty/request/consume fallback mesh | Latest-only worker-thread CPU overlay preparation |
| `ViewportPreparationWorker` | `prepare` slot | Bounded off-thread zone mesh building |
| `LarZoneInputValidator` | `validate` | Field-presence and domain/resource validation for IR/IZ definitions |
| `GeodesicZoneSampler` | `sample` | Adaptive bounded ring/sector sampling on a sphere |
| `LarZoneMeshAssembler` | `append` | Projection-specific indexed fill/outline assembly |
| `LarZoneMeshBuilder` | `build` | Orchestrates validate → sample → assemble |
| `LarZoneGpuLayer` | Mesh install and draw | CPU fallback mesh OpenGL owner |
| `LarParametricZoneGpuLayer` | State install and draw | Shader-evaluated geodesic zone rendering |
| `LarMarkerLayer` | Scene/camera update and paint | Aircraft/target overlay markers |
| `LarNavigationOverlay` | Scale and north indicator update/paint | Shared Earth navigation overlay |
| `LarViewFit` | Mercator and sphere fits | Pure camera fit policy for available LAR coordinates |

Resource limits live in `lar_zone_mesh_limits.h` and
`map_asset_limits.h`; renderers never allocate directly from untrusted counts.

## Plane visualization subsystem

| Component | Important operations | Responsibility |
| --- | --- | --- |
| `PlaneViewWorkspace` | Scene/page contract, event forwarding, model/terrain upload actions | Plane-mode host with lower-left Upload and lower-right terrain/target/skybox control boxes |
| `PlaneSceneWidget` | `setSceneState`, `loadModelFromFile`, `loadTerrainFromDirectory`, `setSurfaceVisible`, `selectNextSkybox`, `resetCamera` | OpenGL widget, packaged/user model loading, session terrain-source switching, orbit input, diagnostics, frame signals |
| `PlaneSceneRenderer` | Model/skybox/surface install, `initialize`, `draw`, `cleanup` | Context-thread GPU lifecycle and ordered scene composition |
| `GlbModelReader` | `readFile` | Bounded `.gltf`/`.glb` mesh/UV parsing, data-URI or local-resource loading, node transform flattening, material extraction, normalized mesh output |
| `GlbResourceReader` | Buffer/image URI resolution | Restricts external resources to the selected model directory and enforces encoded and aggregate byte budgets |
| `GlbTextureReader` | Image decode and sampler conversion | Validates dimensions, decoded byte cost, format, and glTF sampler state before publication |
| `CubemapCatalog` | Discover, name, load | Natural-order grouping and validation of `rt/lf/up/dn/ft/bk` square-face sets |
| `PlaneAttitudeTransform` | `orientation` | Available-field-aware yaw/pitch/roll to model quaternion conversion |
| `PlaneOrbitCamera` | `orbit`, `zoom`, `reset`, `setGroundConstrained`, `viewMatrix` | Heading-relative chase/orbit camera anchored at the aircraft origin |
| `PlaneAircraftScale` | `metersPerSceneUnit` | Fixed 15 m F-16 reference used to expand metric surface content without resizing the model |
| `PlaneSurfaceProjection` / `PlaneSurfaceState` | `project` and immutable draw values | Local tangent projection, telemetry altitude, persistent grid phase, aircraft-derived scale, bounds, bounded target sizing, and partial-field behavior |
| `PlaneSurfaceGpuLayer` | `initialize`, `draw`, `cleanup` | Opaque ground, adaptive grid, flat IR/IZ zones, and target-pyramid GPU rendering |
| `PlaneModelMesh` / `CubemapFaces` | Immutable CPU values | Validated geometry, decoded textures, sampler state, and cubemap handoff to the context-owned renderer |

### Plane terrain pipeline

| Component | Important operations | Responsibility |
| --- | --- | --- |
| `DtedCellReader` | `readFile`, profile parsing | Validates UHL/DSI/ACC records, level-specific dimensions, profile sequence/checksum, coordinates, signed-magnitude samples, and no-data values |
| `DtedTileSource` | `pathFor`, `load` | Converts WGS84 degree keys to canonical DT0/DT1/DT2 paths and applies platform-aware canonical-relative containment beneath the selected root |
| `DtedMosaicSampler` | `sample` | Bilinear elevation sampling across cell boundaries with a 24-entry/128-MiB LRU and bounded negative lookups |
| `PlaneLandMaskBuilder` | `build` | Rasterizes candidate `MapLandIndex` triangles into an adaptive local R8 mask; collapses uniform all-land/all-water results |
| `PlaneTerrainPatchBuilder` | `build` | Samples a bounded local tangent grid, derives normals and water depth, and emits one immutable CPU patch |
| `PlaneTerrainWorker` | source/request/cancel slots | Owns samplers off-thread, coalesces requests, and publishes only revision-tagged immutable candidates |
| `PlaneTerrainGpuLayer` | `initialize`, `install`, `draw`, `cleanup` | Context-thread terrain buffer/texture ownership and separate land/water passes |

The model uses nose `-Z`, right wing `+X`, and up `+Y`. Plane yaw, pitch, and
roll arrive in radians and are applied without translating the aircraft. The
optional tactical surface uses aircraft position as its local metric origin,
interprets the unchanged model's forward extent as 15 m, and expands the ground
and overlays around it. Available altitude sets the ground's vertical distance;
the first valid geographic position anchors grid lines so later aircraft motion
slides the ground beneath the centered model. This does not reintroduce a
spherical Earth or a translated aircraft model.

## DLZ presentation

| Component | Important operations | Responsibility |
| --- | --- | --- |
| `ControlPanel` | Input mode, slider inputs, readouts, status | Exclusive UDP/replay vs calculation-test input UI |
| `HudWorkspace` | `setInputMode`, `setExternalInputs`, `clearExternalInputs` | Joins controls, `ScenarioAdapter`, solver output, and temporal presentation; its 16 ms animation timer runs only while visible |
| `PresentationController` | `reset`, `update`, `clear`, state/readout getters | Range filtering, directional scale changes, shoot-cue timing |
| `RangeScaleController` | `reset`, `update`, `scaleMaximumNm` | Hysteretic selection among fixed scale steps |
| `HudView` | Frame/diagnostic setters | Owns paint-ready values and delegates to renderer |
| `HudRenderer` | `render` | Stateless `QPainter` symbology |
| `RenderFixture` helpers | Named fixture lookup | Deterministic visual/test states |

## Build-time map compiler

| Component | Important operations | Responsibility |
| --- | --- | --- |
| `GeoJsonSourceReader` | `readFile`, `readData` | Bounded GeoJSON Feature/geometry parsing into `SourceMap` |
| `PolygonTriangulator` | `triangulate` | Polygon/ring cleanup and deterministic planar fill indices |
| `MapMeshCompiler` | `compile` | Projection, triangulation, deduplication, border generation, one-degree land-index construction, and count checks |
| `MapAssetWriter` | `write` | Versioned header, sections, CRC, and manifest output |

The compiler executable runs during the `lar-map-asset` CMake target. Only the
generated `.larmap` and manifest are packaged with the viewer.
