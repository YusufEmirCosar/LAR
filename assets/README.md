# Assets

This is the single source-asset tree for the project. Source code must not be
placed here, and build outputs must remain under a configured `build-*`
directory.

| Directory | Purpose | Packaging behavior |
| --- | --- | --- |
| `map/` | Natural Earth GeoJSON used by the map compiler | Compiled to `.larmap`; the GeoJSON itself is not installed |
| `models/` | Packaged runtime aircraft models | Validated at runtime and copied/installed under `assets/models` beside executables; user-uploaded Plane models are session-only and are not staged here |
| `cubemaps/` | Runtime six-face PNG cubemap sets | Grouped and validated before GPU upload; copied/installed under `assets/cubemaps` |
| `DTED0/` | WGS84 DTED Level-0 elevation cells used by Plane terrain | Addressed lazily in place; not copied or installed by default because the tree is about 1.7 GB |
| `ne_10m_land/` | Natural Earth 1:10m land polygons used to compile the water mask | Build-time source; not loaded or installed at runtime |
| `water/` | Compact DTED0-aligned generated water mask | Validated lazily and copied/installed under `assets/water` |
| `ui/icons/` | Qt resource collection and raster toolbar/navigation icons | Embedded in executables through `icons.qrc` |
| `ui/styles/` | Application-wide Qt stylesheet | Embedded by the UI resource collection |

The resource aliases (`:/icons/...` and `:/styles/mainwindow.qss`) are stable;
moving their source files here does not change runtime callers.

## DTED terrain

Plane mode resolves cells by their conventional degree-aligned path, for
example `DTED0/e035/n39.dt0`; it never scans the full directory at startup.
The native reader accepts bounded Level-0 dimensions, verifies UHL/DSI/ACC
structure and profile checksums, decodes signed-magnitude metre elevations,
and honors the no-data value. Variable longitude-profile counts near the poles
are supported.

The Plane **Upload > DTED Folder** action accepts external Level-1 and Level-2
trees using the same directory layout and `.dt1`/`.dt2` suffixes. The selected
tree is validated, read lazily in place, and used only for the current session;
it is never copied into this asset tree or a build/install output. The reader
streams high-resolution profiles, while terrain caching is bounded by both
entry count and decoded bytes.

The runtime root is selected in this order: `LAR_DTED0_ROOT`, an
executable-adjacent `assets/DTED0`, then the CMake `LAR_DTED0_ROOT` development
path compiled into the presentation target. The large source tree is
deliberately excluded from `lar_copy_plane_assets` and default install rules.
For a deployment, copy or mount the tree beside the executable, or set the
environment variable to an external data location. Record the dataset's
provenance, redistribution terms, and required attribution before distributing
these files.

Negative DTED elevations are preserved as bathymetric depth. The generated
water mask keeps below-sea-level land distinct from ocean, then Plane mode
places classified water vertices at mean sea level and passes the retained
depth to the blue shader ramp. Regenerate the mask with
`tools/build_dted_water_mask.py` whenever the DTED tree or Natural Earth land
source changes. Runtime lookup checks `LAR_DTED0_WATER_MASK`, an
executable-adjacent `assets/water/dted0_water_mask.bin`, then the configured
development source file.

The land polygons are Natural Earth 1:10m physical vectors, version 5.1.1.
Natural Earth publishes its data in the public domain; see its
[terms of use](https://www.naturalearthdata.com/about/terms-of-use/).

## Plane assets

`models/f16_3.glb` is the glTF 2.0 binary loaded by Plane mode. Its embedded
metadata identifies the model as **F16-C Falcon** by
[Carlos.Maciel](https://sketchfab.com/Carlos.Maciel), sourced from
[Sketchfab](https://sketchfab.com/3d-models/f16-c-falcon-4bc2ff75dc584af2afd0aa6bd8b79015)
under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Preserve that
attribution when redistributing the application.

Each cubemap uses six PNG files with one shared prefix and the suffixes `_rt`,
`_lf`, `_up`, `_dn`, `_ft`, and `_bk`. Every face in a set must be a readable,
matching square image no larger than 4096 pixels per side. Sets are ordered by
their shared prefix; incomplete or mismatched sets are excluded from the
runtime selector. The current cubemap files do not contain source or license
metadata; their redistribution rights and required attribution must be
recorded before a public release.
