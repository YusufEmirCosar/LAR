#pragma once

/**
 * @file dted_tile_source.h
 * @brief Deterministic DTED tile addressing without scanning the source tree.
 */

#include "viewer/terrain/dted_cell_reader.h"

#include <QString>

#include <optional>

/** @brief Resolves WGS84 positions directly to conventional eDDD/nDD.dtN paths. */
class DtedTileSource final {
  public:
    explicit DtedTileSource(QString rootDirectory = {}, DtedLevel level = DtedLevel::Level0);

    [[nodiscard]] const QString &rootDirectory() const noexcept {
        return m_rootDirectory;
    }
    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] DtedLevel level() const noexcept {
        return m_level;
    }
    [[nodiscard]] QString pathFor(const DtedCellKey &key) const;
    [[nodiscard]] DtedCellReadResult load(const DtedCellKey &key) const;

    [[nodiscard]] static std::optional<DtedCellKey> keyForRadians(double latitudeRadians,
                                                                  double longitudeRadians) noexcept;

  private:
    [[nodiscard]] bool containsFile(const QString &path) const;

    QString m_rootDirectory;
    QString m_canonicalRootDirectory;
    DtedLevel m_level = DtedLevel::Level0;
};
