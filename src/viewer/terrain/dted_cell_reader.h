#pragma once

/**
 * @file dted_cell_reader.h
 * @brief Bounded parser for DTED UHL, profile, elevation, and checksum records.
 */

#include "viewer/terrain/dted_cell.h"

#include <QString>

#include <memory>

/** @brief Result of parsing one DTED file without retaining its source bytes. */
struct DtedCellReadResult final {
    std::shared_ptr<const DtedCell> cell;
    QString message;

    [[nodiscard]] bool succeeded() const noexcept {
        return cell != nullptr;
    }
};

/** @brief Reads one strictly bounded DTED tile using no external terrain runtime. */
class DtedCellReader final {
  public:
    /** @brief Parses a DTED file and verifies level, dimensions, and profile checksums. */
    [[nodiscard]] static DtedCellReadResult readFile(const QString &path,
                                                     DtedLevel expectedLevel = DtedLevel::Level0);
};
