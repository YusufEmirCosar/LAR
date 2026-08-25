#pragma once

/**
 * @file dted_cell.h
 * @brief Immutable in-memory representation of one degree-aligned DTED cell.
 */

#include "viewer/terrain/dted_level.h"

#include <QtGlobal>

#include <cstddef>
#include <optional>
#include <vector>

/** @brief Integer south-west origin used to address one DTED cell. */
struct DtedCellKey final {
    int longitudeDegrees = 0;
    int latitudeDegrees = 0;

    friend bool operator==(const DtedCellKey &left, const DtedCellKey &right) noexcept {
        return left.longitudeDegrees == right.longitudeDegrees &&
               left.latitudeDegrees == right.latitudeDegrees;
    }

    friend bool operator<(const DtedCellKey &left, const DtedCellKey &right) noexcept {
        return left.longitudeDegrees < right.longitudeDegrees ||
               (left.longitudeDegrees == right.longitudeDegrees &&
                left.latitudeDegrees < right.latitudeDegrees);
    }
};

/** @brief Decoded DTED elevations stored as longitude profiles from south to north. */
struct DtedCell final {
    static constexpr qint16 NoDataElevation = -32767;

    DtedCellKey key;
    DtedLevel level = DtedLevel::Level0;
    int longitudeSampleCount = 0;
    int latitudeSampleCount = 0;
    double longitudeIntervalDegrees = 0.0;
    double latitudeIntervalDegrees = 0.0;
    std::vector<qint16> elevations;

    /** @brief Returns true when dimensions and storage form a bounded usable DTED cell. */
    [[nodiscard]] bool valid() const noexcept {
        const int maximumSamples = dtedLatitudeSampleCount(level);
        if (longitudeSampleCount < 2 || latitudeSampleCount < 2 ||
            longitudeIntervalDegrees <= 0.0 || latitudeIntervalDegrees <= 0.0 ||
            maximumSamples <= 0) {
            return false;
        }
        const std::size_t longitudeCount = static_cast<std::size_t>(longitudeSampleCount);
        const std::size_t latitudeCount = static_cast<std::size_t>(latitudeSampleCount);
        return longitudeCount <= static_cast<std::size_t>(maximumSamples) &&
               latitudeCount == static_cast<std::size_t>(maximumSamples) &&
               elevations.size() == longitudeCount * latitudeCount;
    }

    [[nodiscard]] std::size_t storageBytes() const noexcept {
        return elevations.size() * sizeof(qint16);
    }

    /** @brief Returns one decoded elevation in metres, excluding the DTED no-data sentinel. */
    [[nodiscard]] std::optional<double> elevation(int longitudeIndex,
                                                  int latitudeIndex) const noexcept {
        if (!valid() || longitudeIndex < 0 || latitudeIndex < 0 ||
            longitudeIndex >= longitudeSampleCount || latitudeIndex >= latitudeSampleCount) {
            return std::nullopt;
        }
        const std::size_t offset = static_cast<std::size_t>(longitudeIndex) *
                                       static_cast<std::size_t>(latitudeSampleCount) +
                                   static_cast<std::size_t>(latitudeIndex);
        const qint16 value = elevations[offset];
        return value == NoDataElevation ? std::nullopt
                                        : std::optional<double>(static_cast<double>(value));
    }
};
