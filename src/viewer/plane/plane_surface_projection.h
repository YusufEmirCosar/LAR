#pragma once

/**
 * @file plane_surface_projection.h
 * @brief Pure geographic-to-local projection and scale policy for Plane mode.
 */

#include "viewer/lar_geodesic_geometry.h"
#include "viewer/plane/plane_surface_state.h"
#include "viewer/viewport/lar_scene_state.h"

#include <optional>

/** @brief Builds stable local metric surface values around the aircraft. */
class PlaneSurfaceProjection final {
  public:
    [[nodiscard]] static PlaneSurfaceState
    project(const LarSceneState &scene,
            double metersPerSceneUnit = PlaneAircraftScale::DefaultMetersPerSceneUnit,
            const std::optional<GeoCoordinateRadians> &groundOrigin = std::nullopt) noexcept;
};
