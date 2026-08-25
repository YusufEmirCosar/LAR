# Project Audit Report

**Project:** LAR Packet Monitor  
**Audit date:** 2026-08-24  
**Scope:** Architecture, visualization correctness, security, memory and concurrency,
documentation/comments, build and test evidence  
**Audit type:** Static review plus local dynamic verification of the supplied source snapshot

## Remediation update

The following changes were made on 2026-08-24 in response to the requested
findings. The original baseline narrative below is retained so the reason for
each control remains auditable; it describes the pre-remediation snapshot and
must not be read as current implementation status.

**Current status:** all eight requested remediation items are implemented and
their release gates pass in this workspace.

| Requested item | Current control | Regression evidence |
| --- | --- | --- |
| A-03 glTF exhaustion | Shared limits cap the model/resource files, aggregate logical buffers, encoded images, decoded/GPU image bytes, buffer/texture counts, and dimensions; repeated references consume aggregate budget and candidate commit is transactional | External-buffer aggregate and repeated-texture amplification tests |
| Strict `qsizetype` conversion | `QString`/Qt container sizes and positions remain `qsizetype` or are converted only after range proof; strict conversion builds are the release gate | GCC/Clang warnings-as-errors builds |
| A-05 path escape | One buffer/image resolver rejects absolute/traversal/query/fragment/NUL syntax, symlink components, non-regular files, and canonical targets outside the selected model directory | Traversal, absolute and symlink rejection tests |
| A-06 shutdown race | Worker shutdown, QObject-tree rehoming, thread join, and deletion are synchronous; worker timers are QObject children and no lifetime depends on post-exit `deleteLater` | Repeated QPointer destruction test and TSan runtime contract |
| A-07 extreme zoom | GPU eligibility proves fixed tessellation and float rounding against the 0.65-pixel contract; sensitive cases use adaptive camera-relative CPU meshes | Quantitative extreme-zoom vertex/chord reference test plus native GPU tests |
| A-08 dependency/SBOM | CMake requires Qt 6.10.3+, CI installs 6.11.2, and SPDX enumerates direct Qt modules, platform plugin, PNG/JPEG handlers, package URLs, and relationships | Install-layout SBOM schema/component validation and OSV scan job |
| Session record limit / A-09 | The 10-million product cap and per-record index were removed; counts/indices are 64-bit and the reader stores one checkpoint per 4,096 records plus one 4,096-location page | Cross-page sparse random-access test and session adapter suite |
| A-11 documentation | Specification and threat model added; stale LAR1/concurrency/Qt prose corrected; 1,098 generated filler blocks removed and a semantic documentation gate added | Link, coverage, semantic-quality, and warning-free Doxygen gates |

### Remediation verification

| Gate | Current result |
| --- | --- |
| Strict optimized build | Pass; warnings, sign conversions, and implicit Qt-size conversions are errors |
| Registered deterministic suite | 28/28 pass; native GPU cases take their documented skip path in this headless run |
| Native OpenGL smoke | Earth and Plane suites both pass with real macOS OpenGL contexts |
| ASan + UBSan preset | 22/22 selected tests pass with the checked-in preset |
| TSan preset | 9/9 selected concurrency/contract tests pass with no project suppression added for shutdown |
| Install and SPDX validation | Pass; installed application, runtime assets, platform plugin, Qt modules, image handlers, package URLs, and dependency relationships validated |
| Repository gates | Pass; architecture, format, 17-document links, 159-header Doxygen coverage, semantic comment quality, and warning-free Doxygen generation |

These results were produced from the remediated source on 2026-08-24. The two
native GPU tests were run separately because ordinary CI deliberately skips
desktop-context tests unless their explicit environment gates are enabled.

The original A-01, A-02, and A-10 observations were outside the requested
remediation set and remain documented residual risks in the
[threat model](THREAT_MODEL.md).

## Executive summary

