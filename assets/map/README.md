# World Boundary Source

`world_boundaries.geojson` contains Natural Earth Admin-0 country polygons used
to generate LAR's packaged Earth-map mesh and shared land/sea index. Natural
Earth publishes its vector and raster map data in the public domain:

https://www.naturalearthdata.com/about/terms-of-use/

The source file is a build input and is not installed with the application.
The generated LRM1 version-2 `.larmap` contains the geometry required by the
Mercator and spherical viewport presentations plus a 360-by-180 one-degree
triangle index. Plane terrain uses that same index to rasterize a bounded local
land mask, so Earth and Plane have one coastline authority. Runtime code never
parses this GeoJSON directly; rebuild `lar-map-asset` after changing the source.
