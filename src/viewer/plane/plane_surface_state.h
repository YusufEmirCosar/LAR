#pragma once

/**
 * @file plane_surface_state.h
 * @brief Projection-ready local ground, target, and LAR values for Plane mode.
 */

#include "viewer/plane/plane_aircraft_scale.h"

#include <QVector2D>

/** Half-count of prebuilt grid lines on each side of the aircraft. */
inline constexpr int PlaneSurfaceGridHalfLineCount = 600;
/** Maximum finite coordinate accepted by the Plane surface renderer. */
inline constexpr float PlaneSurfaceMaximumCoordinate = 8'000'000.0F;

/** @brief One local flat LAR patch expressed in Plane-scene units. */
struct PlaneSurfaceZone final {
    QVector2D centerXZ;
    float innerRadius = 0.0F;
    float outerRadius = 0.0F;
    float startBearingRadians = 0.0F;
    float spanRadians = 0.0F;
    bool visible = false;
    bool fullCircle = false;
};

/** @brief Immutable metric, altitude, and Earth-fixed grid state consumed by the GPU layer. */
struct PlaneSurfaceState final {
    PlaneSurfaceZone inRange;
    PlaneSurfaceZone inZone;
    QVector2D targetXZ;
    QVector2D gridPhaseXZ;
    QVector2D groundOriginXZ;
    double metersPerSceneUnit = PlaneAircraftScale::DefaultMetersPerSceneUnit;
    double gridSpacingMeters = 4.0;
    float gridSpacingSceneUnits = 0.5F;
    float targetMarkerScale = 0.4F;
    float surfaceHalfExtent = 300.0F;
    float surfaceHeight = -1.3F;
    bool geographicAnchorValid = false;
    bool targetVisible = false;
};