The project has a sound ports-and-adapters design, unusually broad system documentation,
defensive binary parsers, bounded recording and rendering pipelines, and a substantial automated
test matrix. All 28 registered tests passed when the optional native-GPU and operational-soak
tests were explicitly enabled in focused runs. A diagnostic ASan/UBSan build also passed all 28
tests after strict conversions and warnings-as-errors were temporarily disabled.

The snapshot is **not ready to be treated as release-certified**, however. The checked-in strict,
ASan/UBSan, and TSan configurations do not compile because of two signed-size conversions. The
repository architecture target also fails because one source file exceeds its hard size ceiling.
After relaxing only those compilation gates, TSan produced an unresolved shutdown/lifetime report.

The most important product risks are:

1. UDP telemetry is unauthenticated, accepts all sources by default, and provides no replay
   protection.
2. Every malformed datagram can create a distinct cross-thread UI error event; those events are
   not bounded or coalesced, allowing memory and CPU exhaustion.
3. glTF loading has no aggregate buffer budget and occurs synchronously on the UI thread. A valid
   model can request roughly 1 GiB of retained buffer data plus hundreds of MiB of decoded texture
   data.
4. glTF external-resource paths are not contained to the selected model directory.
5. The primary parametric GPU LAR overlay uses fixed tessellation and single-precision geographic
   inputs. Its visual error can exceed the CPU renderer's stated 0.65-pixel target at extreme zoom,
   and current GPU tests do not measure positional error.
6. The CI Qt version is pinned to 6.8.3, which is affected by published QtCore and image-decoder
   advisories, while the declared minimum Qt 6.5 reached end of standard support in April 2026.

No critical vulnerability, conventional heap corruption, leak, use-after-free, or undefined
behavior was reproduced by the exercised ASan/UBSan tests. This is not equivalent to proving their
absence, especially on untested file/network inputs.

### Risk register

| ID | Severity | Area | Finding | Release disposition |
| --- | --- | --- | --- | --- |
| A-01 | High | Security / integrity | UDP accepts unauthenticated and replayable telemetry; default policy is allow-all | Fix or formally constrain deployment |
| A-02 | High | Security / memory | Malformed UDP errors can create an unbounded queued-event backlog | Fix before exposure to untrusted networks |
| A-03 | High | Memory / availability | glTF buffers have per-item but no aggregate budget; texture and GPU copies add substantial peak memory | Fix before loading untrusted models |
| A-04 | High | Quality gates | Strict and shipped sanitizer builds do not compile; architecture gate is red | Fix before release certification |
| A-05 | Medium | Security | glTF relative URIs can traverse outside the model directory or follow symlinks | Fix trust boundary |
| A-06 | Medium | Concurrency | Relaxed TSan run reports a runtime shutdown/lifetime race; Qt synchronization visibility makes root cause unresolved | Minimize and resolve or narrowly justify suppression |
| A-07 | Medium | Visualization | GPU overlay precision and fixed tessellation do not meet a demonstrated pixel-error contract at extreme zoom | Add quantitative GPU accuracy gate and adapt rendering |
| A-08 | Medium | Dependencies | CI uses a Qt release with known advisories; minimum is out of standard support; SBOM is too coarse | Upgrade and inventory deployed components |
| A-09 | Medium | Memory | Session reader can reserve about 240 MiB for a hostile but structurally valid index | Introduce byte budget or paged index |
| A-10 | Low | Visualization contract | Grid and Plane are intentionally flat metric views, but the shared validator accepts global-scale radii without clearly labelling that model | Document Euclidean flat-view semantics and keep large-radius tessellation/resource limits explicit |
| A-11 | Low | Documentation | Coverage is broad but comment quality is boilerplate-heavy; threat model is missing and one review is stale | Improve semantic documentation controls |

Severity reflects a desktop telemetry viewer that may ingest network traffic and user-selected
models. In a strictly isolated demonstration environment, A-01 through A-03 have lower practical
exposure; on an operational or shared network they are release blockers.

## Scope and limitations

