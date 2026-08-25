#pragma once

/**
 * @file polygon_triangulator.h
 * @brief Hole-aware planar polygon triangulation contract.
 */

#include "source_map.h"

#include <cstdint>
#include <vector>

namespace lar::map::tool {

/** @brief Flattened polygon coordinates with fill and border indices. */
struct PlanarPolygonMesh final {
    std::vector<SourceCoordinate> coordinates;
    std::vector<std::uint32_t> fillIndices;
    std::vector<std::uint32_t> borderIndices;

    /**
     * @brief Reports whether triangulation produced no renderable geometry.
     *
     * @details The operation observes the current object state without modifying it.
     *
     * @return True when coordinates or fill indices are empty.
     */
    [[nodiscard]] bool empty() const noexcept {
        return coordinates.empty() || fillIndices.empty();
    }
};

/** @brief Normalizes winding and triangulates a source polygon. */
class PolygonTriangulator final {
  public:
    static PlanarPolygonMesh triangulate(const SourcePolygon &polygon);
};

} // namespace lar::map::tool
