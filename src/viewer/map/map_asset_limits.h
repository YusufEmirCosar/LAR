#pragma once

/**
 * @file map_asset_limits.h
 * @brief Hard resource and coordinate limits for untrusted map assets.
 */

#include <QtGlobal>

#include "viewer/map/map_asset_format.h"

#include <cstddef>

namespace lar::map::limits {

constexpr qint64 MaximumAssetBytes = 64LL * 1024LL * 1024LL;
constexpr std::size_t MaximumPayloadBytes =
    static_cast<std::size_t>(MaximumAssetBytes) - format::HeaderSize;
constexpr std::size_t MaximumVertexCount = 2'000'000ULL;
constexpr std::size_t MaximumMercatorIndexCount = 6'000'000ULL;
constexpr std::size_t MaximumSphereIndexCount = 6'000'000ULL;
constexpr std::size_t MaximumBorderIndexCount = 4'000'000ULL;

constexpr double MaximumAbsoluteLongitude = 540.0;
constexpr double MaximumAbsoluteLatitude = 90.0;
constexpr double MaximumAbsoluteMercatorY = 180.001;

} // namespace lar::map::limits