Reviewed inputs included production C++/headers under `src/`, map compiler code under `tools/`,
tests, CMake/presets, GitHub Actions, assets and primary Markdown/Doxygen documentation.

The workspace was not recognized as a Git checkout. Consequently this audit could not evaluate
history, provenance, blame, release tags, submodule state, or whether findings are regressions.
Static analyzers `clang-tidy`, `cppcheck`, `scan-build`, Valgrind, and the configured coverage tool
were unavailable locally. No current coverage artifact was present, so the documented 90/80 and
95/90 line/branch thresholds were not independently verified. Natural Earth/map source data,
DTED terrain, weapon-model physics, and the F-16 asset were not compared with authoritative
external ground truth.

## Verification results

| Check | Result | Evidence / qualification |
| --- | --- | --- |
| Release build | Pass | `cmake --build --preset release --parallel 4` |
| Registered tests | Pass | All 28 passed across deterministic, explicit 10-second loopback soak, and two explicit native OpenGL runs |
| Native Earth and Plane GPU tests | Pass | Both ran with native desktop/OpenGL rather than taking their normal skip path |
| Operational soak | Pass | 100 Hz capture/record/replay; RSS stayed under the test's 256 MiB growth ceiling |
| ASan + UBSan | Diagnostic pass | All 28 unique tests passed after disabling strict conversions and warnings-as-errors solely to make the instrumented build compile |
| Checked-in ASan/UBSan preset | **Fail** | Does not compile due to two `qsizetype` to `int` conversions |
| Checked-in strict `ci` build | **Fail** | Same two conversions are errors under `-Werror` |
| Architecture executable | Pass | Runtime substitution and architecture contracts pass |
| Repository architecture target | **Fail** | `glb_model_reader.cpp` is 723 lines; gate maximum is 600 |
| Relaxed diagnostic TSan selection | **Fail** | 8/9 selected tests passed; runtime contract aborted on a shutdown/lifetime report |
| Documentation coverage/link/Doxygen gates | Pass | 157/157 production headers satisfy the structural gate; 15 primary links valid; Doxygen emitted zero warnings |
| Install-layout smoke | Pass | Packaged runtime assets found in expected layout |
| SBOM generation | Pass, incomplete inventory | SPDX generated, but Qt is represented as one generic component |
| Visual UI smoke | Pass with limitation | Grid and DLZ snapshots rendered cleanly offscreen; Plane view correctly reported that native OpenGL is required; native Plane GPU test passed separately |

The strict conversion failures are at
[`glb_model_reader.cpp`](../src/viewer/plane/glb_model_reader.cpp) line 515 and
[`glb_texture_reader.cpp`](../src/viewer/plane/glb_texture_reader.cpp) line 51.
Both assign `QString::indexOf()`'s `qsizetype` result to `int`.

## Architecture review

### Structure and dependency direction

The implementation follows the documented layering:

```text
viewer/presentation ──> application ──> domain
          │                    ▲
          └── composition ──> infrastructure ──> domain
```

- `domain` contains state, field/mapping rules, validation and the isolated DLZ model.
- `application` owns lifecycle, use-case services, recording/playback coordination, ports and the
  view model.
- `infrastructure` implements UDP, JSON mapping, session persistence, clocks and threaded workers.
- `viewer` owns widgets, camera/geometry/rendering and the application composition root.
- `tools/map_asset` compiles source geography into the runtime package without entering the viewer
  link graph.

A direct include scan found no dependency cycle. Cross-layer edges point inward as intended:
application to domain; infrastructure to application/domain; and viewer to
application/domain/infrastructure. The only viewer-to-infrastructure includes are in the
composition root. This agrees with [the declared architecture](ARCHITECTURE.md).

Strong design choices include typed request identifiers and source epochs, immutable state
snapshots, atomic `QSaveFile` writes, a direct runtime for deterministic tests, bounded recording
batches, latest-only state publication, explicit worker shutdown, bounded terrain/mesh caches and
transactional decode/load operations.

### Concentration and enforcement gaps

