#pragma once

/**
 * @file plane_terrain_patch.h
 * @brief Immutable CPU terrain mesh and bounded build request for Plane mode.
 */

#include "viewer/plane/plane_land_mask.h"
#include "viewer/terrain/dted_level.h"

#include <QMetaType>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

/** Position, normal, and bathymetric-depth floats stored per terrain vertex. */
inline constexpr std::size_t PlaneTerrainVertexStrideFloats = 7U;

/** @brief Parameters for one aircraft-centered terrain build. */
struct PlaneTerrainBuildRequest final {
    quint64 revision = 0;
    double latitudeRadians = 0.0;
    double longitudeRadians = 0.0;
    // NaN means use the request latitude (for standalone builders and old callers).
    double projectionOriginLatitudeRadians = std::numeric_limits<double>::quiet_NaN();
    double halfExtentMeters = 0.0;
    double metersPerSceneUnit = 1.0;
    int resolution = 0;
};

/** @brief CPU mesh whose positions are local to a stable geographic anchor. */
struct PlaneTerrainPatch final {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<unsigned char> sampleValidity;
    PlaneLandMask landMask;
    double anchorLatitudeRadians = 0.0;
    double anchorLongitudeRadians = 0.0;
    // All local east/north coordinates use this fixed latitude scale. Keeping it stable while
    // the aircraft moves makes patch translation algebraically match PlaneSurfaceProjection.
    double projectionOriginLatitudeRadians = std::numeric_limits<double>::quiet_NaN();
    double halfExtentMeters = 0.0;
    double metersPerSceneUnit = 1.0;
    double minimumElevationMeters = 0.0;
    double maximumElevationMeters = 0.0;
    double centerElevationMeters = 0.0;
    double maximumWaterDepthMeters = 0.0;
    std::size_t validSampleCount = 0U;
    std::size_t waterSampleCount = 0U;
    int resolution = 0;
    DtedLevel sourceLevel = DtedLevel::Level0;

    [[nodiscard]] bool empty() const noexcept {
        return vertices.empty() || indices.empty();
    }

    [[nodiscard]] std::size_t vertexCount() const noexcept {
        return vertices.size() / PlaneTerrainVertexStrideFloats;
    }
};

using PlaneTerrainPatchPtr = std::shared_ptr<const PlaneTerrainPatch>;

Q_DECLARE_METATYPE(PlaneTerrainBuildRequest)
Q_DECLARE_METATYPE(PlaneTerrainPatchPtr)
