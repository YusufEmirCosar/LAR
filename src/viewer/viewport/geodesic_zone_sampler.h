#pragma once

/**
 * @file geodesic_zone_sampler.h
 * @brief Bounded adaptive sampling of validated geographic LAR zones.
 */

#include "viewer/map/map_camera.h"
#include "viewer/viewport/lar_zone_input_validator.h"

#include <cstddef>
#include <vector>

/** Deterministic geographic sample grid for one validated zone. */
struct GeodesicZoneSampleGrid final {
    std::vector<double> radii;
    std::vector<double> bearings;
    std::vector<GeoCoordinateRadians> points; // row-major
    double startBearingRadians = 0.0;
    double seamPoleDistanceMeters = 0.0;
    bool fullCircle = false;

    [[nodiscard]] std::size_t rowCount() const noexcept {
        return radii.size();
    }
    [[nodiscard]] std::size_t columnCount() const noexcept {
        return bearings.size();
    }
    [[nodiscard]] const GeoCoordinateRadians &point(std::size_t row,
                                                    std::size_t column) const noexcept {
        return points[row * columnCount() + column];
    }
};

/** Selects bounded density and samples geodesic ring/sector coordinates. */
class GeodesicZoneSampler final {
  public:
    [[nodiscard]] GeodesicZoneSampleGrid sample(const LarZoneDefinition &zone,
                                                const lar::map::MapCamera &camera,
                                                int viewportWidth, int viewportHeight) const;
};