The architecture is good, but several coordination surfaces are too concentrated:

| File | Physical lines | Observation |
| --- | ---: | --- |
| `src/viewer/plane/glb_model_reader.cpp` | 723 | Fails the enforced 600-line limit; URI, data-URI, buffer and model assembly concerns are combined |
| `src/application/application_facade.h` | 667 | Large public coordination surface; 526 lines are comments and 115 are code |
| `src/application/source_lifecycle_coordinator.cpp` | 573 | Central source state machine; change blast radius is high |
| `src/infrastructure/runtime/threaded_application_runtime.h/.cpp` | 501 / 473 | Thread wiring, command protocol and shutdown are tightly coupled |

The repository checker is useful but shallow: it uses source/include/link and size rules rather
than a complete semantic dependency graph. It cannot establish QObject thread affinity, indirect
generated dependencies, ownership correctness or absence of architectural cycles introduced
through abstractions. Keep the gate, but add a generated target dependency graph and explicit
thread-affinity/lifetime tests.

Recommended refactor: move glTF URI containment/data-URI decoding, aggregate resource accounting,
and buffer resolution into separate bounded components. Split facade/runtime APIs by mapping,
capture, recording and playback capabilities where callers do not need the full surface.

## Visualization correctness

### Coordinate and unit contracts

The core contracts are internally consistent: latitude/longitude and Euler angles use radians,
altitude and radii use metres, and velocity uses km/h. State validation rejects non-finite and
out-of-range geographic values. Documentation and tests agree on those units.

The Earth implementation uses the standard great-circle destination formula with a 6,371,000 m
Earth radius and Web Mercator's ±85.05112878° latitude clamp. CPU tests exercise dateline/polar
seams, hostile inputs and view bounds. The adaptive CPU sampler verifies geodesic boundary error
below one metre, and its mesh budgets are explicit: 12,000 vertices, 48,000 indices, at most 2,048
angular samples, and a 0.65-pixel curve-error target.

The map camera, zone validation, spherical navigation and Plane attitude conventions have strong
unit/integration coverage. Representative native GPU runs found the expected overlay colors and
camera orientation.

### Accuracy gaps

The primary parametric GPU overlay uses 24 radial by 128 angular segments and converts geographic
centre, radii and angular parameters from `double` to shader `float`; see
[`lar_parametric_zone_gpu_layer.cpp`](../src/viewer/viewport/lar_parametric_zone_gpu_layer.cpp).
It does not share the adaptive CPU renderer's pixel-error contract.

At the allowed maximum Mercator zoom of 100,000, a 900-pixel viewport spans approximately
0.00498° of longitude. Around 180°, one float32 ULP is approximately 0.00001526°; centre rounding
alone can therefore approach 1.38 pixels, before independent camera/zone rounding or radius error.
For a full 10 km circle at the same zoom, 128 fixed segments can introduce roughly 4.9 pixels of
sagitta error. These are analytical upper-bound examples, not observed corruption, but they show
that the stated CPU accuracy target does not carry over to the primary GPU path.

Current GPU tests are presence tests: they establish that overlays render, not that boundaries are
in the correct pixels. Add framebuffer comparisons against double-precision CPU reference points
at the dateline, Mercator limit, minimum/maximum zoom and minimum/maximum radii. Use adaptive
tessellation based on projected radius and keep positions camera-relative/high-low split through
the complete shader input path.

Grid and Plane surfaces intentionally use a flat metric presentation:
`east = R * deltaLongitude * cos(originLatitude)` and
`north = R * deltaLatitude`, followed by Euclidean rings. That is correct for the declared
tactical-plane model; it should not be compared with WGS 84 geodesic boundaries. The shared input
validator permits radii up to 20,000 km, so the UI should disclose that this is a Euclidean flat-view
distance and keep the separate tessellation/resource budget visible. Mercator and Sphere remain the
geographic global views; an Earth-distance mode would require a separate geodesic construction.

