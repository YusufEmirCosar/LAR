# DTED0 water mask

`dted0_water_mask.bin` is a generated runtime asset that classifies every DTED0
post as land or water. It is derived from Natural Earth 1:10m land polygons,
version 5.1.1. Natural Earth data is in the public domain; see its
[terms of use](https://www.naturalearthdata.com/about/terms-of-use/).

Regenerate the pack from the repository root after replacing either the DTED0
tree or the Natural Earth source:

```bash
python3 tools/build_dted_water_mask.py \
  --dted-root assets/DTED0 \
  --land assets/ne_10m_land/ne_10m_land.shp \
  --output assets/water/dted0_water_mask.bin \
  --force
```

The script requires NumPy and GDAL Python bindings only at generation time.
The application has no GDAL runtime dependency.

The little-endian version-1 format starts with a fixed 32-byte header and
64,800 fixed 16-byte index entries in latitude-major, then longitude-major
order. An entry is missing, all land, all water, or mixed. Only mixed cells
store a least-significant-bit-first post mask and CRC32 payload. Counts are
copied from each DTED UHL header, including reduced polar longitude widths.
The native reader validates all bounds, dimensions, offsets, padding bits, and
checksums before exposing a cell.

For external DT1/DT2 terrain, Plane mode maps geographic cell fractions onto
this native DTED0-aligned mask. Elevations therefore gain the selected source's
detail while coastline classification retains the mask pack's DTED0 resolution.
