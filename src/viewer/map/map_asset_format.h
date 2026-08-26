#pragma once

/**
 * @file map_asset_format.h
 * @brief Versioned binary .larmap header layout and field offsets.
 */

#include <array>
#include <cstddef>
#include <cstdint>

namespace lar::map::format {

/**
 * @namespace lar::map::format
 * @brief Binary LRM1 header and payload layout.
 *
 * Every scalar is little-endian. The 64-byte header contains, in order: the
 * four-byte magic, 32-bit version, header size, and flags; 64-bit vertex,
 * Mercator-index, sphere-index, border-index, and payload-byte counts; the
 * 32-bit IEEE CRC-32 of the payload; and a reserved zero 32-bit field.
 *
 * The payload is tightly concatenated arrays: 32-bit float vertex triplets
 * `(longitude degrees, latitude degrees, projected Mercator y degrees)`, then
 * 32-bit unsigned Mercator fill indices, sphere fill indices, and line-pair
 * border indices. Count fields describe vertices or indices, not bytes.
 */

/** Four-byte file signature. */
constexpr std::array<char, 4> Magic{{'L', 'R', 'M', '1'}};
/** Current and only accepted format version. */
constexpr std::uint32_t Version = 1U;
/** Fixed header length in bytes. */
constexpr std::uint32_t HeaderSize = 64U;
/** Version 1 defines no optional flag bits. */
constexpr std::uint32_t SupportedFlags = 0U;

constexpr std::size_t MagicOffset = 0U;
constexpr std::size_t VersionOffset = 4U;
constexpr std::size_t HeaderSizeOffset = 8U;
constexpr std::size_t FlagsOffset = 12U;
constexpr std::size_t VertexCountOffset = 16U;
constexpr std::size_t MercatorIndexCountOffset = 24U;
constexpr std::size_t SphereIndexCountOffset = 32U;
constexpr std::size_t BorderIndexCountOffset = 40U;
constexpr std::size_t PayloadSizeOffset = 48U;
constexpr std::size_t PayloadCrcOffset = 56U;
constexpr std::size_t ReservedOffset = 60U;

} // namespace lar::map::format
