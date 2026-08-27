#pragma once

/**
 * @file plane_land_mask.h
 * @brief Adaptive local land coverage derived from the packaged world-map triangles.
 */

#include "viewer/map/map_land_index.h"

#include <functional>
#include <vector>

/** @brief Storage shortcut for a uniformly classified or rasterized Plane patch. */
enum class PlaneLandCoverage { AllWater, AllLand, Mixed };

/** @brief Immutable south-to-north R8 land mask for one local metric patch. */
struct PlaneLandMask final {
    PlaneLandCoverage coverage = PlaneLandCoverage::AllWater;
    int resolution = 0;
    double halfExtentMeters = 0.0;
    std::vector<unsigned char> texels;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool landAtLocal(double eastMeters, double northMeters) const noexcept;
    [[nodiscard]] std::size_t storageBytes() const noexcept {
        return texels.size();
    }
};

/** @brief Builds a bounded local raster by querying only indexed map triangles. */
class PlaneLandMaskBuilder final {
  public:
    explicit PlaneLandMaskBuilder(lar::map::MapLandIndex landIndex);

    [[nodiscard]] PlaneLandMask build(double anchorLatitudeRadians, double anchorLongitudeRadians,
                                      double projectionOriginLatitudeRadians,
                                      double halfExtentMeters,
                                      const std::function<bool()> &cancelled = {}) const;

    [[nodiscard]] bool isAvailable() const noexcept {
        return m_landIndex.isValid();
    }

    [[nodiscard]] const lar::map::MapLandIndex &landIndex() const noexcept {
        return m_landIndex;
    }

    [[nodiscard]] static int resolutionFor(double halfExtentMeters) noexcept;

  private:
    lar::map::MapLandIndex m_landIndex;
};
