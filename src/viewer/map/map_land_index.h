#pragma once

/**
 * @file map_land_index.h
 * @brief Bounded geographic queries over the packaged world-map fill triangles.
 */

#include "viewer/map/map_mesh.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace lar::map {

/** @brief Unwrapped geographic bounds in degrees; east may exceed 180 degrees. */
struct MapGeographicBounds final {
    double westDegrees = 0.0;
    double eastDegrees = 0.0;
    double southDegrees = 0.0;
    double northDegrees = 0.0;
};

/** @brief Read-only spatial index over the planar land triangles in one map mesh. */
class MapLandIndex final {
  public:
    explicit MapLandIndex(std::shared_ptr<const MapMesh> mesh = {});

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] const std::shared_ptr<const MapMesh> &mesh() const noexcept {
        return m_mesh;
    }

    /** Returns whether one finite WGS84 degree coordinate lies in a land triangle. */
    [[nodiscard]] bool contains(double latitudeDegrees, double longitudeDegrees) const noexcept;

    /** Appends possibly duplicated candidate triangle ordinals for bounded unwrapped bounds. */
    void appendCandidates(const MapGeographicBounds &bounds,
                          std::vector<std::uint32_t> &destination) const;

  private:
    std::shared_ptr<const MapMesh> m_mesh;
    bool m_valid = false;
};

/** Validates cell ranges and triangle references without allocating. */
[[nodiscard]] bool mapLandIndexIsValid(const MapMesh &mesh) noexcept;

} // namespace lar::map
