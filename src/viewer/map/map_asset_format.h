#pragma once

/**
 * @file map_asset_format.h
 * @brief Versioned binary .larmap header layout and field offsets.
 */

#include <array>
#include <cstddef>
#include <cstdint>

namespace lar::map::format {

constexpr std::array<char, 4> Magic{{'L', 'R', 'M', '1'}};
constexpr std::uint32_t Version = 1U;
constexpr std::uint32_t HeaderSize = 64U;
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
