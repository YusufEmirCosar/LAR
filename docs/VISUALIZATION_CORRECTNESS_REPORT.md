# Visualization and simulation correctness review

**Review date:** 2026-08-25  
**Scope:** Grid, Mercator, Sphere, Plane, terrain, LAR IR/IZ geometry, markers,
test-sender scenarios, and the DLZ teaching model  
**Review type:** Read-only source, numerical, test, and framebuffer audit

## Executive verdict

The visualization pipeline is internally consistent, bounded, and substantially
better tested than a typical prototype, but it is **not yet metrically correct
over its entire accepted input and zoom domain**.

The direct answers to the requested questions are:

- **Plane altitude:** with flat ground, an altitude value of `3000 m` places the
  aircraft model's bounding-box centre exactly `3000 m` above the zero-height
  surface. With DTED terrain, that same value is treated as altitude above the
  terrain dataset's vertical datum, not height above the local ground. Over a
  `1178 m` terrain post, the displayed height above ground is therefore
  `3000 - 1178 = 1822 m`, not `3000 m`.
- **Plane size:** the packaged model is deliberately scaled to `15.0 m` long.
  Independent parsing of the flattened GLB produced rendered bounding-box
  dimensions of approximately `15.000 x 9.578 x 4.638 m` (length, span,
  height). The U.S. Air Force reference is `14.8 x 9.8 x 4.8 m`, so the current
  model is close, but not dimensionally exact.
- **Mercator:** the forward/inverse formulas, longitude wrapping, camera
  rotation, and latitude cutoff implement the usual Pseudo-Mercator display
  correctly. Geodesic LAR boundaries are generated before projection, so their
  characteristic Mercator distortion is expected. EPSG:3857 itself is a web
  visualization CRS, however, not a distance-preserving or navigation CRS.
- **Sphere/global view:** the orthographic globe projection, front/back test,
  tracked-centre precision split, pole behavior, and dateline handling are
  correct for the application's mean-radius spherical Earth model.
- **Circles:** IR and IZ centres are kept independent and are correct on the
  Earth views relative to that spherical model. Grid and Plane intentionally
  use a flat metric presentation: their circles are Euclidean circles in the
  selected local plane, not Earth geodesic circles. Comparing those outlines
  with WGS 84 is therefore not a defect under the flat-view contract. The
  remaining issue is contract disclosure and finite renderer/resource limits
  for very large flat circles; the shared `20,000 km` value is an input cap,
  not a geodesic correctness threshold.
- **Simulation scenarios:** the named aircraft motion scenarios are
  deterministic display fixtures. Their primary kinematic equations are
  internally consistent, but their geographic position conversion is a local
  approximation and they are not a navigation or flight-dynamics simulation.
  The DLZ is explicitly a fictional teaching model and is internally consistent
  with its documented equations; it cannot be validated as real weapon
  performance.

The most important corrective work is to document the per-view geometry
contract: Grid/Plane are intentionally Euclidean flat tactical views, while
Mercator/Sphere are geographic views with an explicitly chosen Earth model.
The remaining accuracy budgets must then hold for every accepted radius and
zoom level within the corresponding model.

## Scope and method

The review traced each displayed value through this chain:

| Stage | Examined behavior |
| --- | --- |
| Protocol/domain | Units, availability bits, range validation, angle conventions |
| Scenario generation | Position, velocity, heading, pitch/roll, target and LAR updates |
| Geographic geometry | Local tangent plane, spherical destination points, IR/IZ centres |
| Map projection | Pseudo-Mercator, orthographic sphere, dateline and polar handling |
| Plane geometry | GLB transforms, normalization, aircraft metric scale, attitude |
| Terrain | DTED addressing, interpolation, patch coordinates, vertical placement |
| GPU/painter rendering | Precision, tessellation, camera-relative coordinates, markers |
| Verification | Unit/integration tests, native OpenGL tests, snapshots, independent calculations |

