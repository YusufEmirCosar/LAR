# Quality gates

This page distinguishes checks that the repository can run from checks that the
currently committed GitHub workflow actually runs. A configured target is not
release evidence until a suitable runner executes and retains its result.

## Current automated evidence

`.github/workflows/windows.yml` is the only checked-in workflow. On pushes,
pull requests, and manual dispatch it uses `windows-latest`, MSVC 2022, and Qt
6.11.2 to:

1. configure the `ci-msvc` preset;
2. build with warnings-as-errors and strict conversions;
3. run the deterministic CTest suite, excluding both native GPU tests; and
4. run `check-install-layout` against a temporary install prefix.

The workflow uses read-only repository permissions and pins third-party actions
to full commit hashes. It does **not** currently automate Linux, macOS, GCC,
Clang, sanitizers, coverage, fuzzing, mutation, documentation generation,
dependency vulnerability scanning, native GPU execution, or the extended soak.
Those capabilities are described below because they are implemented locally,
not because CI presently supplies their evidence.

Qt 6.10.3 is the source/API and installed-SBOM minimum. The Windows workflow
uses Qt 6.11.2. A cross-platform release should obtain strict GCC, Clang,
AppleClang, and MSVC evidence plus representative native GPU/install results.

## Configure and test presets

| Preset | Important options | Intended use |
| --- | --- | --- |
| `dev` | Debug, hardening | Local development |
| `release` | Release, hardening | Optimized local build |
| `windows-release` | Visual Studio 2022, tests/quality targets off | End-user Windows build |
| `ci` | Release, warnings-as-errors, strict conversions | Strict default compiler |
| `ci-msvc` | Visual Studio 2022, tests enabled | Current Windows CI |
| `ci-gcc` | GCC, warnings-as-errors, conversions/sign/shadow | Strict GCC evidence |
| `ci-clang` | Clang, warnings-as-errors, conversions/sign/shadow | Strict Clang/static evidence |
| `asan-ubsan` | Clang, address + undefined behavior sanitizers | Memory and UB defects |
| `tsan` | Clang, thread sanitizer | Contract/concurrency race checks |
| `coverage` | GCC, debug, coverage instrumentation | Threshold report |
| `fuzz` | Clang, libFuzzer when detected, otherwise deterministic runner | Parser/domain fuzz targets |
| `mutation` | Clang plus matching Mull frontend | Domain/application test effectiveness |

Typical strict local run:

```bash
cmake --preset ci-clang
cmake --build --preset ci-clang --parallel
ctest --preset ci-clang --output-on-failure
cmake --build build-ci-clang --target check-repository check-tidy generate-sbom
```

## Repository and static gates

These targets are created when `LAR_ENABLE_QUALITY_TARGETS=ON`, which is the
default for normal presets.

| Target | Enforced property |
| --- | --- |
| `check-format` | `clang-format --dry-run --Werror` over C++ source, tests, and tools |
| `check-tidy` | Project `.clang-tidy` over production/tool `.cpp` files, with all findings promoted to errors |
| `check-architecture` | Layer includes, concrete Qt leakage, CMake link direction, map-tool separation, file-size ceilings |
| `check-doc-links` | Every local Markdown link resolves inside the repository |
| `check-doc-coverage` | Every production header has exactly one Doxygen `@file` and `@brief` block, plus required semantic contracts |
| `check-doc-quality` | Generated filler is absent and security, lifetime, resource, specification, and threat-model contracts remain explicit |
| `check-docs` | Doxygen parses production source and authored pages without warnings and builds the HTML reference |
| `check-repository` | Aggregate format, architecture, link, semantic/API documentation, and Doxygen gate |
| `generate-sbom` | Component-level SPDX 2.3 manifest for the application and Qt runtime components |
| `check-install-layout` | Temporary install plus executable, map, asset, mapping, sender, and SBOM validation |

`check-repository` intentionally does not include `check-tidy`, tests,
sanitizers, coverage, mutation, performance, or the install smoke; invoke those
separately when they apply.