The DLZ panel is accurate to its documented deterministic teaching equations and fixtures. It is
explicitly a fictional/toy model, not a certified operational weapon envelope. The audit did not
independently certify geographic source data, terrain elevation, aircraft dimensions, or DLZ
physics against external truth.

## Security review

### A-01 — Unauthenticated, replayable UDP telemetry (High)

[`QtUdpDatagramSource`](../src/infrastructure/network/qt_udp_datagram_source.cpp) binds to any
address and requests shared/reusable binding. The README and UI make **Allow all** the default.
The optional exact-IP whitelist reduces accidental sources but is not authentication. There is no
message authentication code, encryption, sender identity, monotonically increasing sequence, or
replay window.

Any host able to reach the listener can inject or replay a syntactically valid packet and change
the displayed/recorded state. Recommended controls:

- default to loopback or deny-until-policy-selected;
- use exclusive binding unless shared binding is a deliberate requirement;
- add a versioned authenticated envelope with sender identity, sequence and timestamp/replay
  checks, or constrain deployment to an authenticated tunnel/segmented trusted network;
- show an unmistakable untrusted-input indicator when authentication is absent.

### A-02 — Unbounded malformed-datagram error queue (High)

[`OnlineCaptureService`](../src/application/online_capture_service.cpp) emits a distinct error for
every decode failure. [`NetworkRuntimeWorker`](../src/infrastructure/runtime/network_runtime_worker.cpp)
forwards every error, and [`ThreadedApplicationRuntime`](../src/infrastructure/runtime/threaded_application_runtime.cpp)
queues it to the UI thread. Valid state publication and recording data are batched/bounded, but
error publication is not. The UDP adapter drains up to 1,024 packets or two milliseconds and
immediately schedules another drain.

A malformed-packet flood can therefore allocate queued `QString`/Qt metacall events faster than
the UI consumes them, increasing RSS and making the interface unresponsive. Coalesce errors by
source/code and time window, bound the pending diagnostic queue, count dropped diagnostics, and
apply ingress rate limiting before formatting strings. Add an adversarial soak that floods invalid
packets and asserts both UI responsiveness and a much tighter RSS ceiling.

### A-03/A-05 — glTF resource exhaustion and path escape (High / Medium)

[`glb_model_reader.cpp`](../src/viewer/plane/glb_model_reader.cpp) permits 32 buffers of up to
32 MiB each, but eagerly retains them with no total-byte limit. Reusing the same large external
file in 32 buffer entries can approach 1 GiB. Texture limits permit 64 million decoded pixels,
roughly 256 MiB at RGBA8 before original bytes, GPU upload and mipmaps. The load is invoked
synchronously by [`PlaneSceneWidget`](../src/viewer/plane/plane_scene_widget.cpp), so a valid model
can freeze the UI or terminate the process under memory pressure.

External buffer and image URIs reject URL schemes, hosts and NUL bytes, but then join the decoded
path to the source directory without canonical containment. `../`, absolute-path behavior and
symlink traversal can read outside the selected model package. The application has no identified
exfiltration channel, so the immediate impact is unintended local-file parsing/rendering and
availability rather than proven disclosure to a remote party.

Add one canonical direct-child resolver for both buffers and images, reject absolute/traversal
paths, require regular files, define symlink policy, and test canonical containment. Add a total
resource-byte budget, decoded-image byte budget, URI deduplication/cache, cancellation and
allocation-failure handling. Parse/decode off the UI thread and publish only an immutable bounded
result.

### A-08 — Dependency and SBOM posture (Medium)

The project declares Qt 6.5 as its minimum, CI is pinned to Qt 6.8.3, and the local audit used Qt
6.11.1. Qt's official advisories state that:

