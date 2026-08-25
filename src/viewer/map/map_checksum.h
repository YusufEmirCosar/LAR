#pragma once

/**
 * @file map_checksum.h
 * @brief Dependency-free CRC-32 implementation for map asset integrity.
 */

#include <array>
#include <cstddef>
#include <cstdint>

namespace lar::map {

/** @brief Computes the IEEE CRC-32 used by the .larmap format. */
class MapChecksum final {
  public:
    static std::uint32_t crc32(const unsigned char *data, std::size_t length) noexcept {
        static constexpr std::array<std::uint32_t, 256> Table = [] {
            std::array<std::uint32_t, 256> values{};
            for (std::size_t index = 0; index < values.size(); ++index) {
                std::uint32_t entry = static_cast<std::uint32_t>(index);
                for (int bit = 0; bit < 8; ++bit) {
                    entry = (entry >> 1U) ^ (0xedb88320U & (0U - (entry & 1U)));
                }
                values[index] = entry;
            }
            return values;
        }();

        std::uint32_t crc = 0xffffffffU;
        for (std::size_t index = 0; index < length; ++index) {
            crc = (crc >> 8U) ^ Table[(crc ^ data[index]) & 0xffU];
        }
        return ~crc;
    }
};

} // namespace lar::map
