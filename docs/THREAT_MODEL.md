# Threat model

## Scope and security objectives

This model covers the LAR Packet Monitor desktop process, its installed runtime
assets, UDP input, mapping/session/model/map/DTED/image files, worker threads,
and GPU upload paths. It assumes the operating system, Qt runtime selected by
the release, and the user's account are not already compromised.

Security objectives are:

- keep untrusted input from escaping its selected file boundary;
- bound CPU memory, GPU memory, parser work, queues, and persistent storage work;
- prevent malformed input from becoming partially published application state;
- preserve session output atomically and avoid corrupting an existing file;
- keep worker and OpenGL lifetimes deterministic during shutdown;
- make telemetry authenticity limitations visible to operators and maintainers;
- retain enough dependency identity to respond to Qt and image-decoder
  advisories.

The application does not promise confidentiality for data displayed to the
logged-in user, protection from a malicious local administrator, authenticated
telemetry, replay prevention, or certification of geographic/weapon-model
truth.

## Assets and adverse outcomes

| Asset | Required property | Representative failure |
| --- | --- | --- |
| Displayed/recorded telemetry | Integrity and source context | Forged or replayed UDP changes the operational picture |
| User files and model directory | Path confinement | glTF URI reads a file outside the selected package |
| Map, terrain, and coastline state | Integrity and availability | Malformed geometry exhausts resources or misclassifies land/water |
| Process and GPU capacity | Availability | Compressed image, repeated buffer, or queue exhausts memory |
| Saved `.lar` destination | Atomicity | Failed save replaces a valid existing session |
| Worker/QObject/OpenGL state | Lifetime safety | Shutdown races a timer, socket, queued call, or context cleanup |
| Dependency inventory | Traceability | A vulnerable Qt module/plugin is shipped but absent from the SBOM |

## Actors and trust assumptions

- A remote network peer may send arbitrary UDP datagrams at high rate. An IP
  whitelist limits addresses but does not authenticate a peer and does not stop
  spoofing or replay on a capable network.
- A local file provider may supply malformed or adversarial mapping, `.lar`,
  `.gltf`, `.glb`, PNG, JPEG, `.larmap`, GeoJSON, or DTED data.
- A normal operator is trusted to choose files and destinations but may make a
  mistake. The operator is not expected to inspect binary formats.
- Build and release maintainers are trusted to pin Qt, review the generated
  SBOM, and run the quality matrix. Downloaded dependencies and plugins remain
  supply-chain inputs that require scanning.

## Trust boundaries and data flow

```text
untrusted UDP -> QUdpSocket/network worker -> mapping + domain validation
                                            -> bounded recording batches
                                            -> coalesced UI state

untrusted files -> bounded parser/resolver -> immutable candidate
                                        success -> application/render state
                                        failure -> diagnostic, old state retained

UI commands -> typed runtime boundary -> network/session/persistence workers
                                      -> request-correlated completions

CPU geometry/model -> render thread + current context -> GPU buffers/textures

map land index + addressed DTED -> Plane terrain worker -> immutable patch
                                                       -> context-thread upload
```

The UI thread, three application runtime workers, Earth preparation worker,
Plane terrain worker, and OpenGL contexts are separate lifetime/affinity
boundaries. Queued Qt delivery is also a memory queue and must be treated as a
resource, not only as a synchronization mechanism.

## Attack surfaces and controls

### UDP telemetry

Datagrams are size-bounded, decoded through an immutable mapping, and published
only after domain validation. State and recording batches are bounded/coalesced.
The current protocol has no MAC, encryption, sender identity, sequence number,
or replay window. Allow-all is therefore appropriate only on an isolated trusted
network; an exact-IP whitelist is defense in depth, not authentication.

For integrity-sensitive deployments, bind only on the intended interface,
select a whitelist, and carry UDP through an authenticated tunnel or network
segment with equivalent peer authentication and replay controls. A future
authenticated envelope must be versioned and tested independently of the
payload mapping.

Malformed-datagram diagnostics remain a distinct availability concern: ingress
errors must be rate-limited or coalesced before queued UI delivery. Until that
control is implemented, do not expose the listener to a hostile high-rate
network, and monitor process memory and diagnostic rate.

