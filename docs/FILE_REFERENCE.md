# File reference

This inventory covers the maintained repository. Generated `build-*`
directories are deliberately excluded. In the tables, `name.{h,cpp}` denotes
the declaration and implementation files as a pair; header-only and split
implementation files are listed explicitly.

## Repository root

| Path | Purpose |
| --- | --- |
| `CMakeLists.txt` | Project options, Qt discovery, target inclusion, install rules, and source map-copy policy |
| `CMakePresets.json` | Developer, release, strict CI, sanitizer, coverage, fuzz, and mutation configurations |
| `README.md` | User-facing overview, build/run instructions, and feature guide |
| `ProjectSpecification.md` | Authoritative functional and acceptance requirements |
| `.clang-format` | C++ formatting policy used by `check-format` |
| `.clang-tidy` | Static-analysis checks used by `check-tidy` |
| `.gitignore` | Generated builds, Finder metadata, and editor temporary exclusions |
| `mull.yml` | Mutation-analysis source and mutation filters |
| `.vscode/c_cpp_properties.json` | Optional VS Code C++ include/compiler configuration |

## Automation and CMake modules

| Path | Purpose |
| --- | --- |
| `.github/workflows/ci.yml` | PR/static, strict toolchain, sanitizer, coverage, fuzz, GPU, and packaging jobs |
| `.github/workflows/nightly.yml` | Extended TSan, fuzz, and 30-minute loopback soak jobs |
| `cmake/LarWarnings.cmake` | Baseline and optional strict compiler warnings |
| `cmake/LarHardening.cmake` | Supported compile/link hardening flags |
| `cmake/LarSanitizers.cmake` | ASan, UBSan, and TSan compatibility/configuration |
| `cmake/LarCoverage.cmake` | GCC/gcov instrumentation |
| `cmake/LarFuzzing.cmake` | libFuzzer detection, instrumentation, and standalone fallback |
| `cmake/LarMutation.cmake` | Mull IR frontend and instrumented target setup |
| `cmake/LarMapAssets.cmake` | GeoJSON-to-`.larmap` custom target and executable packaging |
| `cmake/LarPlaneAssets.cmake` | F-16/cubemap/water-mask executable-adjacent copy helper |
| `cmake/LarSbom.cmake` | Platform-specific componentized SPDX SBOM configuration |
| `cmake/LarQualityTargets.cmake` | Format, tidy, architecture, docs, install, performance, coverage, mutation gates |
| `cmake/lar-sbom.spdx.json.in` | SPDX 2.3 Qt module/plugin/C++ runtime inventory template |

## Assets and sample mappings

| Path | Purpose |
| --- | --- |
| `assets/README.md` | Single asset-tree ownership and packaging rules |
| `assets/map/README.md` | Natural Earth attribution and source/package behavior |
| `assets/map/world_boundaries.geojson` | Build-time country-boundary source; not installed directly |
| `assets/models/f16_3.glb` | Packaged glTF 2.0 F-16 model for Plane mode |
| `assets/cubemaps/*_{rt,lf,up,dn,ft,bk}.png` | Naturally ordered six-face cubemap sets |
| `assets/DTED0/{e|w}DDD/{n|s}DD.dt0` | Lazily addressed WGS84 Level-0 terrain cells; excluded from default staging/install |
| User-selected `{e|w}DDD/{n|s}DD.dt1` / `.dt2` | Session-only Level-1/Level-2 terrain trees accessed in place and never staged |
| `assets/ne_10m_land/*` | Natural Earth 1:10m build-time land polygons and required shapefile sidecars |
| `assets/water/dted0_water_mask.bin` | Generated indexed DTED-post water mask; staged and installed with Plane assets |
| `assets/water/README.md` | Mask format, provenance, and deterministic regeneration command |
| `assets/ui/icons/icons.qrc` | Stable `:/icons` and `:/styles` Qt resource aliases |
| `assets/ui/icons/folder.png` | Open/select action icon |
| `assets/ui/icons/lightning.png` | Burst analysis placeholder icon |
| `assets/ui/icons/navigator.png` | North/navigation overlay image |
| `assets/ui/icons/pause.png` | Recording pause-state icon |
| `assets/ui/icons/play-button-empty.png` | Recording continue-state icon |
| `assets/ui/icons/play.png` | Generic play action icon |
| `assets/ui/icons/reset.png` | Recording reset and replay-repeat icon |
| `assets/ui/icons/save.png` | Recording persistence icon |
| `assets/ui/icons/save2.png` | Alternate save action icon |
| `assets/ui/icons/trash.png` | Recording discard/reset icon |
| `assets/ui/styles/mainwindow.qss` | Application-wide embedded Qt stylesheet |
| `maps/full-state.json` | Complete legacy Plane/Target packet mapping |
| `maps/dlz-inputs.json` | Standalone three-field DLZ mapping |
| `maps/full-state-dlz.json` | Combined legacy and DLZ mapping |

