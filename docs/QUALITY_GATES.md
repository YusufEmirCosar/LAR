# Quality gates

## Supported verification matrix

| Environment | Compiler/tooling | Purpose |
| --- | --- | --- |
| Ubuntu 24.04 | GCC + Qt 6.11.2 | Strict warnings, deterministic tests, coverage, software GPU |
| Ubuntu 24.04 | Clang + Qt 6.11.2 | Strict warnings, static analysis, sanitizers, fuzzing |
| macOS 15 | AppleClang + Qt 6.11.2 | Native platform strict build/test/install evidence |

Qt 6.10.3 is the source/API minimum. Other platforms may work but are not
release evidence until represented by strict CI and relevant GPU/install jobs.

## Presets

| Preset | Important options | Intended use |
| --- | --- | --- |
| `dev` | Debug, hardening | Local development |
| `release` | Release, hardening | Optimized local build |
| `ci` | Release, warnings-as-errors, strict conversions | Default strict toolchain |
| `ci-gcc` | `g++`, warnings-as-errors, conversions/sign/shadow | GCC release evidence |
| `ci-clang` | `clang++`, warnings-as-errors, conversions/sign/shadow | Clang release/static evidence |
| `asan-ubsan` | Clang, address + undefined behavior, frame pointers | Memory/UB defects |
| `tsan` | Clang, thread sanitizer, selected contract/concurrency tests | Data races and thread misuse |
| `coverage` | GCC, `--coverage`, debug/no optimization | gcovr threshold report |
| `fuzz` | Clang, ASan/UBSan, libFuzzer when available | Parser/domain fuzz targets |
| `mutation` | Clang + matching Mull IR frontend | Domain/application test effectiveness |

## Repository/static gates

| Target | Gate |
| --- | --- |
| `check-format` | `clang-format --dry-run --Werror` over source, tests, and tools |
| `check-tidy` | Project `.clang-tidy` with all findings promoted to errors |
| `check-architecture` | Layer includes, concrete Qt leakage, CMake link direction, map-tool separation, file-size limits |
| `check-doc-links` | Every local Markdown link remains inside the repository and exists |
| `check-doc-coverage` | Every production header has one Doxygen `@file` + `@brief` block |
| `check-doc-quality` | Generated filler is absent and security/lifetime/resource contracts remain explicit |
| `check-docs` | Doxygen parses all production code/docs without warnings and generates Graphviz diagrams |
| `check-repository` | Aggregate format, architecture, link, semantic/API documentation, and Doxygen build |
| `generate-sbom` | Reproducible component-level SPDX 2.3 manifest for Qt modules/plugins and runtime |

Typical local invocation:

```bash
cmake --preset ci-clang
cmake --build --preset ci-clang --parallel
cmake --build build-ci-clang --target check-repository check-tidy generate-sbom
ctest --preset ci-clang --output-on-failure
```

## Deterministic CTest suite

The configured release tree currently registers 28 tests.

| Area | Tests |
| --- | --- |
| Domain/sender/DLZ | `lar-testsender-scenario-tests`, `lardomain-tests`, `lardlz-tests`, `lardlz-view-tests` |
| Architecture/runtime | `lararchitecture-tests`, `larsource-lifecycle-tests`, `larruntime-contract-tests`, `larrequest-gate-tests` |
| Recording/session/playback | `larrecording-pipeline-tests`, `larplayback-service-tests`, `larsession-adapter-tests`, `larsession-time-tests` |
| Network/policy/operational | `larnetwork-adapter-tests`, `lar-ip-access-policy-tests`, `laroperational-soak-tests`, `larpolar-udp-tests` |
| Map/compiler/package/render | `larmap-tests`, `larmap-compiler-tests`, `larmap-renderer-validation-tests`, `larmap-loading-tests`, `lar-earth-view-gpu-tests` |
| Plane/viewport/UI | `larplane-view-tests`, `lar-plane-view-gpu-tests`, `larzone-tests`, `larcamera-tests`, `larviewport-page-tests`, `larviewport-worker-tests`, `larviewer-tests` |

All normal tests have a 30-second timeout. The soak test has a 1,900-second
timeout. QWidget tests set `QT_QPA_PLATFORM=offscreen`. The native Earth and
Plane GPU tests use skip return code 77 unless explicitly enabled.

CTest labels support focused runs:

- `unit`
- `contract`
- `integration`
- `network`
- `concurrency`
- `gpu`
- `slow`
- `sanitizer` when a sanitizer preset is enabled

## Coverage thresholds

`coverage-report` excludes `network|gpu|slow`, generates a gcovr JSON summary,
and enforces:

| Scope | Lines | Branches |
| --- | ---: | ---: |
| All `src/` | 90% | 80% |
| `src/domain/` + `src/application/` | 95% | 90% |

