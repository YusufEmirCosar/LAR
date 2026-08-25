#!/usr/bin/env python3
"""Build a compact DTED0-post-aligned land/water mask from Natural Earth land polygons."""

from __future__ import annotations

import argparse
import os
import struct
import sys
import tempfile
import zlib
from dataclasses import dataclass
from pathlib import Path

try:
    import numpy as np
    from osgeo import gdal, ogr, osr
except ImportError as error:  # pragma: no cover - exercised only on misconfigured hosts
    raise SystemExit(
        "GDAL Python bindings and NumPy are required. Install them for the Python "
        f"interpreter running this script: {error}"
    ) from error


MAGIC = b"LARWMSK1"
VERSION = 1
HEADER = struct.Struct("<8sHHIHHIQ")
ENTRY = struct.Struct("<BBBBIII")
CELL_COUNT = 360 * 180
POSTS_PER_DEGREE = 120
GLOBAL_WIDTH = 360 * POSTS_PER_DEGREE + 1
GLOBAL_HEIGHT = 180 * POSTS_PER_DEGREE + 1
STATE_MISSING = 0
STATE_LAND = 1
STATE_WATER = 2
STATE_MIXED = 3


@dataclass(frozen=True)
class DtedMetadata:
    """Minimal validated DTED UHL metadata needed to align one mask cell."""

    longitude: int
    latitude: int
    longitude_count: int
    latitude_count: int
    longitude_step: int


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Rasterize Natural Earth land polygons at exact DTED0 post coordinates and "
            "write an indexed mask pack."
        )
    )
    parser.add_argument("--dted-root", required=True, type=Path, help="DTED0 directory")
    parser.add_argument(
        "--land", required=True, type=Path, help="Natural Earth ne_10m_land.shp path"
    )
    parser.add_argument("--output", required=True, type=Path, help="Output mask pack")
    parser.add_argument(
        "--allow-missing",
        action="store_true",
        help="Write missing entries instead of requiring all 64,800 degree cells",
    )
    parser.add_argument(
        "--force", action="store_true", help="Replace an existing output pack atomically"
    )
    return parser.parse_args()


def parse_origin(field: bytes) -> float:
    if len(field) != 8:
        raise ValueError("invalid UHL origin width")
    degrees = int(field[0:3])
    minutes = int(field[3:5])
    seconds = int(field[5:7])
    hemisphere = chr(field[7])
    if minutes >= 60 or seconds >= 60 or hemisphere not in "EWNS":
        raise ValueError("invalid UHL origin")
    result = degrees + minutes / 60.0 + seconds / 3600.0
    return -result if hemisphere in "WS" else result


def key_from_path(path: Path) -> tuple[int, int]:
    directory = path.parent.name.lower()
    filename = path.stem.lower()
    if (
        len(directory) != 4
        or directory[0] not in "ew"
        or not directory[1:].isdigit()
        or len(filename) != 3
        or filename[0] not in "ns"
        or not filename[1:].isdigit()
    ):
        raise ValueError(f"non-standard DTED0 path: {path}")
    longitude = int(directory[1:]) * (-1 if directory[0] == "w" else 1)
    latitude = int(filename[1:]) * (-1 if filename[0] == "s" else 1)
    return longitude, latitude


def read_dted_metadata(path: Path) -> DtedMetadata:
    with path.open("rb") as stream:
        uhl = stream.read(80)
    if len(uhl) != 80 or uhl[:4] != b"UHL1":
        raise ValueError(f"{path}: missing 80-byte UHL1 header")
    try:
        longitude_origin = parse_origin(uhl[4:12])
        latitude_origin = parse_origin(uhl[12:20])
        longitude_interval_tenths = int(uhl[20:24])
        latitude_interval_tenths = int(uhl[24:28])
        longitude_count = int(uhl[47:51])
        latitude_count = int(uhl[51:55])
    except ValueError as error:
        raise ValueError(f"{path}: malformed UHL fields") from error

    path_key = key_from_path(path)
    origin_key = (round(longitude_origin), round(latitude_origin))
    if origin_key != path_key or abs(longitude_origin - origin_key[0]) > 1.0e-9 or abs(
        latitude_origin - origin_key[1]
    ) > 1.0e-9:
        raise ValueError(f"{path}: UHL origin does not match its path")
    if not (2 <= longitude_count <= 121 and 2 <= latitude_count <= 121):
        raise ValueError(f"{path}: dimensions exceed DTED0 bounds")
    if (
        (longitude_count - 1) * longitude_interval_tenths != 36000
        or (latitude_count - 1) * latitude_interval_tenths != 36000
        or latitude_interval_tenths != 300
        or longitude_interval_tenths % 300 != 0
    ):
        raise ValueError(f"{path}: sample intervals are not DTED0-post aligned")
    longitude_step = longitude_interval_tenths // 300
    return DtedMetadata(
        longitude=path_key[0],
        latitude=path_key[1],
        longitude_count=longitude_count,
        latitude_count=latitude_count,
        longitude_step=longitude_step,
    )