## Domain source

| Path | Purpose |
| --- | --- |
| `src/domain/CMakeLists.txt` | `lar-domain` source and dependency declaration |
| `src/domain/state.h` | Fixed-layout `Plane` and `Target` protocol values |
| `src/domain/decoded_state.h` | Atomic decoded state plus field-availability mask |
| `src/domain/statefield.{h,cpp}` | Canonical scalar registry, accessors, mapping/display names, units, validation category |
| `src/domain/statevalidator.{h,cpp}` | Available-field scalar and cross-field validation |
| `src/domain/packet_mapping.{h,cpp}` | Immutable binding validation plus packet encode/decode |
| `src/domain/dlz/dlz_types.h` | DLZ shooter, target, atmosphere, geometry, solution, and HUD values |
| `src/domain/dlz/dlz_units.h` | DLZ unit conversion constants |
| `src/domain/dlz/dlz_geometry.{h,cpp}` | Relative engagement geometry derivation |
| `src/domain/dlz/dlz_solver.{h,cpp}` | Guarded fictional launch-zone equations and categorized errors |
| `src/domain/dlz/dlz_scenario_adapter.{h,cpp}` | Compact control inputs to complete DLZ frame adapter |

## Application source

| Path | Purpose |
| --- | --- |
| `src/application/CMakeLists.txt` | `lar-application` source and inward-only link declaration |
| `src/application/application_facade.{h,cpp}` | Main-thread presentation command facade, result gates, error translation |
| `src/application/application_state.h` | Facade-owned lifecycle and command-eligibility state |
| `src/application/application_view_model.{h,cpp}` | Observable main-thread display projection |
| `src/application/mode_coordinator.{h,cpp}` | Legal top-level mode state machine |
| `src/application/source_lifecycle_coordinator.{h,cpp}` | Online/playback switching, request correlation, epoch filtering |
| `src/application/captured_packet.h` | Accepted raw datagram and monotonic receipt timestamp |
| `src/application/session_limits.h` | LAR1 resource and duration limits |
| `src/application/session_timestamp.h` | Validated exact millisecond value object |
| `src/application/ip_access_policy.{h,cpp}` | Allow-all or canonical exact-address whitelist value |
| `src/application/ip_access_policy_service.{h,cpp}` | Policy loading use case through a repository port |
| `src/application/recording_operation_state.h` | Exclusive snapshot/final-save/reset operation state |
| `src/application/request_result_gate.h` | Template for synchronous/asynchronous latest-request correlation |
| `src/application/metrics_service.{h,cpp}` | Online/direct-runtime totals and elapsed-time rate windows |
| `src/application/online_capture_service.{h,cpp}` | Datagram decode, recording publication, and visual coalescing |
| `src/application/recording_service.{h,cpp}` | Recording transaction state and active-time calculation |
| `src/application/recording_pipeline_coordinator.{h,cpp}` | Drain, batch, snapshot, persistence, retry, and finalization policy |
| `src/application/playback_service.{h,cpp}` | Fixed-step indexed playback, binary-search sampling, seek, rate, and repeat policy |
| `src/application/direct_application_runtime.{h,cpp}` | Single-thread runtime implementation used by tests/embedders |

### Application ports and runtime protocol