Independent checks used binary64 calculations and PROJ `geod` 9.8.1 as a WGS
84 ellipsoidal reference. The packaged GLB was parsed independently, including
scene-node transforms and accessor strides. Numerical error values below are
maximums over sampled bearings, not visual estimates from a screenshot.

This review does not certify the application for navigation, flight safety,
terrain avoidance, targeting, or weapon employment. No external truth set was
provided for live telemetry, the complete DTED corpus, political boundaries,
or a real weapon model.

## Findings

Severity means:

- **High:** accepted data can be displayed with a materially wrong physical
  interpretation or location.
- **Medium:** correctness fails at an extreme, depends on unstated source
  metadata, or lacks an enforceable accuracy contract.
- **Low:** consistency, documentation, or verification weakness with a smaller
  direct geometric effect.

| ID | Severity | Finding | Consequence |
| --- | --- | --- | --- |
| V-01 | Low | The flat-world contract for Grid/Plane is not explicit enough while the shared validator accepts global-scale values | Users may interpret a Euclidean flat-view radius as an Earth distance; this is a model-disclosure and verification issue, not a geodesic placement error |
| V-02 | High | Altitude carries no machine-readable vertical datum or AGL/MSL semantic | `3000 m` can mean 3 km above datum while an operator reads it as 3 km above terrain |
| V-03 | Medium | Earth LAR geometry uses a mean-radius sphere, not WGS 84 ellipsoidal geodesics | Even correctly rendered spherical endpoints differ from WGS 84 by tens to hundreds of metres at a 26 km radius |
| V-04 | Medium | The fallback zone mesh's `0.65 px` curve target is defeated by the fill-cell budget for large zones | At maximum zoom, accepted 1,000 km and 3,000 km boundaries can be visibly faceted |
| V-05 | Medium | The base map uses absolute float32 coordinates, fixed compile-time sphere subdivision, and generalized Natural Earth data at extreme zoom | LAR overlays can remain stable while the coastline/border beneath them shifts or lacks appropriate detail |
| V-06 | Medium | Aircraft scaling assumes every loaded model points along Z and represents a 15 m aircraft | The packaged F-16 is approximate; arbitrary uploaded jets acquire a fictitious 15 m length and potentially the wrong axis/reference point |
| V-07 | Medium | Terrain datum/accuracy metadata is not parsed, and reused patches are repositioned with a different local approximation | Vertical accuracy cannot be certified; horizontal terrain registration degrades with movement and latitude |
| V-08 | Low | Target-marker fallback differs by view | With IR present and IZ absent, Plane shows an IR marker while Grid/Mercator/Sphere show no target marker |
| V-09 | Medium | GPU checks verify rendering presence rather than pixel metrology | Passing GPU tests does not prove altitude, model dimensions, centre placement, or boundary pixels are correct |
| V-10 | Low | Two sender descriptions overstate scenario behavior | The default drift speed depends on packet interval despite a fixed velocity value; the documented `0°↔180°` DLZ sweep actually rises once and then stays at `180°` |

## Plane visualization

### Aircraft scale

The GLB reader flattens scene-node transforms, computes a complete axis-aligned
bounding box, centres that box at the scene origin, and normalizes the largest
axis to two scene units. `PlaneAircraftScale` then defines the model's forward
Z extent as `15.0 m`:

```text
metresPerSceneUnit = 15.0 / normalizedForwardZExtent
```

For the packaged `f16_3.glb`, forward Z is also the largest axis, so the scale
is `7.5 m/scene-unit`. Independent results were:

| Dimension | Rendered package | USAF reference | Difference |
| --- | ---: | ---: | ---: |
| Length | 15.000 m | 14.800 m | +0.200 m (+1.35%) |
| Wingspan | 9.578 m | 9.800 m | -0.222 m (-2.27%) |
| Height | 4.638 m | 4.800 m | -0.162 m (-3.38%) |

