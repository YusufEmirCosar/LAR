#pragma once

/**
 * @file dted_water_mask_source.h
 * @brief Validated indexed access to the compact global DTED0 water-mask pack.
 */

#include "viewer/terrain/dted_water_mask.h"

#include <QString>

#include <cstddef>
#include <vector>

/** @brief Opens one mask index and reads mixed-cell payloads only when addressed. */
class DtedWaterMaskSource final {
  public:
    explicit DtedWaterMaskSource(QString packPath = {});

    [[nodiscard]] const QString &packPath() const noexcept {
        return m_packPath;
    }
    [[nodiscard]] bool isAvailable() const noexcept {
        return !m_entries.empty();
    }
    [[nodiscard]] const QString &initializationError() const noexcept {
        return m_initializationError;
    }

    /** @brief Loads and verifies one mask cell in the pack's native DTED0-aligned grid. */
    [[nodiscard]] DtedWaterMaskReadResult load(const DtedCellKey &key) const;

  private:
    struct IndexEntry final {
        DtedWaterCoverage coverage = DtedWaterCoverage::Missing;
        quint8 longitudeSampleCount = 0;
        quint8 latitudeSampleCount = 0;
        quint32 payloadOffset = 0;
        quint32 payloadBytes = 0;
        quint32 checksum = 0;
    };

    void initialize();
    [[nodiscard]] static std::size_t indexFor(const DtedCellKey &key) noexcept;

    QString m_packPath;
    QString m_initializationError;
    std::vector<IndexEntry> m_entries;
};