| Path | Purpose |
| --- | --- |
| `src/application/ports/application_runtime.h` | Segregated runtime command interfaces and typed event boundary |
| `src/application/ports/runtime_messages.{h,cpp}` | Request IDs, epochs, result/event values, metatype registration |
| `src/application/ports/runtime_event_context.h` | Compatibility alias for `RuntimeSourceEpoch` |
| `src/application/ports/datagram_source.h` | Datagram transport port |
| `src/application/ports/mapping_repository.h` | Mapping file/JSON repository port |
| `src/application/ports/packet_decoder.h` | Mutable active-mapping decoder port |
| `src/application/ports/ip_access_policy_repository.h` | Validated sender-policy repository port |
| `src/application/ports/recording_clock.h` | Process-wide monotonic receipt-time port |
| `src/application/ports/recording_transaction.h` | Mutable append/reset/snapshot transaction port |
| `src/application/ports/session_snapshot.h` | Type-erased immutable recording snapshot port/value |
| `src/application/ports/session_persistence.h` | Atomic snapshot destination port |
| `src/application/ports/session_reader.h` | Validated random-access playback reader port |
| `src/application/ports/playback_clock.h` | Fixed-rate playback tick port |

## Infrastructure source

| Path | Purpose |
| --- | --- |
| `src/infrastructure/CMakeLists.txt` | `lar-infrastructure-qt` sources and adapter dependencies |
| `src/infrastructure/mapping/json_mapping_repository.{h,cpp}` | Bounded JSON mapping parser/repository adapter |
| `src/infrastructure/mapping/mapped_packet_decoder.{h,cpp}` | PacketMapping/StateValidator decoder adapter |
| `src/infrastructure/network/qt_ip_address_normalizer.{h,cpp}` | Exact canonical IPv4/IPv6 representation helper |
| `src/infrastructure/network/qt_ip_access_policy_repository.{h,cpp}` | Text-file whitelist adapter and parser |
| `src/infrastructure/network/qt_udp_datagram_source.{h,cpp}` | `QUdpSocket` ownership, sender filtering, receipt timestamps |
| `src/infrastructure/session/lar_session_writer.{h,cpp}` | Streaming temporary-file LAR1 transaction |
| `src/infrastructure/session/file_session_snapshot.{h,cpp}` | Immutable retained file-prefix snapshot |
| `src/infrastructure/session/qt_session_persistence.{h,cpp}` | Atomic `QSaveFile` snapshot commit |
| `src/infrastructure/session/lar_session_reader.{h,cpp}` | Full LAR1 validator with 4,096-record sparse checkpoints, one page cache, and lazy packet decode |
| `src/infrastructure/timing/qt_recording_clock.{h,cpp}` | `steady_clock` recording-clock adapter |
| `src/infrastructure/timing/qt_playback_clock.{h,cpp}` | Precise `QTimer` playback-clock adapter |
| `src/infrastructure/runtime/network_runtime_worker.{h,cpp}` | Network-thread composition, commands, capture batching, metrics |
| `src/infrastructure/runtime/recording_runtime_worker.{h,cpp}` | Session-thread recording composition and protocol translation |
| `src/infrastructure/runtime/playback_runtime_worker.{h,cpp}` | Session-thread reader/playback composition and protocol translation |
| `src/infrastructure/runtime/persistence_runtime_worker.{h,cpp}` | Persistence-thread atomic snapshot saves |
| `src/infrastructure/runtime/playback_publication_throttle.{h,cpp}` | Latest-frame/position coalescing before UI delivery |
| `src/infrastructure/runtime/playback_metrics.{h,cpp}` | Playback processed-record totals and rate windows |
| `src/infrastructure/runtime/threaded_application_runtime.{h,cpp}` | Production worker construction, affinity, relay, lifecycle, shutdown |

## Viewer shell and helpers