If a configured tool is missing, its target fails with an explicit dependency
message rather than silently passing.

## Deterministic CTest suite

The release configuration currently registers 28 tests.

| Area | Tests |
| --- | --- |
| Domain/sender/DLZ | `lar-testsender-scenario-tests`, `lardomain-tests`, `lardlz-tests`, `lardlz-view-tests` |
| Architecture/runtime | `lararchitecture-tests`, `larsource-lifecycle-tests`, `larruntime-contract-tests`, `larrequest-gate-tests` |
| Recording/session/playback | `larrecording-pipeline-tests`, `larplayback-service-tests`, `larsession-adapter-tests`, `larsession-time-tests` |
| Network/policy/operational | `larnetwork-adapter-tests`, `lar-ip-access-policy-tests`, `laroperational-soak-tests`, `larpolar-udp-tests` |
| Map/compiler/package/render | `larmap-tests`, `larmap-compiler-tests`, `larmap-renderer-validation-tests`, `larmap-loading-tests`, `lar-earth-view-gpu-tests` |
| Plane/viewport/UI | `larplane-view-tests`, `lar-plane-view-gpu-tests`, `larzone-tests`, `larcamera-tests`, `larviewport-page-tests`, `larviewport-worker-tests`, `larviewer-tests` |

Normal tests have a 30-second timeout. The operational soak has a 1,900-second
timeout. QWidget tests set `QT_QPA_PLATFORM=offscreen`. Native Earth and Plane
GPU executables return CTest skip code 77 unless their opt-in variables are set
and a usable OpenGL context is available.

Labels support focused execution:

| Label | Meaning |
| --- | --- |
| `unit` | Pure or narrowly composed behavior |
| `contract` | Port/runtime/lifecycle substitution contracts |
| `integration` | Multiple production components or UI composition |
| `network` | Local UDP/socket behavior |
| `concurrency` | Asynchronous preparation/lifecycle behavior |
| `gpu` | Shader/renderer or native context evidence |
| `slow` | Extended-duration behavior |
| `sanitizer` | Added to all tests in sanitizer configurations |

## Coverage

Configure with `cmake --preset coverage`, build, then run:

```bash
cmake --build build-coverage --parallel
cmake --build build-coverage --target coverage-report
```

The target excludes `network|gpu|slow`, writes
`build-coverage/coverage-summary.json`, and enforces aggregate thresholds:

| Scope | Lines | Branches |
| --- | ---: | ---: |
| All `src/` | 90% | 80% |
| `src/domain/` + `src/application/` | 95% | 90% |

Aggregate success is not a waiver for an untested parser, error path,
concurrency transition, or resource bound.

## Sanitizers

AddressSanitizer and UndefinedBehaviorSanitizer are combined by
`asan-ubsan`; its test preset excludes `network|gpu|slow`. ThreadSanitizer has a
separate `tsan` preset, selects `contract|concurrency`, uses
`tests/tsan_suppressions.txt`, and stops on the first report. All sanitizer
builds retain frame pointers.

```bash
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --parallel
ctest --preset asan-ubsan --output-on-failure

cmake --preset tsan
cmake --build --preset tsan --parallel
ctest --preset tsan --output-on-failure
```

The repository contains no currently automated repeated TSan job. For release
concurrency evidence, repeat runtime, source-lifecycle, recording-pipeline,
playback, map-loading, and viewport-worker tests under a compatible TSan host.

## Fuzzing

The `fuzz` preset builds seven targets:

| Target | Untrusted surface |
| --- | --- |
| `lar-map-reader-fuzz` | `.larmap` bytes |
| `lar-map-source-fuzz` | GeoJSON source bytes |
| `lar-mapping-repository-fuzz` | Mapping JSON |
| `lar-packet-decoder-fuzz` | Mapped UDP bytes |
| `lar-session-reader-fuzz` | LAR1 bytes |
| `lar-ip-policy-fuzz` | Whitelist text |
| `lar-dlz-solver-fuzz` | DLZ numeric domain and ordering |

