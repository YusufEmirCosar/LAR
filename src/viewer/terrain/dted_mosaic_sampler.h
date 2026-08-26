#pragma once

/**
 * @file dted_mosaic_sampler.h
 * @brief Bilinear DTED surface sampler with bounded terrain and water-mask caches.
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

/**
 * @brief Samples a logical DTED mosaic while loading only addressed tiles.
 *
 * Inputs are WGS84 latitude and longitude in radians; elevations and water
 * depths are metres. Bilinear interpolation ignores DTED void posts and
 * renormalizes the remaining weights. With a water mask, surface sampling first
 * interpolates posts matching the classification at the requested coordinate;
 * if none contribute, it falls back to all valid terrain posts. Water is
 * rendered at zero elevation while a negative source elevation is retained as
 * non-negative depth.
 *
 * Terrain cells are evicted least-recently-used when either the configured tile
 * count or byte budget is exceeded; the currently addressed tile is retained
 * when it alone exceeds the byte budget. Water-mask cells use the same access
 * order and tile-count budget.
 */
class DtedMosaicSampler final {
  public:
    static constexpr std::size_t DefaultCacheByteCapacity = 128U * 1024U * 1024U;

    explicit DtedMosaicSampler(DtedTileSource source, std::size_t cacheCapacity = 24U,
                               std::size_t cacheByteCapacity = DefaultCacheByteCapacity);
    DtedMosaicSampler(DtedTileSource source, DtedWaterMaskSource waterMaskSource,
                      std::size_t cacheCapacity = 24U,
                      std::size_t cacheByteCapacity = DefaultCacheByteCapacity);

    /** Returns interpolated terrain elevation, or no value when no valid post contributes. */
    [[nodiscard]] std::optional<double> sampleRadians(double latitudeRadians,
                                                      double longitudeRadians);
    /** Samples a sea-level water surface while retaining source depth for coloring. */
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
    [[nodiscard]] std::size_t cachedTerrainBytes() const noexcept {
        return m_cachedTerrainBytes;
    }
    [[nodiscard]] bool waterMaskAvailable() const noexcept {
        return m_waterMaskSource.isAvailable();
    }

  private:
    struct CacheEntry final {
        std::shared_ptr<const DtedCell> cell;
        quint64 access = 0;
        std::size_t bytes = 0U;
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
    waterMaskFor(const DtedCellKey &key, const DtedCell &terrainCell);
    [[nodiscard]] std::optional<SamplePosition> positionFor(double latitudeRadians,
                                                            double longitudeRadians);
    [[nodiscard]] static std::optional<double>
    interpolatedElevation(const SamplePosition &position, const DtedWaterMaskCell *mask = nullptr,
                          std::optional<bool> water = std::nullopt);
    void evictIfNeeded();

    DtedTileSource m_source;
    DtedWaterMaskSource m_waterMaskSource;
    std::size_t m_cacheCapacity = 24U;
    std::size_t m_cacheByteCapacity = DefaultCacheByteCapacity;
    std::size_t m_cachedTerrainBytes = 0U;
    std::map<DtedCellKey, CacheEntry> m_cache;
    std::map<DtedCellKey, WaterMaskCacheEntry> m_waterMaskCache;
    quint64 m_accessCounter = 0;
    QString m_lastError;
};