| Path | Purpose |
| --- | --- |
| `src/viewer/CMakeLists.txt` | Presentation library and viewer/test-sender executable targets |
| `src/viewer/main.cpp` | Production composition root and Qt event-loop entry |
| `src/viewer/mainwindow.{h,cpp}` | Fixed layout, panel/view composition, facade signal rendering, close policy |
| `src/viewer/lar_camera.h` | View and tracking mode enums plus camera value |
| `src/viewer/grid_geometry_builder.{h,cpp}` | Grid spacing/label geometry helper |
| `src/viewer/lar_projection.{h,cpp}` | Local tangent-plane metric projection |
| `src/viewer/lar_geometry_builder.{h,cpp}` | Grid-page LAR paths from target values |
| `src/viewer/lar_geodesic_geometry.{h,cpp}` | Spherical destination, normalization, and distance/bearing math |
| `src/viewer/terrain/dted_level.h` | DT0/DT1/DT2 metadata and active dataset value |
| `src/viewer/terrain/dted_cell.h` | Bounded immutable DTED cell and degree key |
| `src/viewer/terrain/dted_cell_reader.{h,cpp}` | Streaming level-aware UHL/profile/checksum and signed-magnitude parser |
| `src/viewer/terrain/dted_tile_source.{h,cpp}` | Direct WGS84 coordinate-to-cell path resolution |
| `src/viewer/terrain/dted_mosaic_sampler.{h,cpp}` | Bilinear sampling with bounded positive/negative tile cache |
| `src/viewer/terrain/dted_water_mask.h` | Immutable pure/mixed DTED-post water classification |
| `src/viewer/terrain/dted_water_mask_source.{h,cpp}` | Bounded indexed mask-pack validation and lazy payload reads |
| `src/viewer/statevalueformatter.{h,cpp}` | Available-field-aware protocol value formatting |
| `src/viewer/playbacktimeformatter.{h,cpp}` | Exact timestamp text formatting |
| `src/viewer/playback_timeline_mapper.{h,cpp}` | Overflow-safe slider/time conversion |
| `src/viewer/viewport_frame_counter.{h,cpp}` | Rendered-frame total and rate calculation |

### Dialogs, workflows, and panels

| Path | Purpose |
| --- | --- |
| `src/viewer/dialogs/viewer_file_dialog.h` | Mapping/policy/session selection port |
| `src/viewer/dialogs/qt_viewer_file_dialog.{h,cpp}` | Native Qt selection adapter |
| `src/viewer/dialogs/recording_file_dialog.h` | Save/reset/discard decision port |
| `src/viewer/dialogs/qt_recording_file_dialog.{h,cpp}` | Native Qt recording decision adapter |
| `src/viewer/workflows/online_workflow_controller.{h,cpp}` | Mapping and IP-policy choice/application workflow |
| `src/viewer/workflows/recording_workflow_controller.{h,cpp}` | Snapshot/final-save/reset/window-close workflow |
| `src/viewer/panels/online_panel.{h,cpp}` | Mapping, listener, whitelist, and recording widgets |
| `src/viewer/panels/offline_panel.{h,cpp}` | Session, sampled playback, repeat, rate, timeline, and Burst-placeholder widgets |
| `src/viewer/panels/values_panel.{h,cpp}` | Current-values/DLZ-values fixed column |
| `src/viewer/panels/metrics_panel.{h,cpp}` | Frame and packet metric widgets |

## Map subsystem

| Path | Purpose |
| --- | --- |
| `src/viewer/map/CMakeLists.txt` | Widget-free format and OpenGL rendering target split |
| `src/viewer/map/map_asset_format.h` | Versioned `.larmap` magic, header, offsets, and widths |
| `src/viewer/map/map_asset_limits.h` | Package size/count/coordinate resource limits |
| `src/viewer/map/map_mesh.h` | CPU geographic vertices and fill/border indices |
| `src/viewer/map/map_checksum.h` | Header-only IEEE CRC-32 implementation |
| `src/viewer/map/map_projection.{h,cpp}` | Mercator conversion and periodic-longitude helpers |
| `src/viewer/map/map_asset_reader.{h,cpp}` | Bounded file/data parser with categorized errors |
| `src/viewer/map/map_asset_source.h` | Immutable map package source port |
| `src/viewer/map/packaged_map_asset_source.{h,cpp}` | Executable-adjacent package/manifest resolver |
| `src/viewer/map/map_palette.h` | Central normalized rendering colors |
| `src/viewer/map/map_shaders.h` | Core/legacy GLSL source strings |
| `src/viewer/map/map_camera.{h,cpp}` | Flat/sphere camera projection, pan, zoom, picking, visible copies |
| `src/viewer/map/earth_map_gpu_renderer.{h,cpp}` | OpenGL program/buffer lifecycle and map draw calls |
| `src/viewer/map/earth_map_widget.{h,cpp}` | Interactive OpenGL widget and camera event translation |

