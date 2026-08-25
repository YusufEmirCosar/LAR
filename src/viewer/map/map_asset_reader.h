#pragma once

/**
 * @file map_asset_reader.h
 * @brief Bounded, integrity-checking parser for packaged .larmap bytes.
 */

#include "viewer/map/map_mesh.h"

#include <QByteArrayView>
#include <QString>

#include <memory>

namespace lar::map {

/** @brief Stable failure categories returned by MapAssetReader. */
enum class MapAssetError { None, Io, Size, Integrity, Format, Limits, Allocation };

/** @brief Parsed immutable mesh or a categorized diagnostic. */
struct MapAssetReadResult final {
    std::shared_ptr<const MapMesh> mesh;
    MapAssetError error = MapAssetError::None;
    QString message;

    /**
     * @brief Reports whether parsing produced a valid mesh.
     *
     * @return True when a mesh is present and no read error was recorded.
     */
    [[nodiscard]] bool succeeded() const noexcept {
        return mesh != nullptr && error == MapAssetError::None;
    }
};

/** @brief Validates header, CRC, sizes, indices, and coordinate ranges. */
class MapAssetReader final {
  public:
    static MapAssetReadResult read(QByteArrayView bytes);
};

} // namespace lar::map