When Clang supports libFuzzer, targets link ASan/UBSan and accept normal
libFuzzer arguments/corpora. Otherwise they use
`tests/fuzz/fuzz_standalone_main.cpp`, whose positional argument is a bounded
iteration count. No fuzz-duration or corpus-retention policy is currently
automated by the checked-in workflow.

## Mutation analysis

The `mutation` preset instruments domain/application code through a Mull IR
frontend matching the selected Clang. `check-mutation` runs `lardomain-tests`
in strict mode and requires an 85% mutation score. Mull must be installed
separately and can be selected with `LAR_MULL_IR_FRONTEND`.

## Performance evidence

`run-benchmarks` writes JSON metrics for mapping, recording, playback, map, and
viewport operations. `check-performance` additionally requires
`LAR_PERFORMANCE_BASELINE` and rejects either median or p95 latency more than
10% above that baseline. A comparison is meaningful only on a pinned runner
with controlled power and thermal state.

## Install and dependency evidence

`check-install-layout` performs a clean temporary install and requires:

- the viewer executable or macOS app bundle and `lar-test-sender`;
- the `.larmap` package and manifest;
- the F-16 GLB and at least one complete six-face cubemap set;
- all three supplied mapping files; and
- the SPDX 2.3 SBOM.

It parses the manifest and SBOM, requires Qt 6.10.3 or newer, verifies direct
Core/Concurrent/Gui/Network/Widgets/OpenGL/OpenGLWidgets relationships plus the
platform and PNG/JPEG plugins, checks package URLs, and executes the installed
sender's scenario listing. DTED is intentionally excluded from normal staging
and install because the Level-0 tree is large and can be supplied externally.

The repository generates dependency inventory but the current workflow does
not run a vulnerability scanner. A release process should scan the generated
SPDX file with an approved current scanner and review the result.

## GPU evidence

Pure shader, renderer, projection, and asset-validation tests run without a
native visible context. Native tests require:

```bash
LAR_RUN_MAP_GPU_TESTS=1 LAR_RUN_PLANE_GPU_TESTS=1 \
  ctest --test-dir build-release --output-on-failure -L gpu
```

On a headless Linux runner, a software OpenGL stack may additionally require
Xvfb/Mesa plus `LIBGL_ALWAYS_SOFTWARE=1` and `QT_OPENGL=software`. That setup is
not present in the current workflow. Release evidence should include a native
representative GPU/driver as well as any software-renderer run.

`larzone-tests` quantitatively checks extreme zoom. It rejects fixed topology
when it cannot meet the 0.65-pixel budget, reconstructs camera-relative CPU
vertices, and compares both vertex and chord error with a binary64 geodesic
reference. Native GPU tests complement that proof by compiling real shaders and
inspecting framebuffer output.

## Operational soak

The normal `laroperational-soak-tests` run is short. Setting
`LAR_SOAK_SECONDS=1800` exercises a 30-minute loopback scenario with:

- 100 Hz UDP input;
- decode/validation and bounded recording batches;
- periodic snapshots;
- pause/resume and reset barriers;
- final atomic save;
- exact reader/playback verification; and
- a resident-memory growth ceiling.

The extended duration is opt-in and not scheduled by the current workflow.

## Release evidence checklist

A release candidate should assemble evidence appropriate to the changed risk:

- repository gates and warning-free documentation;
- strict builds and deterministic tests on all supported compiler/platforms;
- ASan/UBSan and TSan runs for memory/concurrency changes;
- coverage and focused parser/error-path tests;
- fuzz or mutation evidence for new untrusted formats/domain rules;
- installed-layout and reviewed SBOM/vulnerability results;
- native GPU checks for renderer/shader/asset changes; and
- benchmark or extended-soak evidence when latency, throughput, or long-lived
  resource behavior changes.

These checks cover distinct failure modes; one green job does not substitute
for the others.