## Viewport subsystem

| Path | Purpose |
| --- | --- |
| `src/viewer/viewport/lar_scene_state.h` | Plane/Target/availability value passed to viewport pages |
| `src/viewer/viewport/lar_viewport_page.h` | Base and Earth-specific page interfaces plus event relay |
| `src/viewer/viewport/viewport_content_mode.h` | LAR, Plane, and DLZ content enum |
| `src/viewer/viewport/viewport_content_switch.{h,cpp}` | Accessible three-button content switch |
| `src/viewer/viewport/lar_viewport.{h,cpp}` | Grid/Earth/Plane/DLZ host, page switching, state/camera delegation |
| `src/viewer/viewport/viewport_controls.{h,cpp}` | Projection/tracking/rotation/fit controls |
| `src/viewer/viewport/viewport_camera_controller.{h,cpp}` | Projection-neutral tracking/free/rotation camera policy |
| `src/viewer/viewport/grid_camera_transform.{h,cpp}` | Grid world/screen transforms, pan, zoom |
| `src/viewer/viewport/grid_lar_view.{h,cpp}` | Local-grid page rendering and interaction |
| `src/viewer/viewport/earth_camera_policy.{h,cpp}` | Pure Earth tracking, fit, zoom-anchor, scale policy |
| `src/viewer/viewport/earth_lar_view.h` | Earth page ownership and public page contract |
| `src/viewer/viewport/earth_lar_view.cpp` | Earth page lifecycle, scene updates, asynchronous coordination |
| `src/viewer/viewport/earth_lar_view_interaction.cpp` | Mouse/wheel/double-click camera behavior |
| `src/viewer/viewport/earth_lar_view_rendering.cpp` | OpenGL overlay and fallback paint paths |
| `src/viewer/viewport/earth_map_load_controller.{h,cpp}` | Revision-safe single-flight packaged map load |
| `src/viewer/viewport/earth_overlay_coordinator.{h,cpp}` | Latest-only fallback mesh worker ownership |
| `src/viewer/viewport/viewport_preparation_worker.{h,cpp}` | Worker request/result values and mesh-build slot |
| `src/viewer/viewport/lar_zone_input_validator.{h,cpp}` | Field/domain/resource checks and IR/IZ normalization |
| `src/viewer/viewport/geodesic_zone_sampler.{h,cpp}` | Adaptive bounded spherical sample grid |
| `src/viewer/viewport/lar_zone_mesh_assembler.{h,cpp}` | Mercator/sphere vertices and bounded index ranges |
| `src/viewer/viewport/lar_zone_mesh_builder.{h,cpp}` | Validate/sample/assemble orchestration |
| `src/viewer/viewport/lar_zone_mesh.h` | Shared CPU vertex/index/draw-range value |
| `src/viewer/viewport/lar_zone_mesh_limits.h` | CPU zone resource and accuracy limits |
| `src/viewer/viewport/lar_zone_gpu_layer.{h,cpp}` | Prepared-mesh OpenGL owner/draw path |
| `src/viewer/viewport/lar_parametric_zone_gpu_layer.{h,cpp}` | Shader-parametric geodesic zone draw path |
| `src/viewer/viewport/lar_marker_layer.{h,cpp}` | Aircraft/target overlay marker widget |
| `src/viewer/viewport/lar_navigation_overlay.{h,cpp}` | North indicator and scale-bar widget |
| `src/viewer/viewport/lar_view_fit.{h,cpp}` | Mercator/sphere fit calculations |

## Plane visualization

