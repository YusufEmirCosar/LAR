#pragma once

/**
 * @file map_source_limits.h
 * @brief Resource ceilings applied while parsing untrusted GeoJSON.
 */

#include <cstddef>

namespace lar::map::tool::source_limits {

inline constexpr std::size_t MaximumSourceBytes = 64U * 1024U * 1024U;
inline constexpr std::size_t MaximumFeatureCount = 2000U;
inline constexpr std::size_t MaximumPolygonCount = 20000U;
inline constexpr std::size_t MaximumRingCount = 50000U;
inline constexpr std::size_t MaximumCoordinatesPerRing = 1000000U;
inline constexpr std::size_t MaximumCoordinateCount = 1500000U;

} // namespace lar::map::tool::source_limits
