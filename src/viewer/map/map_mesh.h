#pragma once

/**
 * @file map_mesh.h
 * @brief CPU-side vertex/index representation shared by tools and renderer.
 */

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lar::map {

/** Number of one-degree longitude buckets in the packaged land index. */
inline constexpr std::size_t MapLandLongitudeCellCount = 360U;
/** Number of one-degree latitude buckets in the packaged land index. */
inline constexpr std::size_t MapLandLatitudeCellCount = 180U;
/** Total number of fixed one-degree buckets in the packaged land index. */
inline constexpr std::size_t MapLandCellCount =
    MapLandLongitudeCellCount * MapLandLatitudeCellCount;

/** @brief Contiguous triangle-reference range for one geographic degree cell. */
struct MapLandCellRange final {
    std::uint32_t firstReference = 0U;
    std::uint32_t referenceCount = 0U;

    bool operator==(const MapLandCellRange &other) const noexcept {
        return firstReference == other.firstReference && referenceCount == other.referenceCount;
    }
};

/** @brief Geographic vertices plus projection-specific fill and border indices. */
struct MapMesh final {
    // Interleaved longitude, latitude, and projected Mercator Y.
    std::vector<float> vertices;
    std::vector<std::uint32_t> mercatorFillIndices;
    std::vector<std::uint32_t> sphereFillIndices;
    std::vector<std::uint32_t> borderIndices;
    std::vector<MapLandCellRange> landCellRanges;
    // Entries are triangle ordinals into mercatorFillIndices, not vertex indices.
    std::vector<std::uint32_t> landTriangleReferences;

    /**
     * @brief Reports whether the mesh has no renderable projection data.
     *
     * @return True when vertices are absent or both projected fill-index sets are empty.
     */
    [[nodiscard]] bool empty() const noexcept {
        return vertices.empty() || (mercatorFillIndices.empty() && sphereFillIndices.empty());
    }

    [[nodiscard]] std::size_t vertexCount() const noexcept {
        return vertices.size() / 3U;
    }

    [[nodiscard]] std::size_t byteSize() const noexcept {
        return vertices.size() * sizeof(float) +
               (mercatorFillIndices.size() + sphereFillIndices.size() + borderIndices.size()) *
                   sizeof(std::uint32_t) +
               landCellRanges.size() * 2U * sizeof(std::uint32_t) +
               landTriangleReferences.size() * sizeof(std::uint32_t);
    }

    /** @brief Reports whether the optional geographic land index is present. */
    [[nodiscard]] bool hasLandIndex() const noexcept {
        return landCellRanges.size() == MapLandCellCount;
    }
};

} // namespace lar::map