| Path | Purpose |
| --- | --- |
| `src/viewer/plane/plane_model_mesh.h` | Immutable interleaved model mesh and material draw ranges |
| `src/viewer/plane/glb_resource_limits.h` | Aggregate CPU/GPU model, buffer, and image resource budgets |
| `src/viewer/plane/glb_resource_reader.{h,cpp}` | Shared data-URI/GLB reader and canonical contained external-resource resolver |
| `src/viewer/plane/glb_model_reader.{h,cpp}` | Bounded glTF 2.0 mesh, UV, material, and node reader |
| `src/viewer/plane/glb_texture_reader.{h,cpp}` | Bounded embedded PNG/JPEG decoder and glTF sampler reader |
| `src/viewer/plane/cubemap_catalog.{h,cpp}` | Six-face cubemap grouping, natural ordering, validation, and loading |
| `src/viewer/plane/plane_attitude_transform.{h,cpp}` | Telemetry Euler-to-model quaternion conversion |
| `src/viewer/plane/plane_orbit_camera.{h,cpp}` | Heading-relative orbit/chase camera policy |
| `src/viewer/plane/plane_aircraft_scale.{h,cpp}` | Fixed-aircraft metric surface-unit policy |
| `src/viewer/plane/plane_shaders.h` | Legacy/core mesh and cubemap GLSL sources |
| `src/viewer/plane/plane_surface_state.h` | Local metric ground, grid, target, and LAR draw values |
| `src/viewer/plane/plane_surface_projection.{h,cpp}` | Aircraft-relative tangent projection and stable scale policy |
| `src/viewer/plane/plane_surface_shaders.h` | Legacy/core ground, grid, target, and flat-zone GLSL sources |
| `src/viewer/plane/plane_surface_gpu_layer.{h,cpp}` | Static-topology tactical surface OpenGL layer |
| `src/viewer/plane/plane_terrain_patch*.{h,cpp}` | Bounded local DTED mesh values and CPU patch builder |
| `src/viewer/plane/plane_terrain_worker.{h,cpp}` | Latest-only terrain thread worker and stale-result filtering |
| `src/viewer/plane/plane_terrain_gpu_layer.{h,cpp}` | Context-owned terrain upload and lit hypsometric draw pass |
| `src/viewer/plane/plane_terrain_shaders.h` | Legacy/core DTED terrain GLSL sources |
| `src/viewer/plane/plane_scene_renderer.{h,cpp}` | Aircraft, tactical surface, terrain, and cubemap OpenGL renderer |
| `src/viewer/plane/plane_scene_widget*.cpp` | OpenGL lifecycle, DTED coordination, input, and diagnostics |
| `src/viewer/plane/plane_scene_widget.h` | Plane scene widget API and owned renderer/terrain lifecycle state |
| `src/viewer/plane/plane_view_workspace.{h,cpp}` | Viewport-page adapter and terrain/surface/skybox controls |

## DLZ HUD presentation

| Path | Purpose |
| --- | --- |
| `src/viewer/hud/dlz_control_panel.{h,cpp}` | Input mode, scenario sliders, readouts, persistent diagnostics |
| `src/viewer/hud/dlz_hud_workspace.{h,cpp}` | Input arbitration, domain rebuild, temporal tick/presentation coordination |
| `src/viewer/hud/dlz_presentation_controller.{h,cpp}` | Filtering, scale selection, mode timing, shoot-cue state |
| `src/viewer/hud/dlz_range_scale.{h,cpp}` | Range-to-pixel conversion and hysteretic scale steps |
| `src/viewer/hud/dlz_hud_view.{h,cpp}` | Paint-ready state widget |
| `src/viewer/hud/dlz_hud_renderer.{h,cpp}` | Stateless QPainter symbology |
| `src/viewer/hud/dlz_fixtures.{h,cpp}` | Named deterministic render fixtures |

## Sender source

| Path | Purpose |
| --- | --- |
| `src/testsender/README.md` | Complete sender tutorial, scenarios, protocol behavior, customization |
| `src/testsender/main.cpp` | Maintained scenario sender CLI and UDP loop |
| `src/testsender/scenarios.{h,cpp}` | Deterministic state initialization/update and scenario catalog |
| `src/testsender/custom_sender.cpp` | Always-built editable scenario/malformed-packet template |
| `src/testsender/scenario_tests.cpp` | Determinism, validity, bounds, and scenario coverage tests |

## Repository and map tools

