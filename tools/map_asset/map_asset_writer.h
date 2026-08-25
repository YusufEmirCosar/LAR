#pragma once

/**
 * @file map_asset_writer.h
 * @brief Serializer for binary map assets and JSON manifests.
 */

#include "viewer/map/map_mesh.h"

#include <QString>

namespace lar::map::tool {

/** @brief Writes versioned little-endian mesh bytes with CRC metadata. */
class MapAssetWriter final {
  public:
    static bool write(const QString &assetPath, const QString &manifestPath, const MapMesh &mesh,
                      QString *errorMessage = nullptr);
};

} // namespace lar::map::tool