### Mapping and LAR1 sessions

Mapping JSON has byte/entry bounds and is committed only after complete parse
and validation. A `LAR1` record payload is limited to 16 MiB; embedded mapping
data is limited to 16 MiB; timestamps are non-decreasing and limited to 365
days. There is deliberately no arbitrary maximum record count.

The session reader performs a complete validation pass, stores one timestamp
and header-offset checkpoint per 4,096 records, and caches at most one
4,096-record location page. Thus file
size and validation time can grow with a session, while resident index memory
does not grow one entry per record. Counts and indices use signed 64-bit values;
the byte layout, storage, duration, and that representation are the remaining
natural limits. Saves use immutable snapshots and `QSaveFile` commit semantics.

Sessions do not carry a signature or origin proof. Structurally valid tampering
cannot be distinguished from legitimate capture; distribute trusted evidence
with an external signature or authenticated archive.

### glTF/GLB models and images

The model loader accepts a constrained glTF 2.0 subset. One canonical resolver
serves buffers and images. External URIs must be relative paths without NUL,
query, fragment, traversal, or absolute syntax. Every path component is checked
against symlinks; the final target must be a regular file whose canonical path
is contained below the selected model directory.

The maintained budgets are:

| Resource | Limit |
| --- | ---: |
| Model JSON/GLB file | 32 MiB |
| One data-URI or external resource | 32 MiB |
| All declared logical buffer bytes | 64 MiB |
| All encoded image bytes | 32 MiB |
| All decoded image/GPU logical bytes | 64 MiB |
| Buffer entries | 32 |
| Texture entries | 32 |
| Texture width or height | 8,192 pixels |

Aggregate buffer size is preflighted from declared lengths before file I/O.
Encoded and decoded texture costs are accumulated with checked arithmetic;
repeated texture references consume repeated logical GPU budget. Only PNG and
JPEG decoding is selected, automatic content-format decisions are disabled,
and candidates commit transactionally. Allocation failure becomes a load
diagnostic and the active model is retained.

Canonical containment prevents static path escape. Like most local filesystem
checks, it is not a guarantee against a separately privileged process replacing
directory entries during a read. Do not load a package from a directory that an
untrusted process can concurrently mutate when this race matters.

Cubemap discovery accepts at most 256 sets. Each of the six faces must be a
non-empty file no larger than 64 MiB, decode to the probed dimensions, be square,
and be no larger than 4,096 pixels on either axis. A set is published only when
all named faces validate.

### Map, DTED, and terrain assets

The build-time GeoJSON reader limits source size to 64 MiB, features to 2,000,
polygons to 20,000, rings to 50,000, one ring to 1,000,000 coordinates, and the
whole source to 1,500,000 coordinates. The compiler rejects non-finite or
out-of-range coordinates, invalid rings/triangulation, arithmetic overflow, and
package counts beyond the runtime contract.

The runtime `.larmap` reader accepts at most 64 MiB and verifies magic/version,
the fixed 80-byte header, section offsets and sizes, CRC, coordinates, indices,
and these principal ceilings: 2,000,000 vertices; 6,000,000 indices per fill
projection; 4,000,000 border indices; and 4,000,000 land-triangle references.
Version 2 contains a fixed 360-by-180 one-degree land index. Plane derives its
local land mask from this immutable index; DTED elevation sign is never trusted
as coastline classification.

DTED roots are user-selected directory boundaries. Coordinates resolve only to
the fixed `{e|w}DDD/{n|s}DD.dt0|dt1|dt2` layout; a canonical regular file must
remain beneath the canonical selected root. The containment decision uses a
normalized canonical relative path rather than a separator-sensitive string
prefix, retaining the boundary on both Unix and Windows. The parser streams
UHL, DSI, ACC, and elevation profiles and validates declared origins, level
dimensions, profile order, checksums, signed-magnitude samples, and the
`-32767` no-data sentinel before publishing a cell. The mosaic cache is capped
at 24 entries and 128 MiB, including bounded negative lookups.