| Path | Purpose |
| --- | --- |
| `tools/CMakeLists.txt` | Map compiler/support and fuzz target declarations |
| `tools/check_architecture.py` | Include, Qt side-effect, CMake link, and source-size architecture gate |
| `tools/check_doc_links.py` | Repository-local Markdown link validator |
| `tools/check_doxygen_coverage.py` | Production-header `@file`/`@brief` coverage gate |
| `tools/check_doc_quality.py` | Generated-filler rejection and semantic security/lifetime/resource documentation gate |
| `tools/check_install_layout.py` | Installed executable/map/Plane-assets/mapping/component-SBOM smoke validator |
| `tools/check_performance.py` | Current-versus-baseline benchmark regression evaluator |
| `tools/enforce_coverage.py` | Per-layer and total gcovr threshold gate |
| `tools/map_asset/main.cpp` | Compiler CLI argument/error entry point |
| `tools/map_asset/source_map.h` | Parsed coordinate/ring/polygon/source-map values |
| `tools/map_asset/map_source_limits.h` | GeoJSON size/depth/feature/coordinate limits |
| `tools/map_asset/geojson_source_reader.{h,cpp}` | Bounded GeoJSON reader/parser |
| `tools/map_asset/polygon_triangulator.{h,cpp}` | Deterministic ring cleanup and triangulation |
| `tools/map_asset/map_mesh_compiler.{h,cpp}` | Projection and packaged mesh construction |
| `tools/map_asset/map_asset_writer.{h,cpp}` | Binary package and JSON manifest writer |

## Tests

`tests/CMakeLists.txt` defines targets, labels, timeouts, offscreen settings,
fuzzers, benchmarks, and the optional native GPU tests.

| Path | Coverage |
| --- | --- |
| `tests/domain_tests.cpp` | State registry, mapping encode/decode, validation, availability |
| `tests/dlz_tests.cpp` | Geometry, supported domain, ordering, solver errors, scale/controller behavior |
| `tests/dlz_view_tests.cpp` | DLZ widgets, input arbitration, diagnostics, render fixtures |
| `tests/architecture_tests.cpp` | Modes, facade workflows, stale-source rejection, threaded composition |
| `tests/source_lifecycle_tests.cpp` | Online/playback transition races, generations, deferred publications |
| `tests/request_result_gate_tests.cpp` | Direct/queued/stale/duplicate request-result correlation |
| `tests/runtime_contract_tests.cpp` | Direct and threaded command/result/failure parity |
| `tests/recording_pipeline_tests.cpp` | Batching, drains, snapshots, retries, resets, final saves |
| `tests/playback_service_tests.cpp` | Fixed-frame advancement, logarithmic selection, repeat, seek validation, and failures |
| `tests/session_time_tests.cpp` | Exact timestamps, rounding, bounds, slider/time formatting |
| `tests/session_adapter_tests.cpp` | LAR1 writer/reader/snapshot/persistence contracts and corruption |
| `tests/network_adapter_tests.cpp` | UDP bind/receive, source filtering, timestamp and error behavior |
| `tests/ip_access_policy_tests.cpp` | Canonical address parsing and transactional policy replacement |
| `tests/operational_soak_tests.cpp` | Real loopback 100 Hz recording/persistence/replay and memory ceiling |
| `tests/polar_udp_tests.cpp` | End-to-end polar/dateline UDP state and LAR geometry |
| `tests/viewer_tests.cpp` | Main window, panels, resources, controls, camera/overlay UI behavior |
| `tests/viewport_camera_tests.cpp` | Projection-neutral and Grid/Earth camera policies |
| `tests/viewport_page_tests.cpp` | Page interface and viewport switching contracts |
| `tests/viewport_worker_tests.cpp` | Latest-only asynchronous preparation and bounded shutdown |
| `tests/lar_zone_mesh_tests.cpp` | IR/IZ validation, geodesic sampling, seam/pole mesh assembly, limits |
| `tests/map_asset_tests.cpp` | Packaged `.larmap` parsing, corruption, limits, manifest behavior |
| `tests/map_compiler_tests.cpp` | GeoJSON parsing, triangulation, deterministic compiler/writer output |
| `tests/map_renderer_validation_tests.cpp` | Shader and renderer input/state validation without native GPU execution |
| `tests/map_loading_tests.cpp` | Source abstraction, async load lifecycle, installation failure/retry |
| `tests/earth_lar_view_gpu_tests.cpp` | Opt-in native OpenGL integration and context/resource lifecycle |
| `tests/plane_view_tests.cpp` | GLB/cubemap, DT0/DT1/DT2 and water parser/sampler/worker, source switching, surface, attitude, camera, and controls |
| `tests/plane_view_gpu_tests.cpp` | Opt-in native Plane OpenGL, terrain, tactical-surface, and skybox validation |
| `tests/support/dted_fixture.h` | Synthetic checksummed DT0/DT1/DT2 cell builder shared by CPU and GPU tests |
| `tests/performance_benchmarks.cpp` | Mapping, recording, playback, map parsing, and zone preparation baselines |
| `tests/ui_snapshot_tool.cpp` | Deterministic offscreen UI image helper for manual visual review |

