#pragma once

/**
 * @file plane_aircraft_scale.h
 * @brief Uses the normalized F-16 mesh as Plane mode's fixed metric reference.
 */

/** @brief Pure policy for deriving surface units without resizing the aircraft. */
class PlaneAircraftScale final {
  public:
    /** Physical nose-to-tail length used by Plane mode. */
    static constexpr double F16LengthMeters = 15.0;
    /** Fallback for normalized models whose longest dimension is two scene units. */
    static constexpr double DefaultMetersPerSceneUnit = F16LengthMeters / 2.0;

    [[nodiscard]] static double metersPerSceneUnit(float forwardExtentSceneUnits) noexcept;
};
