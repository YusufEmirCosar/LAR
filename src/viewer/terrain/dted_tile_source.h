#pragma once

/**
 * @file dted_tile_source.h
 * @brief Deterministic DTED0 tile addressing without scanning the source tree.
 */

#include "viewer/terrain/dted_cell_reader.h"

#include <QString>

#include <optional>

/** @brief Resolves WGS84 positions directly to conventional eDDD/nDD.dt0 paths. */
class DtedTileSource final {
  public:
    explicit DtedTileSource(QString rootDirectory = {});

    [[nodiscard]] const QString &rootDirectory() const noexcept {
        return m_rootDirectory;
    }
    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] QString pathFor(const DtedCellKey &key) const;
    [[nodiscard]] DtedCellReadResult load(const DtedCellKey &key) const;

    [[nodiscard]] static std::optional<DtedCellKey> keyForRadians(double latitudeRadians,
                                                                  double longitudeRadians) noexcept;

  private:
    QString m_rootDirectory;
};