def discover_dted(root: Path) -> dict[tuple[int, int], DtedMetadata]:
    if not root.is_dir():
        raise ValueError(f"DTED0 root is not a directory: {root}")
    result: dict[tuple[int, int], DtedMetadata] = {}
    for path in sorted(root.glob("[ew][0-9][0-9][0-9]/[ns][0-9][0-9].dt0")):
        metadata = read_dted_metadata(path)
        key = (metadata.longitude, metadata.latitude)
        if key in result:
            raise ValueError(f"duplicate DTED0 cell: {key}")
        result[key] = metadata
    return result


def verify_land_source(path: Path) -> None:
    if path.suffix.lower() != ".shp" or not path.is_file():
        raise ValueError(f"land source is not a shapefile: {path}")
    required = (".dbf", ".shx", ".prj")
    missing = [str(path.with_suffix(suffix)) for suffix in required if not path.with_suffix(suffix).is_file()]
    if missing:
        raise ValueError("land shapefile sidecars are missing: " + ", ".join(missing))


def rasterize_land(path: Path, destination: Path) -> gdal.Dataset:
    vector = gdal.OpenEx(str(path), gdal.OF_VECTOR | gdal.OF_READONLY)
    if vector is None:
        raise ValueError(f"GDAL could not open land polygons: {path}")
    layer = vector.GetLayer(0)
    source_srs = layer.GetSpatialRef()
    wgs84 = osr.SpatialReference()
    wgs84.ImportFromEPSG(4326)
    wgs84.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    if source_srs is None:
        raise ValueError("land shapefile has no coordinate reference system")
    source_srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    if not source_srs.IsSame(wgs84):
        raise ValueError("land shapefile must use WGS 84 / EPSG:4326")

    driver = gdal.GetDriverByName("GTiff")
    raster = driver.Create(
        str(destination),
        GLOBAL_WIDTH,
        GLOBAL_HEIGHT,
        1,
        gdal.GDT_Byte,
        options=[
            "TILED=YES",
            "BLOCKXSIZE=512",
            "BLOCKYSIZE=512",
            "COMPRESS=DEFLATE",
            "PREDICTOR=1",
            "NBITS=1",
            "BIGTIFF=IF_SAFER",
        ],
    )
    if raster is None:
        raise ValueError("GDAL could not create the temporary land raster")
    post_spacing = 1.0 / POSTS_PER_DEGREE
    raster.SetGeoTransform(
        (
            -180.0 - post_spacing * 0.5,
            post_spacing,
            0.0,
            90.0 + post_spacing * 0.5,
            0.0,
            -post_spacing,
        )
    )
    raster.SetProjection(wgs84.ExportToWkt())
    band = raster.GetRasterBand(1)
    band.Fill(0)
    error = gdal.RasterizeLayer(raster, [1], layer, burn_values=[1])
    if error != gdal.CE_None:
        raise ValueError("GDAL failed while rasterizing land polygons")
    band.FlushCache()
    raster.FlushCache()
    vector = None
    return raster


def cell_index(longitude: int, latitude: int) -> int:
    return (latitude + 90) * 360 + longitude + 180


def pack_entry(
    state: int,
    metadata: DtedMetadata | None,
    payload_offset: int = 0,
    payload: bytes = b"",
) -> bytes:
    if metadata is None:
        return ENTRY.pack(STATE_MISSING, 0, 0, 0, 0, 0, 0)
    checksum = zlib.crc32(payload) & 0xFFFFFFFF if payload else 0
    stored_offset = payload_offset if payload else 0
    return ENTRY.pack(
        state,
        metadata.longitude_count,
        metadata.latitude_count,
        0,
        stored_offset,
        len(payload),
        checksum,
    )


