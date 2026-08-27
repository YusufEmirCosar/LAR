#pragma once

/**
 * @file packaged_map_asset_source.h
 * @brief Loader for the compiled map and manifest beside an executable.
 */

#include "viewer/map/map_asset_source.h"

#include <QString>

#include <mutex>

namespace lar::map {

/** @brief Resolves package paths, validates the manifest, and reads the mesh. */
class PackagedMapAssetSource final : public IMapAssetSource {
  public:
    explicit PackagedMapAssetSource(QString packageDirectory);

    [[nodiscard]] MapAssetReadResult load() const override;

    [[nodiscard]] QString assetPath() const;
    [[nodiscard]] QString manifestPath() const;

  private:
    [[nodiscard]] MapAssetReadResult loadUncached() const;

    QString m_packageDirectory;
    mutable std::once_flag m_loadOnce;
    mutable MapAssetReadResult m_cachedResult;
};

} // namespace lar::map
