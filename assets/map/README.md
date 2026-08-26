# World Boundary Source

`world_boundaries.geojson` contains Natural Earth Admin-0 country boundary
data used to generate LAR's packaged Earth-map mesh. Natural Earth publishes
its vector and raster map data in the public domain:

https://www.naturalearthdata.com/about/terms-of-use/

The source file is a build input and is not installed with the application.
The generated `.larmap` package contains only the geometry required by the
Mercator and spherical viewport presentations.