Terrain requests cap spatial extent and sample resolution before allocation.
The land raster is power-of-two R8 at 256–2,048 pixels, or a constant when the
patch is uniformly land/water. Workers coalesce movement requests, attach a
revision, and publish immutable CPU patches. The widget rejects stale revisions
and alone uploads GPU resources. A DT1/DT2 selection validates its addressed
cell transactionally; failure retains the previous source and visible patch.
These controls bound malformed input and stale work, but they cannot establish
that structurally valid elevation or coastline data is geographically truthful.

### Rendering and extreme zoom

Hostile geometric inputs are checked before allocation. Earth zones use an
adaptive geodesic sampler with explicit vertex/index limits and a 0.65-pixel
curve-error target. The parametric GPU renderer is eligible only when projected
tessellation and float-coordinate rounding remain within its portion of that
budget. Polar or extreme-zoom cases fall back to camera-relative CPU vertices,
which avoids subtracting large absolute world coordinates in float precision.

Native GPU tests are still required because CPU eligibility proofs cannot
detect driver/compiler defects. GPU objects are owned by their context thread
and cleaned while the context is current.

### Threads and shutdown

Runtime commands and completions use typed queued messages, request IDs, and
source epochs. `ThreadedApplicationRuntime::shutdown()` rejects new work,
synchronously invokes each worker's shutdown slot, moves each complete QObject
tree to the runtime affinity thread while its event loop still runs, quits and
joins all worker threads, and explicitly destroys the workers. It does not rely
on `finished -> deleteLater`; deferred deletion can strand its event after the
worker loop exits.

Rehoming success and final affinity are checked in release builds. If Qt cannot
establish safe ownership, shutdown fails closed with a fatal invariant instead
of deleting a live worker across thread affinity.

Value-member timers are forbidden for movable worker objects because they do
not automatically follow the parent's affinity; runtime timers are QObject
children. Repeated teardown is covered by worker-destruction tests and the TSan
contract selection.

### Dependencies and plugins

CMake rejects Qt older than 6.10.3, while the current Windows workflow installs
6.11.2.
The SPDX 2.3 artifact identifies Qt Core, Concurrent, Gui, Network, Widgets,
OpenGL, OpenGLWidgets, the platform plugin, PNG/JPEG handlers, and the C++
runtime with exact configured versions and direct relationships. Installation
smoke validates this component set. The checked-in workflow does not currently
scan the generated SPDX document for known vulnerabilities; release maintainers
must run and review an approved current scanner separately.

The configure-time SBOM describes the intended build/deployment surface, not a
cryptographic inventory of every installed file or transitive system library.
Release packaging should add file hashes and verify the deployed bundle with a
platform-native dependency inventory where stronger attestation is required.

## STRIDE summary

| Category | Primary exposure | Control / residual risk |
| --- | --- | --- |
| Spoofing | UDP sender identity | Whitelist/tunnel guidance; protocol remains unauthenticated |
| Tampering | UDP and `.lar` content | Structural/domain validation; no cryptographic authenticity |
| Repudiation | Capture provenance | Exact raw recording; no signed audit identity |
| Information disclosure | glTF external URI | Canonical contained regular-file resolver; local filesystem race remains |
| Denial of service | Datagram flood, map/DTED/model allocations, GPU uploads | Batches, parser/cache/GPU budgets, sparse session index; diagnostic flood remains |
| Elevation of privilege | Parser/decoder defect or hostile dependency | Hardening, fuzzing, sanitizers, patched Qt baseline, component SBOM |

## Verification and review triggers

The following controls are expected release evidence: strict conversion builds;
ASan/UBSan; TSan runtime contracts; parser adversarial tests; architecture and
documentation semantic gates; native Earth/Plane GPU tests; install-layout and
SBOM validation; and dependency scanning. The current checked-in workflow
automates only the strict MSVC build, deterministic non-GPU tests, and install
smoke; see [Quality gates](QUALITY_GATES.md) for the exact gap.

Review this threat model whenever a new protocol field or transport is added, a
parser accepts a new URI/codec/format, a resource limit changes, a worker or
queued signal is introduced, rendering starts retaining a new GPU resource,
Qt/plugins are upgraded, or installation contents change. A mitigation is not
complete until its failure behavior, resource accounting, and regression test
are documented.
