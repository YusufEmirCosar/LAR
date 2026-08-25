#pragma once

/**
 * @file map_mesh.h
 * @brief CPU-side vertex/index representation shared by tools and renderer.
 */

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lar::map {

/** @brief Geographic vertices plus projection-specific fill and border indices. */
struct MapMesh final {
    // Interleaved longitude, latitude, and projected Mercator Y.
    std::vector<float> vertices;
    std::vector<std::uint32_t> mercatorFillIndices;
    std::vector<std::uint32_t> sphereFillIndices;
    std::vector<std::uint32_t> borderIndices;

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
                   sizeof(std::uint32_t);
    }
};

} // namespace lar::map
