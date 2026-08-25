#pragma once

/**
 * @file plane_terrain_patch_builder.h
 * @brief Bounded DTED sampling and indexed local terrain-mesh construction.
 */

#include "viewer/plane/plane_terrain_patch.h"
#include "viewer/terrain/dted_mosaic_sampler.h"

#include <functional>

/** @brief Builds immutable terrain patches; instances are confined to one CPU worker thread. */
class PlaneTerrainPatchBuilder final {
  public:
    explicit PlaneTerrainPatchBuilder(QString dtedRootDirectory, QString waterMaskPackPath = {});
    PlaneTerrainPatchBuilder(DtedDataset dataset, QString waterMaskPackPath = {});

    /** @brief Samples and triangulates one request, returning null when local terrain is absent. */
    [[nodiscard]] PlaneTerrainPatchPtr build(const PlaneTerrainBuildRequest &request,
                                             QString *errorMessage = nullptr,
                                             const std::function<bool()> &cancelled = {});

    [[nodiscard]] std::size_t cachedTileCount() const noexcept {
        return m_sampler.cachedTileCount();
    }

  private:
    DtedMosaicSampler m_sampler;
    DtedLevel m_level = DtedLevel::Level0;
};