The gate is aggregate by scope, not a waiver for untested high-risk files.
Parser, error, lifecycle, and concurrency changes still require focused tests.

## Sanitizers

- ASan and UBSan are combined in `asan-ubsan`; tests labelled network, GPU, or
  slow are excluded by its test preset.
- TSan cannot be combined with ASan/UBSan. Its preset runs contract/concurrency
  labels with `tests/tsan_suppressions.txt` and `halt_on_error=1`.
- All sanitizer builds retain frame pointers.

Nightly repeats runtime, source lifecycle, recording pipeline, playback, map
loading, and viewport worker tests up to 100 times under TSan.

## Fuzzing

Seven fuzz targets are built:

| Target | Surface |
| --- | --- |
| `lar-map-reader-fuzz` | `.larmap` package bytes |
| `lar-map-source-fuzz` | GeoJSON source bytes |
| `lar-mapping-repository-fuzz` | Mapping JSON |
| `lar-packet-decoder-fuzz` | Mapped UDP bytes |
| `lar-session-reader-fuzz` | LAR1 bytes |
| `lar-ip-policy-fuzz` | Whitelist text |
| `lar-dlz-solver-fuzz` | DLZ numeric domain and ordering |

With libFuzzer, CI runs each persisted corpus for 2,000 iterations with a
30-second maximum. Nightly allows 30 minutes per target and uploads evolved
corpora/crashes. Clang toolchains without libFuzzer build the deterministic
ASan/UBSan mutation runner in `fuzz_standalone_main.cpp`.

## Mutation analysis

The `mutation` preset instruments domain/application code through a Mull IR
frontend matching the selected Clang. `check-mutation` runs the domain test
binary with strict mode and requires an 85% mutation score. The runner must be
installed separately and may be selected with `LAR_MULL_IR_FRONTEND`.

## Performance evidence

`lar-performance-benchmarks` emits JSON metrics for core mapping, recording,
playback, map, and viewport operations. `check-performance` requires a baseline
path in `LAR_PERFORMANCE_BASELINE` and rejects either median or p95 latency more
than 10% above that baseline. Performance evidence is meaningful only on a
pinned runner with controlled power/thermal state.

## Install and dependency evidence

`check-install-layout` installs into a temporary prefix and verifies non-empty:

- viewer executable or macOS app bundle;
- `lar-test-sender`;
- `.larmap` package and manifest;
- F-16 GLB and packaged cubemap catalog;
- all three supplied mappings;
- SPDX SBOM.

It parses manifest/SBOM JSON, requires Qt 6.10.3 or newer, verifies the direct
Core/Concurrent/Gui/Network/Widgets/OpenGL/OpenGLWidgets, platform-plugin,
PNG/JPEG-handler relationships and package URLs, and runs the installed sender's
scenario listing. CI also scans the generated SPDX file with OSV Scanner.

## GPU evidence

Pure shader/renderer and asset-reader validation runs headlessly in the normal
suite. The software-GPU CI job sets `LIBGL_ALWAYS_SOFTWARE=1`,
`QT_OPENGL=software`, `LAR_RUN_MAP_GPU_TESTS=1`, and
`LAR_RUN_PLANE_GPU_TESTS=1`, then runs the native GPU and viewer suites under
Xvfb/Mesa. A release on another GPU stack should additionally run both tests
with a native visible context.

`larzone-tests` quantitatively exercises extreme zoom. It proves the fixed GPU
topology is rejected when it cannot meet the 0.65-pixel budget, reconstructs the
camera-relative CPU vertices, and checks both vertex and chord error against a
double-precision geodesic reference. Native GPU tests complement that policy
proof by exercising actual shader compilation and framebuffer output.

## Operational soak

The nightly 30-minute loopback test sets `LAR_SOAK_SECONDS=1800` and exercises:

- 100 Hz real UDP input;
- decode/validation and bounded recording batches;
- periodic snapshots;
- pause/resume and reset barriers;
- final atomic save;
- exact reader/playback verification;
- resident-memory growth ceiling.

The short default test remains in normal builds; the environment variable makes
the release/nightly duration explicit.

## CI jobs

Pull requests and pushes run dependency/SBOM scan, repository/static gates,
strict GCC and Clang builds, deterministic tests, install smoke, ASan/UBSan,
TSan contracts, fuzz smoke, coverage, software GPU, and macOS. Nightly adds the
100-repeat TSan run, extended fuzz time, and 30-minute operational soak.

No single green job substitutes for the matrix: strict compilation, behavioral
tests, concurrency instrumentation, parser adversarial testing, documentation,
packaging, and platform/GPU execution cover different failure modes.