def build_pack(
    metadata_by_key: dict[tuple[int, int], DtedMetadata],
    land_raster: gdal.Dataset,
    output: Path,
    force: bool,
) -> tuple[int, int, int, int]:
    if output.exists() and not force:
        raise ValueError(f"output already exists (use --force): {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    payload_start = HEADER.size + CELL_COUNT * ENTRY.size
    entries = [pack_entry(STATE_MISSING, None)] * CELL_COUNT
    temporary_name: str | None = None
    statistics = {STATE_MISSING: 0, STATE_LAND: 0, STATE_WATER: 0, STATE_MIXED: 0}
    try:
        with tempfile.NamedTemporaryFile(
            mode="w+b", prefix=output.name + ".", suffix=".tmp", dir=output.parent, delete=False
        ) as stream:
            temporary_name = stream.name
            stream.write(b"\0" * payload_start)
            payload_offset = payload_start
            band = land_raster.GetRasterBand(1)
            for latitude in range(-90, 90):
                north_row = (89 - latitude) * POSTS_PER_DEGREE
                strip = band.ReadAsArray(0, north_row, GLOBAL_WIDTH, POSTS_PER_DEGREE + 1)
                if strip is None or strip.shape != (POSTS_PER_DEGREE + 1, GLOBAL_WIDTH):
                    raise ValueError(f"could not read land-mask latitude band {latitude}")
                south_to_north = strip[::-1, :]
                for longitude in range(-180, 180):
                    key = (longitude, latitude)
                    metadata = metadata_by_key.get(key)
                    index = cell_index(longitude, latitude)
                    if metadata is None:
                        statistics[STATE_MISSING] += 1
                        continue
                    west_column = (longitude + 180) * POSTS_PER_DEGREE
                    columns = slice(
                        west_column,
                        west_column + POSTS_PER_DEGREE + 1,
                        metadata.longitude_step,
                    )
                    tile = south_to_north[:, columns]
                    expected_shape = (metadata.latitude_count, metadata.longitude_count)
                    if tile.shape != expected_shape:
                        raise ValueError(
                            f"mask shape {tile.shape} does not match {expected_shape} for {key}"
                        )
                    if not tile.any():
                        state = STATE_WATER
                        payload = b""
                    elif tile.all():
                        state = STATE_LAND
                        payload = b""
                    else:
                        state = STATE_MIXED
                        ordered = np.logical_not(tile).T.reshape(-1)
                        payload = np.packbits(ordered, bitorder="little").tobytes()
                    entries[index] = pack_entry(state, metadata, payload_offset, payload)
                    statistics[state] += 1
                    if payload:
                        stream.write(payload)
                        payload_offset += len(payload)
                if latitude % 10 == 9:
                    print(f"processed through latitude {latitude:+03d}", file=sys.stderr)

            file_size = stream.tell()
            header = HEADER.pack(
                MAGIC,
                VERSION,
                HEADER.size,
                CELL_COUNT,
                ENTRY.size,
                0,
                payload_start,
                file_size,
            )
            stream.seek(0)
            stream.write(header)
            stream.write(b"".join(entries))
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temporary_name, 0o644)
        os.replace(temporary_name, output)
        temporary_name = None
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
    return (
        statistics[STATE_MISSING],
        statistics[STATE_LAND],
        statistics[STATE_WATER],
        statistics[STATE_MIXED],
    )


def main() -> int:
    arguments = parse_arguments()
    gdal.UseExceptions()
    ogr.UseExceptions()
    try:
        verify_land_source(arguments.land)
        metadata = discover_dted(arguments.dted_root)
        missing = CELL_COUNT - len(metadata)
        if missing and not arguments.allow_missing:
            raise ValueError(
                f"DTED0 root has {len(metadata):,} cells; expected {CELL_COUNT:,}. "
                "Use --allow-missing only for an intentionally partial source."
            )
        print(f"validated {len(metadata):,} DTED0 cells", file=sys.stderr)
        with tempfile.TemporaryDirectory(prefix="lar-water-mask-") as temporary:
            raster_path = Path(temporary) / "land-posts.tif"
            land_raster = rasterize_land(arguments.land, raster_path)
            statistics = build_pack(
                metadata, land_raster, arguments.output, arguments.force
            )
            land_raster = None
        print(
            "wrote "
            f"{arguments.output} ({arguments.output.stat().st_size:,} bytes): "
            f"missing={statistics[0]:,}, land={statistics[1]:,}, "
            f"water={statistics[2]:,}, mixed={statistics[3]:,}"
        )
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        print(f"water-mask generation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