## Standalone source tools

| Path | Purpose |
| --- | --- |
| `tools/build_dted_water_mask.py` | GDAL/NumPy compiler from Natural Earth land polygons to the compact DTED0 mask pack |

### Test support and fuzzing

| Path | Purpose |
| --- | --- |
| `tests/support/architecture_test_support.h` | Shared fake ports and direct facade composition |
| `tests/support/controlled_application_runtime.h` | Manually completed runtime for lifecycle race tests |
| `tests/support/direct_runtime_harness.h` | Reusable direct-runtime adapter composition |
| `tests/support/fake_datagram_source.h` | Deterministic datagram port fake |
| `tests/tsan_suppressions.txt` | Narrow third-party/runtime TSan suppressions |
| `tests/fuzz/fuzz_standalone_main.cpp` | Deterministic mutator fallback where libFuzzer is unavailable |
| `tests/fuzz/packet_decoder_fuzz.cpp` | Binary packet decode/validation fuzz entry |
| `tests/fuzz/mapping_repository_fuzz.cpp` | Mapping JSON fuzz entry |
| `tests/fuzz/ip_policy_fuzz.cpp` | IP policy text fuzz entry |
| `tests/fuzz/session_reader_fuzz.cpp` | LAR1 parser/index fuzz entry |
| `tests/fuzz/map_asset_reader_fuzz.cpp` | `.larmap` parser fuzz entry |
| `tests/fuzz/geojson_source_reader_fuzz.cpp` | GeoJSON source parser fuzz entry |
| `tests/fuzz/dlz_solver_fuzz.cpp` | DLZ finite/order/error-domain fuzz entry |
| `tests/fuzz/corpus/*` | Minimal persisted seeds for IP, JSON, mapping, and binary magic paths |

## Documentation files

| Path | Purpose |
| --- | --- |
| `docs/README.md` | Documentation hub and generated API instructions |
| `docs/DOCUMENTATION_REVIEW_REPORT.md` | Evidence-backed documentation and source-comment audit |
| `docs/ARCHITECTURE.md` | Layer, component, class, runtime, and target UML |
| `docs/SOLID_COMPLIANCE.md` | SOLID rationale, contracts, gates, and review checklist |
| `docs/COMPONENT_REFERENCE.md` | Semantic public API and collaboration index |
| `docs/FILE_REFERENCE.md` | This maintained-file inventory |
| `docs/DATA_FLOWS.md` | Runtime sequence diagrams and flow invariants |
| `docs/CONCURRENCY_MODEL.md` | Threads, ownership, protocols, drains, shutdown |
| `docs/PROTOCOL_UNITS.md` | Packet field names, units, availability, validation |
| `docs/LAR1_FORMAT.md` | Persisted session layout and parser/writer contract |
| `docs/DLZ_MODEL.md` | Teaching-model math and presentation state |
| `docs/DEVELOPER_GUIDE.md` | Build and extension workflows |
| `docs/QUALITY_GATES.md` | Static/dynamic verification matrix |
| `docs/THREAT_MODEL.md` | Assets, trust boundaries, abuse cases, mitigations, residual risks, and review triggers |
| `docs/Doxyfile.in` | Configured exhaustive Doxygen/Graphviz site definition |
