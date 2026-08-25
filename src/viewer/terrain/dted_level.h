#pragma once

/**
 * @file dted_level.h
 * @brief Shared DTED level metadata and external terrain-source configuration.
 */

#include <QMetaType>
#include <QString>
#include <QtGlobal>

/** @brief Supported Digital Terrain Elevation Data resolutions. */
enum class DtedLevel : quint8 {
    Level0 = 0,
    Level1 = 1,
    Level2 = 2,
};

/** @brief One lazily addressed DTED directory selected for Plane terrain. */
struct DtedDataset final {
    QString rootDirectory;
    DtedLevel level = DtedLevel::Level0;
};

[[nodiscard]] inline constexpr int dtedLevelNumber(DtedLevel level) noexcept {
    return static_cast<int>(level);
}

[[nodiscard]] inline constexpr int dtedLatitudeSampleCount(DtedLevel level) noexcept {
    switch (level) {
    case DtedLevel::Level0:
        return 121;
    case DtedLevel::Level1:
        return 1201;
    case DtedLevel::Level2:
        return 3601;
    }
    return 0;
}

[[nodiscard]] inline constexpr int dtedLatitudeIntervalTenthsArcSecond(DtedLevel level) noexcept {
    switch (level) {
    case DtedLevel::Level0:
        return 300;
    case DtedLevel::Level1:
        return 30;
    case DtedLevel::Level2:
        return 10;
    }
    return 0;
}

[[nodiscard]] inline constexpr qint64 dtedMaximumFileBytes(DtedLevel level) noexcept {
    constexpr qint64 HeaderBytes = 80 + 648 + 2700;
    const qint64 samples = dtedLatitudeSampleCount(level);
    return samples > 0 ? HeaderBytes + samples * (8 + samples * 2 + 4) : 0;
}

[[nodiscard]] inline constexpr double dtedNominalPostSpacingMeters(DtedLevel level) noexcept {
    switch (level) {
    case DtedLevel::Level0:
        return 900.0;
    case DtedLevel::Level1:
        return 90.0;
    case DtedLevel::Level2:
        return 30.0;
    }
    return 900.0;
}

[[nodiscard]] inline QString dtedFileSuffix(DtedLevel level) {
    return QStringLiteral(".dt%1").arg(dtedLevelNumber(level));
}

[[nodiscard]] inline QString dtedLevelDisplayName(DtedLevel level) {
    return QStringLiteral("DTED Level %1").arg(dtedLevelNumber(level));
}

Q_DECLARE_METATYPE(DtedLevel)
