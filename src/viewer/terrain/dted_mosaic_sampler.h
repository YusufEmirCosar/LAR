#pragma once

/**
 * @file dted_mosaic_sampler.h
 * @brief Bilinear DTED elevation sampler with a bounded terrain cache.
 */

#include "viewer/terrain/dted_tile_source.h"

#include <QString>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>

/**
 * @brief Samples a logical DTED mosaic while loading only addressed tiles.
 *
 * Inputs are WGS84 latitude and longitude in radians; elevations are metres.
 * Bilinear interpolation ignores DTED void posts and renormalizes the remaining
 * weights. The returned elevation remains signed; a terrain consumer may convert
 * negative water elevation into non-negative depth only after applying the shared
 * vector land classification. Classification deliberately remains outside this
 * class so all viewports can use one coastline source.
 *
 * Terrain cells are evicted least-recently-used when either the configured tile
 * count or byte budget is exceeded; the currently addressed tile is retained
 * when it alone exceeds the byte budget.
 */
class DtedMosaicSampler final {
  public:
    static constexpr std::size_t DefaultCacheByteCapacity = 128U * 1024U * 1024U;

    explicit DtedMosaicSampler(DtedTileSource source, std::size_t cacheCapacity = 24U,
                               std::size_t cacheByteCapacity = DefaultCacheByteCapacity);

    /** Returns interpolated terrain elevation, or no value when no valid post contributes. */
    [[nodiscard]] std::optional<double> sampleRadians(double latitudeRadians,
                                                      double longitudeRadians);
    [[nodiscard]] const QString &lastError() const noexcept {
        return m_lastError;
    }
    [[nodiscard]] std::size_t cachedTileCount() const noexcept {
        return m_cache.size();
    }
    [[nodiscard]] std::size_t cachedTerrainBytes() const noexcept {
        return m_cachedTerrainBytes;
    }

  private:
    struct CacheEntry final {
        std::shared_ptr<const DtedCell> cell;
        quint64 access = 0;
        std::size_t bytes = 0U;
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
    [[nodiscard]] std::optional<SamplePosition> positionFor(double latitudeRadians,
                                                            double longitudeRadians);
    [[nodiscard]] static std::optional<double>
    interpolatedElevation(const SamplePosition &position);
    void evictIfNeeded();

    DtedTileSource m_source;
    std::size_t m_cacheCapacity = 24U;
    std::size_t m_cacheByteCapacity = DefaultCacheByteCapacity;
    std::size_t m_cachedTerrainBytes = 0U;
    std::map<DtedCellKey, CacheEntry> m_cache;
    quint64 m_accessCounter = 0;
    QString m_lastError;
};
