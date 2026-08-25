#pragma once

/**
 * @file source_map.h
 * @brief Projection-neutral polygon model produced from source GeoJSON.
 */

#include <vector>

namespace lar::map::tool {

/** @brief Geographic source coordinate expressed in degrees. */
struct SourceCoordinate final {
    double longitudeDegrees = 0.0;
    double latitudeDegrees = 0.0;
};

using SourceRing = std::vector<SourceCoordinate>;

/** @brief Polygon exterior and zero or more interior hole rings. */
struct SourcePolygon final {
    SourceRing exterior;
    std::vector<SourceRing> holes;
};

/** @brief Bounded collection of source polygons and coordinate accounting. */
struct SourceMap final {
    std::vector<SourcePolygon> polygons;
    std::size_t coordinateCount = 0U;

    /**
     * @brief Reports whether the source map has no renderable polygon data.
     *
     * @details The operation observes the current object state without modifying it.
     *
     * @return True when no polygons or source coordinates are present.
     */
    [[nodiscard]] bool empty() const noexcept {
        return polygons.empty() || coordinateCount == 0U;
    }
};

} // namespace lar::map::tool
