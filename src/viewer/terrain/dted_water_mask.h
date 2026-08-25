#pragma once

/**
 * @file dted_water_mask.h
 * @brief Immutable DTED0-post-aligned water classification for one degree cell.
 */

#include "viewer/terrain/dted_cell.h"

#include <QString>
#include <QtGlobal>

#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

/** @brief Storage form selected by the water-mask compiler for one DTED cell. */
enum class DtedWaterCoverage : quint8 {
    Missing = 0,
    Land = 1,
    Water = 2,
    Mixed = 3,
};

/** @brief Water flags ordered as DTED longitude profiles from south to north. */
struct DtedWaterMaskCell final {
    DtedCellKey key;
    int longitudeSampleCount = 0;
    int latitudeSampleCount = 0;
    DtedWaterCoverage coverage = DtedWaterCoverage::Missing;
    std::vector<quint8> packedWaterBits;

    /** @brief Returns whether dimensions, coverage, and optional mixed payload agree. */
    [[nodiscard]] bool valid() const noexcept {
        if (longitudeSampleCount < 2 || longitudeSampleCount > 121 || latitudeSampleCount < 2 ||
            latitudeSampleCount > 121 || coverage == DtedWaterCoverage::Missing) {
            return false;
        }
        const std::size_t sampleCount = static_cast<std::size_t>(longitudeSampleCount) *
                                        static_cast<std::size_t>(latitudeSampleCount);
        const std::size_t expectedBytes = (sampleCount + 7U) / 8U;
        return coverage == DtedWaterCoverage::Mixed ? packedWaterBits.size() == expectedBytes
                                                    : packedWaterBits.empty();
    }

    /** @brief Returns a post classification, or no value for invalid coordinates/data. */
    [[nodiscard]] std::optional<bool> water(int longitudeIndex, int latitudeIndex) const noexcept {
        if (!valid() || longitudeIndex < 0 || latitudeIndex < 0 ||
            longitudeIndex >= longitudeSampleCount || latitudeIndex >= latitudeSampleCount) {
            return std::nullopt;
        }
        if (coverage == DtedWaterCoverage::Land) {
            return false;
        }
        if (coverage == DtedWaterCoverage::Water) {
            return true;
        }
        const std::size_t bitIndex = static_cast<std::size_t>(longitudeIndex) *
                                         static_cast<std::size_t>(latitudeSampleCount) +
                                     static_cast<std::size_t>(latitudeIndex);
        const quint8 bit = static_cast<quint8>(1U << (bitIndex % 8U));
        return (packedWaterBits[bitIndex / 8U] & bit) != 0U;
    }

    /** @brief Returns the nearest mask-post classification for normalized cell coordinates. */
    [[nodiscard]] std::optional<bool> waterAtFraction(double longitudeFraction,
                                                      double latitudeFraction) const noexcept {
        if (!valid() || !std::isfinite(longitudeFraction) || !std::isfinite(latitudeFraction) ||
            longitudeFraction < 0.0 || longitudeFraction > 1.0 || latitudeFraction < 0.0 ||
            latitudeFraction > 1.0) {
            return std::nullopt;
        }
        const int longitudeIndex = static_cast<int>(
            std::round(longitudeFraction * static_cast<double>(longitudeSampleCount - 1)));
        const int latitudeIndex = static_cast<int>(
            std::round(latitudeFraction * static_cast<double>(latitudeSampleCount - 1)));
        return water(longitudeIndex, latitudeIndex);
    }
};

/** @brief Value-or-diagnostic result for one bounded mask-cell load. */
struct DtedWaterMaskReadResult final {
    std::shared_ptr<const DtedWaterMaskCell> cell;
    QString message;

    [[nodiscard]] bool succeeded() const noexcept {
        return cell != nullptr;
    }
};
