#pragma once

/**
 * @file map_asset_source.h
 * @brief Source port for loading an immutable compiled world-map mesh.
 */

#include "viewer/map/map_asset_reader.h"

namespace lar::map {

/** @brief Hides package/file lookup from EarthLarView. */
class IMapAssetSource {
  public:
    virtual ~IMapAssetSource() = default;
    [[nodiscard]] virtual MapAssetReadResult load() const = 0;
};

} // namespace lar::map
