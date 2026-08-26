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
     */
    [[nodiscard]] bool empty() const noexcept {
        return coordinates.empty() || fillIndices.empty();
    }
};

/**
 * @brief Normalizes winding and triangulates a geographic source polygon.
 *
 * Input rings are projection-neutral longitude/latitude degrees. Consecutive
 * longitudes are unwrapped before planar ear clipping, so a ring crossing the
 * antimeridian follows its short edges instead of spanning the map. The
 * exterior is normalized counter-clockwise, holes clockwise, and holes are
 * joined to the exterior with non-intersecting interior bridges.
 *
 * Output indices address the returned normalized coordinate array. Empty output
 * indicates a malformed or degenerate exterior, an unbridgeable hole, index
 * overflow, or a polygon that cannot be triangulated safely.
 */
class PolygonTriangulator final {
  public:
    static PlanarPolygonMesh triangulate(const SourcePolygon &polygon);
};

} // namespace lar::map::tool