- [CVE-2025-5455 in `qDecodeDataUrl` affects Qt 6.6 through 6.8.3 and is fixed in 6.8.4](https://www.qt.io/blog/security-advisory-recently-discovered-issue-in-qdecodedataurl-in-qtcore-impacts-qt).
- [CVE-2025-5683 in ICNS image handling affects releases through 6.8.4 and is fixed in 6.8.5](https://www.qt.io/blog/security-advisory-recently-discovered-issue-in-icns-image-format-handling-impacts-qt).
- [Qt 6.5 reached end of standard support in April 2026](https://www.qt.io/blog/qt-6.5-reaches-end-of-support).

The CI version is therefore known-vulnerable even though it is not proof that production ships
that exact runtime. Because glTF textures enter Qt image decoding, decoder/plugin inventory is part
of the attack surface. Pin CI and release packaging to a currently supported patched Qt line and
record the exact deployed runtime/plugins.

The generated SPDX lists the application, AppleClang runtime and a single generic Qt 6.11.1
package. It does not enumerate QtCore, QtGui, QtNetwork, OpenGLWidgets, platform/image plugins or
their transitive libraries. Generate the SBOM from the deployed app bundle, componentize Qt and
plugins, include hashes/purl/CPE where available, and scan that artifact rather than only the build
model.

### Positive security controls

No embedded secret, subprocess/shell execution, or unchecked legacy C string routine was found.
Binary/session/map parsers use explicit bounds, numeric validation and format limits. Map packages
and cubemap assets already demonstrate better containment/checksum patterns that can be reused for
glTF. Session/map saves use atomic replacement. Seven fuzz targets cover mapping, packet, session,
map and terrain surfaces, although long-duration/current-corpus fuzz evidence was not available in
this snapshot.

Session files and raw UDP packets have no cryptographic integrity or origin proof. Parsers can
detect malformed structure, but coherent tampering remains indistinguishable from legitimate data.

## Memory and concurrency review

ASan/UBSan found no error across all 28 tests in the relaxed diagnostic build, including the two
native GPU tests. The 10-second operational loopback soak passed its 256 MiB RSS-growth ceiling.
Ownership is generally clear (`QObject` parents and smart pointers), renderer cleanup is
context-aware, terrain caches are capped, viewport publication is latest-only, and recording uses
2 MiB/2,048-packet batches with eight outstanding writes and a 32 MiB input buffer.

These results do not cover the design-level exhaustion cases in A-02/A-03. An additional bounded
but large allocation exists in [`lar_session_reader.cpp`](../src/infrastructure/session/lar_session_reader.cpp):
up to ten million index records may be reserved. With the current record layout/padding this is
approximately 240 MiB before other session state. Replace the count-only cap with a lower total
byte budget, overflow-safe accounting and, for large legitimate sessions, a paged/lazy index.

The relaxed TSan preset ran nine contract/concurrency-labelled tests. Eight passed; the runtime
contract aborted while the next stack-allocated `ThreadedApplicationRuntime` was being constructed
and a previous worker thread was reported reading the prior object's Qt meta-object during
`QObject` destruction. Focused runs also expose reports at queued Qt metatype/callable boundaries;
the project already suppresses one documented prebuilt-Qt metatype pattern because macOS TSan
cannot observe all Qt synchronization.

This audit cannot responsibly label the remaining trace either a confirmed production race or a
false positive. Minimize it outside QtTest, run against a TSan-instrumented Qt build if possible,
and make worker deletion explicitly synchronous before stack reuse. Only then either fix the
lifetime ordering or add a narrow, frame-specific suppression with a regression test. A broad
suppression would hide application races.

## Comments and documentation

### Measured volume

Counts exclude build outputs and generated documentation.

| Scope | Files | Code lines | Comment lines | Blank lines / notes |
| --- | ---: | ---: | ---: | --- |
| Production `.cpp` | 118 | 17,882 | 75 | 2,097 |
| Production headers | 157 | 5,526 | 10,993 | 1,391 |
| Production Python tools | 7 | 743 | 12 | 102 |
| Production total | 282 | 24,151 | 11,080 | 3,590 |
| C++ tests | 41 | 7,055 | 594 | 928 |

Production C/C++ contains 275 files and 37,964 physical lines. The 13 files in `docs/` contain
3,020 lines and 19,553 words. Adding the root README and project specification yields 15 primary
documents, 3,628 lines and 23,420 words. All non-generated project Markdown totals 19 files,
4,042 lines and 26,295 words.

### Quality assessment

Documentation breadth is strong: architecture, SOLID evidence, data flows, concurrency, units,
session format, DLZ semantics, components/files, development and quality gates are all covered.
The link gate passes, Doxygen emits no warning, and every one of 157 production headers satisfies
the configured structural documentation gate. No `TODO`, `FIXME`, `HACK`, `XXX` or `BUG` marker
was found.

The volume metric overstates semantic coverage. Implementation files contain only 75 comment lines
for 17,882 code lines (0.42%), while headers contain almost twice as many comment lines as code.
At least 220 comments repeat “observes the current object...” and 113 repeat “exposes the stable
behavior...”. The Doxygen configuration uses `EXTRACT_ALL=YES` and
`WARN_IF_UNDOCUMENTED=NO`; the custom gate only requires one file-level marker per header.
Consequently a green gate proves presence, not useful API contracts, invariants, ownership,
thread-affinity or failure semantics.

[`DOCUMENTATION_REVIEW_REPORT.md`](DOCUMENTATION_REVIEW_REPORT.md) is also stale: it describes
missing/failing documentation gates and a missing project specification that now exists. Mark
historical reports as archived/date-scoped or regenerate them. Add a threat model covering UDP,
local model/session files, asset trust, Qt plugins, authentication assumptions and denial-of-service
budgets. Replace boilerplate with concise contracts at complex boundaries, particularly threading,
resource accounting, coordinate accuracy and error handling.

## Prioritized remediation plan

### P0 — before release certification

1. Fix both `qsizetype` conversions and restore green `ci`, ASan/UBSan and TSan compilation with
   strict warnings enabled.
2. Bound/coalesce malformed-datagram diagnostics and add a hostile invalid-UDP soak.
3. Decide and enforce the UDP trust model: authenticated envelope/tunnel, replay defense and a
   safe default policy.
4. Add canonical containment and aggregate CPU/GPU resource budgets to glTF loading; move it off
   the UI thread.
5. Split `glb_model_reader.cpp` below the architecture ceiling and make `check-repository` green.
6. Resolve the TSan lifetime report with a minimized reproducer or an instrumented-Qt validation.

### P1 — next hardening cycle

1. Upgrade the CI/release Qt baseline to a supported patched release and scan a componentized SBOM
   of the deployed bundle.
2. Add quantitative GPU image/reference tests and adaptive tessellation/precision handling at
   extreme zoom, seam and pole cases.
3. Replace the ten-million-entry session index allocation with byte budgeting/paging.
4. Run and preserve coverage, static-analysis, fuzz-duration/corpus and long (30-minute) soak
   artifacts in CI; do not infer current thresholds from documentation alone.
5. Add a security/threat-model document and state the non-operational status of the DLZ model in
   every user-facing/export context where confusion is plausible.

### P2 — maintainability and documentation

1. Reduce facade/runtime/lifecycle concentration and generate a machine-readable dependency graph.
2. Document the flat metric contract for Grid/Plane and add model-specific tests for large-radius
   tessellation, precision, and resource behavior; do not silently reinterpret them as globe views.
3. Replace boilerplate Doxygen with invariants, units, ownership, thread affinity, error behavior
   and performance budgets; make the gate sample semantic requirements rather than file markers.
4. Archive or update stale audit/review documents and record version/commit identifiers in future
   reports.

## Acceptance criteria for closure

The audit should be considered closed only when all checked-in strict/sanitizer/repository gates
compile and pass without local relaxations; the malformed-packet and hostile-model tests enforce
explicit memory/latency budgets; UDP trust assumptions are implemented and documented; TSan has a
reproducible disposition; deployed Qt components are patched and inventoried; and GPU boundary
position error is asserted numerically across the supported camera/input range.
