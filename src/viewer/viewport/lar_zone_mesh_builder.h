#pragma once

/**
 * @file lar_zone_mesh_builder.h
 * @brief Bounded geodesic tessellation of Target LAR values for OpenGL.
 */

#include "domain/state.h"
#include "viewer/map/map_camera.h"
#include "viewer/viewport/lar_zone_mesh.h"
#include "viewer/viewport/lar_zone_mesh_limits.h"

#include <QBitArray>

/** @brief Converts validated LAR parameters into clipped render geometry. */
class LarZoneMeshBuilder final {
  public:
    static constexpr std::size_t MaximumVertexCount = LarZoneMeshLimits::MaximumVertexCount;
    static constexpr std::size_t MaximumIndexCount = LarZoneMeshLimits::MaximumIndexCount;
    static constexpr double MaximumRadiusMeters = LarZoneMeshLimits::MaximumRadiusMeters;

    [[nodiscard]] LarZoneMesh build(const Target &target, const QBitArray &availableFields,
                                    const lar::map::MapCamera &camera, int viewportWidth,
                                    int viewportHeight) const;
};
