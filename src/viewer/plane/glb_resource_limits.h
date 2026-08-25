#pragma once

/**
 * @file glb_resource_limits.h
 * @brief Aggregate CPU-memory limits for user-selected glTF/GLB models.
 */

#include <QtGlobal>

namespace lar::gltf::limits {

inline constexpr qint64 MaximumModelBytes = 32LL * 1024LL * 1024LL;
inline constexpr qint64 MaximumResourceBytes = 32LL * 1024LL * 1024LL;
inline constexpr qint64 MaximumAggregateBufferBytes = 64LL * 1024LL * 1024LL;
inline constexpr qint64 MaximumAggregateEncodedImageBytes = 32LL * 1024LL * 1024LL;
inline constexpr qint64 MaximumAggregateDecodedImageBytes = 64LL * 1024LL * 1024LL;
inline constexpr qsizetype MaximumBufferCount = 32;
inline constexpr qsizetype MaximumTextureCount = 32;
inline constexpr int MaximumTextureDimension = 8192;

} // namespace lar::gltf::limits
