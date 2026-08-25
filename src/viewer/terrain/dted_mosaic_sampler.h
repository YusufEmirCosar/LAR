#pragma once

/**
 * @file dted_mosaic_sampler.h
 * @brief Bilinear DTED0 surface sampler with bounded terrain and water-mask caches.
 */

#include "viewer/terrain/dted_tile_source.h"
#include "viewer/terrain/dted_water_mask_source.h"

#include <QString>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>

/** @brief Render-ready local surface elevation and optional bathymetric classification. */
struct DtedSurfaceSample final {
    double elevationMeters = 0.0;
    double waterDepthMeters = 0.0;
    bool water = false;
};

/** @brief Samples a seamless logical terrain mosaic while loading only addressed tiles. */
class DtedMosaicSampler final {
  public:
    explicit DtedMosaicSampler(DtedTileSource source, std::size_t cacheCapacity = 24U);
    DtedMosaicSampler(DtedTileSource source, DtedWaterMaskSource waterMaskSource,
                      std::size_t cacheCapacity = 24U);

    [[nodiscard]] std::optional<double> sampleRadians(double latitudeRadians,
                                                      double longitudeRadians);
    /** @brief Samples a sea-level water surface while retaining source depth for coloring. */
    [[nodiscard]] std::optional<DtedSurfaceSample> sampleSurfaceRadians(double latitudeRadians,
                                                                        double longitudeRadians);
    [[nodiscard]] const QString &lastError() const noexcept {
        return m_lastError;
    }
    [[nodiscard]] std::size_t cachedTileCount() const noexcept {
        return m_cache.size();
    }
    [[nodiscard]] std::size_t cachedWaterMaskTileCount() const noexcept {
        return m_waterMaskCache.size();
    }
    [[nodiscard]] bool waterMaskAvailable() const noexcept {
        return m_waterMaskSource.isAvailable();
    }

  private:
    struct CacheEntry final {
        std::shared_ptr<const DtedCell> cell;
        quint64 access = 0;
    };

    struct WaterMaskCacheEntry final {
        std::shared_ptr<const DtedWaterMaskCell> cell;
        quint64 access = 0;
    };

    struct SamplePosition final {
        DtedCellKey key;
        std::shared_ptr<const DtedCell> cell;
        int longitude0 = 0;
        int longitude1 = 0;
        int latitude0 = 0;
        int latitude1 = 0;
        double longitudeFraction = 0.0;
        double latitudeFraction = 0.0;
    };

    [[nodiscard]] std::shared_ptr<const DtedCell> cellFor(const DtedCellKey &key);
    [[nodiscard]] std::shared_ptr<const DtedWaterMaskCell>
    waterMaskFor(const DtedCellKey &key, int longitudeSampleCount, int latitudeSampleCount);
    [[nodiscard]] std::optional<SamplePosition> positionFor(double latitudeRadians,
                                                            double longitudeRadians);
    [[nodiscard]] static std::optional<double>
    interpolatedElevation(const SamplePosition &position, const DtedWaterMaskCell *mask = nullptr,
                          std::optional<bool> water = std::nullopt);
    void evictIfNeeded();

    DtedTileSource m_source;
    DtedWaterMaskSource m_waterMaskSource;
    std::size_t m_cacheCapacity = 24U;
    std::map<DtedCellKey, CacheEntry> m_cache;
    std::map<DtedCellKey, WaterMaskCacheEntry> m_waterMaskCache;
    quint64 m_accessCounter = 0;
    QString m_lastError;
};