The official comparison dimensions come from the [U.S. Air Force F-16 fact
sheet](https://www.af.mil/About-Us/Fact-Sheets/Display/Article/104505/f-16-fighting-falcon/).
The small mismatches are consistent with a display asset calibrated by one
bounding-box dimension, not a dimensionally authoritative CAD model.

The aircraft origin is the centre of its axis-aligned bounding box. It is not a
declared centre of gravity, inertial reference point, wheel contact point, or
sensor reference. Consequently, "aircraft altitude" means the altitude of
that synthetic centre. At level attitude, the lowest point of the current
bounding box is about `2.319 m` below it.

User-uploaded models have a more important correctness problem. Every accepted
model is centred, assumes Z is the forward axis, and is assigned a 15 m forward
extent. A 10 m trainer, 20 m fighter, model authored along X, or model with
external stores outside the fuselage bounds will establish an incorrect metre
scale for the whole surface and terrain scene.

### Altitude and terrain height

The flat-surface vertical transform is straightforward and has no vertical
exaggeration:

```text
aircraft scene Y = 0
flat ground scene Y = -altitudeMetres / metresPerSceneUnit
```

At `3000 m` with the packaged model:

```text
ground Y = -3000 / 7.5 = -400 scene units
displayed reference-point separation = 400 * 7.5 = 3000 m
```

The focused test suite independently checks the same chain at `3300 m` and
gets exactly `3300 m` after converting back to metres.

Terrain uses the same horizontal and vertical metre scale:

```text
terrain scene Y = elevationMetres / metresPerSceneUnit
                  - aircraftAltitudeMetres / metresPerSceneUnit
AGL at a post    = aircraftAltitudeMetres - elevationMetres
```

The checked-in `e035/n39.dt0` centre sample is `1178 m`. Therefore:

| Telemetry value | Terrain elevation | Aircraft reference height above terrain |
| ---: | ---: | ---: |
| 3000 m | 0 m | 3000 m |
| 3000 m | 1178 m | 1822 m |
| 4178 m | 1178 m | 3000 m |

Thus the requested statement "if the data says 3 km above, it should be 3 km
above" is true only after defining **above what**. The current protocol calls
the field altitude above the model datum, and the Plane documentation asks for
an MSL-compatible datum. It is not an AGL field.

The camera does not help an operator distinguish these meanings. Its default
distance is six scene units, approximately `45 m`, and it looks at the aircraft
centre with a 45° perspective field of view. The aircraft remains prominent
while a surface kilometres below is seen near the horizon. There is no
altitude datum label, AGL readout, vertical ruler, or ground-elevation readout.
The geometry can therefore be numerically right while the screenshot is not
human-verifiable.

### Terrain accuracy limits

The DTED reader correctly bounds Level-0 dimensions, validates UHL/DSI/ACC
record presence, checks every profile checksum, decodes signed-magnitude metre
elevations, preserves no-data, and bilinearly samples adjoining cells.

It does **not** parse or enforce DSI datum fields or ACC accuracy values. The
inspected tile contains `WGS84`/`MSL` text, but its ACC accuracy fields are
unavailable (`NA`). Structural validity is therefore established; geographic
or vertical accuracy is not. NGA describes DTED Level 0 as 30 arc-second,
nominally 1 km post spacing and distinguishes resolution from absolute
accuracy in its [official elevation guidance](https://earth-info.nga.mil/index.php?action=elevation&dir=elevation).

The rendered patch is sampled with spherical destination points about its
anchor, but an already-built patch is translated under a moving aircraft with
the local equirectangular formula. For the largest normal patch, rebuilding is
requested after 35% of its 60 km half-extent (`21 km`). Relative to the same
spherical reference, the maximum translation discrepancies are:

| Latitude | Movement | Maximum patch offset discrepancy |
| ---: | ---: | ---: |
| 0° | 21 km | 0.02 m |
| 41° | 21 km | 30.1 m |
| 80° | 21 km | 196.3 m |

The old patch can remain visible while its replacement is prepared and is
dropped only beyond 80% of the half-extent. At 48 km and 80° latitude, the
same analytical discrepancy is about `1.03 km`. A fixed ECEF/ENU frame for the
patch, or rebuilding/reprojecting before this error threshold, is needed.

Water-classified posts are intentionally rendered at mean sea level while
negative depth remains only a colour input. This is correct for a flat water
surface visualization, but it must not be interpreted as bathymetric geometry.

### Plane LAR zones

The axis and angle conventions are consistent:

- scene +X is east;
- scene -Z is north;
- bearing zero is north and positive bearing turns clockwise;
- IR uses `ir_pos` and `ir_r`;
- IZ uses `iz_pos`, its inner/outer radii, and its directed angular span.

The centre conversion is intentionally a local flat metric mapping:

```text
east  = R * wrappedDeltaLongitude * cos(aircraftLatitude)
north = R * deltaLatitude
```

The boundary is then a Euclidean ring or sector about that centre. Under the
flat tactical-surface contract, this is the correct geometry: a radius of `r`
means `r` metres in the display plane. It must not be presented as a WGS 84
surface-distance circle. The shared validator accepts `20,000,000 m`; that is
an input/resource cap, not a geographic validity threshold. At very large
radii, the separate fixed-tessellation and precision findings still apply.

The Plane GPU ring uses 128 angular segments. Chord sagitta alone, before the
geographic approximation, is:

| Radius | 128-segment sagitta |
| ---: | ---: |
| 26 km | 7.83 m |
| 100 km | 30.12 m |
| 1,000 km | 301.18 m |
| 3,000 km | 903.54 m |

This should either become projected-error-adaptive or be covered by an
explicit flat-view rendering budget.

## Grid visualization

Grid uses the same local equirectangular centre projection and Euclidean
geometry. Under the intended flat-world model, the metre scale bar is exact in
that local Cartesian plane, and its camera rotation, north indicator, heading
glyph, dateline wrapping, and fixed Earth-grid phase are internally coherent.
It is not intended to be an azimuthal-equidistant or ellipsoidal Earth map;
Mercator and Sphere provide the geographic global views. The earlier
local-vs-WGS-84 discrepancy numbers are useful only as a counterfactual test
of what would happen if Grid were interpreted as an Earth projection, and
should not be treated as a Grid correctness failure.

For a full circle, the Grid IZ path uses about 302 angular segments, compared
with Plane's 128. Additional vertices cannot change the flat-world semantics;
they only reduce the separate polygon-faceting error documented under V-04.

## Mercator visualization

### Correct behavior

The implementation uses:

```text
y = degrees(log(tan(pi/4 + radians(latitude)/2)))
latitude limit = ±85.05112878°
```

The inverse is algebraically correct, the tested round trip is below
`1e-10°` for representative latitudes, and periodic longitude helpers select
the nearest world copy. Camera projection and picking remain binary64 on the
CPU. Dateline-spanning fit and polar clipping have focused tests.

IR/IZ points are first sampled as spherical geodesic destinations and then
projected. A projected circle therefore should not look circular away from the
equator; that shape distortion is correct for Mercator. The map scale bar is
computed from two inverse-projected neighbouring screen points, so it reflects
local scale under the application's sphere.

EPSG describes 3857 as having a web-mapping/visualization scope, using a
spherical development of ellipsoidal coordinates, with up to 0.7% scale error
relative to World Mercator and northing differences up to 21 km on the ground.
See the [official EPSG:3857 record](https://epsg.org/crs_3857/WGS-84-Pseudo-Mercator.html).
This does not make the renderer's formula wrong; it means distances must come
from geodesics, not from measuring the projected picture.

### Earth model discrepancy

The LAR destination formula uses a constant mean Earth radius of
`6,371,008.8 m`. It is correct great-circle geometry on that sphere. Compared
with WGS 84 ellipsoidal direct geodesics, maximum endpoint separation is:

| Latitude | 26 km radius | 100 km | 1,000 km | 3,000 km |
| ---: | ---: | ---: | ---: | ---: |
| 0° | 145 m | 558 m | 5.50 km | 14.63 km |
| 41° | 67 m | 256 m | 2.58 km | 10.03 km |
| 80° | 114 m | 438 m | 4.39 km | 12.79 km |

This is the dominant reason an Earth-view circle can be perfectly consistent
with the implementation and still not be at a WGS 84 reference position.

### Extreme zoom and mesh budget

The GPU parametric path now performs a useful eligibility check: when its
fixed topology or float rounding cannot meet the budget, it falls back to a
camera-relative CPU mesh. The existing 10 km, zoom-100,000 test demonstrates
at most `0.05 px` vertex error and about `0.65 px` curve error for that case.

The fallback sampler first chooses angular density for a `0.65 px` target, but
then limits each zone to 2,600 fill cells. Large radial spans consume multiple
rows and force angular density down. At a 1600x900 viewport, equator, and
Mercator zoom 100,000, independent midpoint checks give:

| Radius | Angular x radial cells | Maximum midpoint-to-chord error |
| ---: | ---: | ---: |
| 10 km | 414 x 1 | 0.647 px |
| 26 km | 667 x 1 | 0.648 px |
| 100 km | 1307 x 1 | 0.650 px |
| 1,000 km | 1300 x 2 | 6.70 px |
| 3,000 km | 433 x 6 | 215.37 px |

The documented `0.65 px` value is therefore a sampling target, not a guarantee
for all valid inputs. The builder should preserve boundary density separately
from fill radial density, clip/tile only the visible portion, or reject inputs
whose required mesh exceeds the accuracy budget.

### Base-map precision and source resolution

Zone fallback vertices are camera-relative, and Sphere uses a high/low split
for its centre. The static map is different: it stores absolute longitude,
latitude, and Mercator Y as float32 and passes the Mercator camera centre as a
single float. At zoom 100,000 in a 1600x900 viewport, the representable
longitude spacing corresponds to approximately:

| Longitude magnitude | Pixels per float32 ULP |
| ---: | ---: |
| 30° | 0.48 px |
| 41° | 0.95 px |
| 90° | 1.91 px |
| 180° | 3.81 px |

Because both map vertex and centre are rounded, their relative error can
approach one ULP. An overlay can consequently remain camera-relative and stable
while the map under it moves by several pixels near the dateline.

The compiled map has 583,762 vertices, 1,594,269 Mercator fill indices, and
1,832,433 sphere fill indices, and passes CRC/SHA format validation. Its source
README identifies Natural Earth Admin-0 data but does not record dataset
version, download date, source archive hash, or a measured positional-error
budget. Natural Earth describes the product as generalized small-scale
cartographic data; its own guidance notes that close Google Earth inspection
can be 100 times the intended display resolution. See [Natural Earth data
creation guidance](https://www.naturalearthdata.com/about/data-creation/) and
its [accuracy discussion](https://www.naturalearthdata.com/forums/reply/re-poor-accuracy-of-the-boundaries/).

At maximum Mercator zoom, an equatorial 1600-pixel view is only about 712 m
wide (`~0.45 m/px`). That zoom is incompatible with Natural Earth's
small-scale generalized world linework, independently of floating-point
precision. The missing source-version/scale record prevents a more specific
accuracy claim for this exact file.

## Sphere/global visualization

The CPU orthographic formulas are standard:

```text
x = cos(lat) * sin(lon - lon0)
y = cos(lat0) * sin(lat)
    - sin(lat0) * cos(lat) * cos(lon - lon0)
z = sin(lat0) * sin(lat)
    + cos(lat0) * cos(lat) * cos(lon - lon0)
visible when z >= 0
```

The shader's rearranged high/low-centre form is algebraically equivalent. The
inverse screen-to-sphere calculation and bearing rotation are also consistent.
Tests retain a moving tracked centre to below `1e-12°` on the CPU/high-low
reconstruction at sphere zoom 5,000.

The global view handles poles without Mercator's cutoff. It still inherits the
mean-sphere-versus-WGS-84 error table above.

The map compiler refines sphere fill edges only to a fixed maximum of 2° and
a unit-sphere chord threshold, independent of viewport zoom. A 2° edge has the
following analytical midpoint sagitta in a 900-pixel-high view:

| Sphere zoom | Upper-bound screen sagitta for a 2° edge |
| ---: | ---: |
| 1 | 0.069 px |
| 10 | 0.685 px |
| 100 | 6.85 px |
| 5,000 | 342.69 px |

This is an upper bound for an edge at the maximum compiler threshold, not a
claim that every Natural Earth edge has that error. It proves that compile-time
angular subdivision alone cannot establish a pixel-error guarantee throughout
the offered zoom range. Runtime LOD or a lower zoom ceiling is required.

## Circle and marker placement summary

| View | Centre placement | Boundary model | Dateline/poles | Verdict |
| --- | --- | --- | --- | --- |
| Grid | Local equirectangular from aircraft | Euclidean | Longitude wraps; no true pole model | Correct under the flat metric model; not an Earth geodesic view |
| Plane | Local equirectangular from aircraft | 128-segment Euclidean GPU ring/sector | Longitude wraps; local plane at poles | Correct under the flat tactical-surface model; inspect tessellation at large radii |
| Mercator | Independent IR/IZ geographic centres | Mean-sphere geodesic, then Pseudo-Mercator | Dateline split/copies and polar clipping tested | Correct on the app sphere; not WGS 84 ellipsoidal |
| Sphere | Independent IR/IZ geographic centres | Mean-sphere geodesic on orthographic globe | Continuous across dateline and poles | Correct on the app sphere; not WGS 84 ellipsoidal |

The actual IR and IZ shapes do not accidentally share a centre. A focused test
places IR and IZ far apart and verifies that the IR mesh starts at `ir_pos`.

Marker behavior is less consistent. Grid and Earth draw the target triangle
only when IZ position fields exist. Plane selects IZ first but falls back to IR.
The marker policy should be shared and its meaning labelled, particularly when
IR and IZ centres differ.

## Simulation and DLZ review

### Test-sender aircraft scenarios

The following equations are internally consistent:

- `straight-line`: 200 m/s east and reported 720 km/h;
- `coordinated-turn`: position is a 6 km circle, heading is tangent, speed is
  `R * omega`, and roll is `atan(v^2 / (R*g))`;
- `figure-eight`: heading and horizontal speed use the analytical derivatives,
  and vertical velocity matches the altitude derivative;
- `mixed-high-dynamics`: horizontal and vertical speeds match the path
  derivatives;
- `dateline-crossing`: 260 m/s east and reported 936 km/h;
- `crossing-target`: 175 m/s east and reported 630 km/h;
- `pole-crossing-route`: `asin(sin(phase))` plus a 180° longitude reversal
  produces a continuous great-circle track, with an expected heading flip at a
  pole.

All `setPosition` calls use first-order spherical equirectangular offsets. The
largest ordinary paths are about 18 km, so they are useful deterministic UI
fixtures, but they are not an authoritative geodesic trajectory. The polar and
dateline cases are deliberately stress patterns rather than physical flight
dynamics.

Two minor semantic inconsistencies remain:

- `default` advances longitude by packet index rather than elapsed time, so
  changing the interval changes physical drift speed while `velocity[0]`
  remains 720 km/h;
- the testsender README says the DLZ aspect scenario sweeps `0°↔180°`, but
  code clamps a single eight-second rise at 180° and never sweeps back.

### DLZ

The DLZ equations, unit conversions, domain guards, strict
`RMIN < RTR < RNE < RPI < RMAX` ordering, range filtering, scale hysteresis,
and shoot-cue hysteresis agree with `docs/DLZ_MODEL.md` and pass their focused
tests. Invalid inputs clear the symbology rather than rendering stale or
unordered values.

That establishes implementation correctness relative to the documented toy
equations. The model explicitly uses fictional coefficients and is not missile
performance, flight mechanics, guidance, or a certified F-16 display. There is
no legitimate external real-world accuracy claim to test.

## Key implementation evidence

| Concern | Primary implementation |
| --- | --- |
| Packet units and altitude meaning | [`PROTOCOL_UNITS.md`](PROTOCOL_UNITS.md) |
| GLB flattening, centring, and normalization | [`glb_model_reader.cpp`](../src/viewer/plane/glb_model_reader.cpp) |
| Fixed aircraft metre scale | [`plane_aircraft_scale.h`](../src/viewer/plane/plane_aircraft_scale.h) |
| Plane altitude, grid, target, and zone projection | [`plane_surface_projection.cpp`](../src/viewer/plane/plane_surface_projection.cpp) |
| Plane 128-segment GPU zones | [`plane_surface_gpu_layer.cpp`](../src/viewer/plane/plane_surface_gpu_layer.cpp) |
| Terrain patch sampling | [`plane_terrain_patch_builder.cpp`](../src/viewer/plane/plane_terrain_patch_builder.cpp) |
| Terrain reuse/recentring | [`plane_scene_widget_terrain.cpp`](../src/viewer/plane/plane_scene_widget_terrain.cpp) |
| DTED structural parser | [`dted_cell_reader.cpp`](../src/viewer/terrain/dted_cell_reader.cpp) |
| Grid/Plane local geographic conversion | [`lar_projection.cpp`](../src/viewer/lar_projection.cpp) |
| Mean-sphere destination formula | [`lar_geodesic_geometry.cpp`](../src/viewer/lar_geodesic_geometry.cpp) |
| Pseudo-Mercator conversion | [`map_projection.cpp`](../src/viewer/map/map_projection.cpp) |
| Orthographic projection and inverse | [`map_camera.cpp`](../src/viewer/map/map_camera.cpp) |
| Adaptive geodesic samples | [`geodesic_zone_sampler.cpp`](../src/viewer/viewport/geodesic_zone_sampler.cpp) |
| Zone resource/error budgets | [`lar_zone_mesh_limits.h`](../src/viewer/viewport/lar_zone_mesh_limits.h) |
| Parametric GPU eligibility | [`lar_parametric_zone_gpu_layer.cpp`](../src/viewer/viewport/lar_parametric_zone_gpu_layer.cpp) |
| Static map float shader path | [`map_shaders.h`](../src/viewer/map/map_shaders.h) |
| Sphere map subdivision | [`map_mesh_compiler.cpp`](../tools/map_asset/map_mesh_compiler.cpp) |
| Marker selection policy | [`lar_marker_layer.cpp`](../src/viewer/viewport/lar_marker_layer.cpp) and [`plane_surface_projection.cpp`](../src/viewer/plane/plane_surface_projection.cpp) |
| Sender kinematics | [`scenarios.cpp`](../src/testsender/scenarios.cpp) |
| DLZ equations and limits | [`DLZ_MODEL.md`](DLZ_MODEL.md) |

## Verification performed

| Check | Result |
| --- | --- |
| Focused visualization/simulation CTest selection | 13/13 passed in 4.34 s |
| Packaged map integrity/format/projection tests | 15/15 passed |
| Native Plane OpenGL test | Passed cleanly |
| Native Mercator/Sphere OpenGL test | Passed cleanly |
| Grid and DLZ offscreen snapshots | Rendered and manually inspected |
| Packaged GLB independent transform/accessor parse | 13,415 positions; dimensions reported above |
| WGS 84 comparison | PROJ `geod` direct/inverse sampling across 0°, 41°, 80° and multiple radii |
| Extreme zoom numerical audit | Camera float spacing, curve midpoint error, and sphere chord error quantified above |

The focused CTest command was:

```bash
ctest --preset ci --output-on-failure \
  -R 'lar-testsender-scenario-tests|lardlz-tests|lardlz-view-tests|larmap-tests|larmap-compiler-tests|larmap-renderer-validation-tests|larzone-tests|larcamera-tests|larmap-loading-tests|larviewport-page-tests|larplane-view-tests|larviewport-worker-tests|larviewer-tests'
```

The native GPU commands were:

```bash
LAR_RUN_PLANE_GPU_TESTS=1 \
  LAR_PLANE_SURFACE_SNAPSHOT=/private/tmp/lar-plane-visual-audit.png \
  ./build-ci/lar-plane-view-gpu-tests

LAR_RUN_MAP_GPU_TESTS=1 ./build-ci/lar-earth-view-gpu-tests
```

The current native GPU assertions prove successful context creation, expected
overlay colours, and camera orientation. They do not compare expected geometry
against framebuffer coordinates.

## Required remediation

### P0: establish physical meaning

1. Define separate view contracts:
   - Mercator/Sphere: WGS 84 latitude/longitude inputs and an explicitly chosen
     Earth model for LAR distances and bearings;
   - Grid/Plane: Euclidean metres in the flat tactical plane, with geographic
     coordinates used only to establish the local anchor;
   - all views: altitude explicitly identified as WGS 84 ellipsoidal height or
     EGM96 orthometric height, with a separate AGL value when that is what the
     producer means.
2. Put the datum/height reference in mapping or session metadata and show it in
   the UI (`3000 m MSL`, `1822 m AGL`, ground `1178 m MSL`). Do not infer AGL
   from an unlabeled altitude.
3. Document the Grid/Plane flat-world contract next to the controls and in the
   protocol/UI help: radii are Euclidean metres in the tactical plane, while
   Mercator/Sphere are the geographic views. Keep the `20,000 km` input cap
   explicitly labelled as a resource/rendering bound, not a physical-distance
   guarantee. If a future mode requires Earth distances, add a separate
   geodesic construction path rather than changing the flat view implicitly.

### P1: make accuracy budgets enforceable

4. Separate outline accuracy from fill-cell density. Maintain the curve pixel
   target for every accepted radius/zoom, tile/clip visible arcs, or reject and
   diagnose geometry that cannot meet it.
5. Use camera-relative or high/low coordinates for the Mercator base map, and
   runtime zoom-dependent subdivision/LOD for the globe. Cap zoom to the source
   dataset's useful scale until a higher-resolution source is available.
6. Add an aircraft asset manifest with forward/up axes, physical length,
   wingspan, height, and reference origin. Use `14.8 m` if the packaged model is
   intended to match the cited F-16C/D dimensions; require explicit dimensions
   for uploaded models instead of assigning every jet 15 m.
7. Parse and expose DTED horizontal datum, vertical datum, security/source, and
   ACC accuracy fields. Reject incompatible tiles for metric terrain mode.
   Reposition terrain in one fixed ECEF/ENU frame so patch reuse does not change
   its local basis.

### P2: verification and consistency

8. Add framebuffer metrology tests that compare sampled boundary, centre,
   model-extents, and ground pixels to binary64 reference projections. Cover:
   - latitudes 0°, 41°, 80°;
   - the antimeridian and Mercator cutoff;
   - minimum and maximum zoom;
   - local, 26 km, 100 km, 1,000 km, and maximum accepted radii;
   - flat zero-datum, known DTED terrain, and missing-altitude cases.
9. Add explicit numerical acceptance thresholds. Suggested starting points are
   `<= 1 m` centre/range error against the chosen geodesic reference,
   `<= 0.65 px` visible boundary error, and `<= 0.1 m` computational vertical
   transform error, with terrain source uncertainty reported separately.
10. Share one target-marker fallback policy across all views, fix the sender
    scenario descriptions, and label the testsender/DLZ prominently as display
    fixtures rather than physical simulations.

## Release decision

The current build is suitable for deterministic UI demonstrations and for
visualization against its documented flat-local and spherical mathematical
models. It should not be described as a single geodetic/navigation product
across all views, and it should not be used for navigation, terrain clearance,
or targeting decisions.

Release as a metrically correct visualization should wait until V-02 and V-04
are resolved and the chosen accuracy thresholds are enforced by tests. V-01
should be closed through explicit flat-view documentation and model-specific
tests. V-03 through V-07 should either be corrected or converted into explicit,
user-visible operating limits.
