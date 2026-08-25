#pragma once

/**
 * @file lar_zone_mesh_limits.h
 * @brief Central memory, complexity, radius, and accuracy bounds for zone meshes.
 */

#include "viewer/lar_geodesic_geometry.h"

#include <cstddef>

/**
 * Central resource and accuracy limits for CPU LAR geometry preparation.
 *
 * A build produces at most 12,000 vertices and 48,000 indices. Each zone is
 * capped at 2,600 fill cells, 2,048 angular subdivisions, and 48 radial
 * subdivisions. Radii above 20,000 km are rejected before sampling. These
 * limits keep worker memory and execution bounded independently of input.
 */
namespace LarZoneMeshLimits {

inline constexpr std::size_t MaximumVertexCount = 12'000U;
inline constexpr std::size_t MaximumIndexCount = 48'000U;
inline constexpr double MaximumRadiusMeters = 20'000'000.0;
inline constexpr int MaximumAngularSegmentCount = 2048;
inline constexpr int MaximumRadialSegmentCount = 48;
inline constexpr int MaximumFillCellsPerZone = 2600;
inline constexpr double CurveErrorPixels = 0.65;
inline constexpr double MaximumRadialStepRadians = LarGeodesicGeometry::Pi / 36.0;
inline constexpr double FullCircleTolerance = 1.0e-9;
inline constexpr double SampleTolerance = 1.0e-7;

} // namespace LarZoneMeshLimits
