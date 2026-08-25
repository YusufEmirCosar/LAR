#include "viewer/terrain/dted_mosaic_sampler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr double Pi = 3.14159265358979323846;

double normalizedLongitudeDegrees(double radians) noexcept {
    double degrees = radians * 180.0 / Pi;
    degrees = std::fmod(degrees + 180.0, 360.0);
    if (degrees < 0.0) {
        degrees += 360.0;
    }
    return degrees - 180.0;
}
} // namespace

DtedMosaicSampler::DtedMosaicSampler(DtedTileSource source, std::size_t cacheCapacity,
                                     std::size_t cacheByteCapacity)
    : m_source(std::move(source)), m_cacheCapacity(std::max<std::size_t>(1U, cacheCapacity)),
      m_cacheByteCapacity(std::max<std::size_t>(1U, cacheByteCapacity)) {}

DtedMosaicSampler::DtedMosaicSampler(DtedTileSource source, DtedWaterMaskSource waterMaskSource,
                                     std::size_t cacheCapacity, std::size_t cacheByteCapacity)
    : m_source(std::move(source)), m_waterMaskSource(std::move(waterMaskSource)),
      m_cacheCapacity(std::max<std::size_t>(1U, cacheCapacity)),
      m_cacheByteCapacity(std::max<std::size_t>(1U, cacheByteCapacity)) {}

std::shared_ptr<const DtedCell> DtedMosaicSampler::cellFor(const DtedCellKey &key) {
    ++m_accessCounter;
    const auto found = m_cache.find(key);
    if (found != m_cache.end()) {
        found->second.access = m_accessCounter;
        return found->second.cell;
    }
    DtedCellReadResult result = m_source.load(key);
    if (!result.succeeded()) {
        m_lastError = result.message;
    }
    const std::shared_ptr<const DtedCell> cell = result.cell;
    const std::size_t bytes = cell != nullptr ? cell->storageBytes() : 0U;
    m_cachedTerrainBytes += bytes;
    m_cache.emplace(key, CacheEntry{cell, m_accessCounter, bytes});
    evictIfNeeded();
    return cell;
}

std::shared_ptr<const DtedWaterMaskCell>
DtedMosaicSampler::waterMaskFor(const DtedCellKey &key, const DtedCell &terrainCell) {
    if (!m_waterMaskSource.isAvailable()) {
        return nullptr;
    }
    ++m_accessCounter;
    const auto found = m_waterMaskCache.find(key);
    if (found != m_waterMaskCache.end()) {
        found->second.access = m_accessCounter;
        return found->second.cell;
    }
    DtedWaterMaskReadResult result = m_waterMaskSource.load(key);
    std::shared_ptr<const DtedWaterMaskCell> cell = result.cell;
    if (m_source.level() == DtedLevel::Level0 && cell != nullptr &&
        (cell->longitudeSampleCount != terrainCell.longitudeSampleCount ||
         cell->latitudeSampleCount != terrainCell.latitudeSampleCount)) {
        cell.reset();
    }
    m_waterMaskCache.emplace(key, WaterMaskCacheEntry{cell, m_accessCounter});
    evictIfNeeded();
    return cell;
}

void DtedMosaicSampler::evictIfNeeded() {
    while (m_cache.size() > m_cacheCapacity ||
           (m_cachedTerrainBytes > m_cacheByteCapacity && m_cache.size() > 1U)) {
        auto oldest = m_cache.begin();
        for (auto iterator = std::next(m_cache.begin()); iterator != m_cache.end(); ++iterator) {
            if (iterator->second.access < oldest->second.access) {
                oldest = iterator;
            }
        }
        m_cachedTerrainBytes -= oldest->second.bytes;
        m_cache.erase(oldest);
    }
    while (m_waterMaskCache.size() > m_cacheCapacity) {
        auto oldest = m_waterMaskCache.begin();
        for (auto iterator = std::next(m_waterMaskCache.begin());
             iterator != m_waterMaskCache.end(); ++iterator) {
            if (iterator->second.access < oldest->second.access) {
                oldest = iterator;
            }
        }
        m_waterMaskCache.erase(oldest);
    }
}

std::optional<DtedMosaicSampler::SamplePosition>
DtedMosaicSampler::positionFor(double latitudeRadians, double longitudeRadians) {
    const std::optional<DtedCellKey> key =
        DtedTileSource::keyForRadians(latitudeRadians, longitudeRadians);
    if (!key) {
        m_lastError = QStringLiteral("Terrain coordinate is outside the WGS84 DTED range.");
        return {};
    }
    const std::shared_ptr<const DtedCell> cell = cellFor(*key);
    if (cell == nullptr || !cell->valid()) {
        return {};
    }

    const double latitudeDegrees = latitudeRadians * 180.0 / Pi;
    const double longitudeDegrees = normalizedLongitudeDegrees(longitudeRadians);
    const double longitudeCoordinate =
        std::clamp((longitudeDegrees - static_cast<double>(key->longitudeDegrees)) /
                       cell->longitudeIntervalDegrees,
                   0.0, static_cast<double>(cell->longitudeSampleCount - 1));
    const double latitudeCoordinate =
        std::clamp((latitudeDegrees - static_cast<double>(key->latitudeDegrees)) /
                       cell->latitudeIntervalDegrees,
                   0.0, static_cast<double>(cell->latitudeSampleCount - 1));
    SamplePosition result;
    result.key = *key;
    result.cell = cell;
    result.longitude0 = static_cast<int>(std::floor(longitudeCoordinate));
    result.latitude0 = static_cast<int>(std::floor(latitudeCoordinate));
    result.longitude1 = std::min(result.longitude0 + 1, cell->longitudeSampleCount - 1);
    result.latitude1 = std::min(result.latitude0 + 1, cell->latitudeSampleCount - 1);
    result.longitudeFraction = longitudeCoordinate - static_cast<double>(result.longitude0);
    result.latitudeFraction = latitudeCoordinate - static_cast<double>(result.latitude0);
    return result;
}

std::optional<double> DtedMosaicSampler::interpolatedElevation(const SamplePosition &position,
                                                               const DtedWaterMaskCell *mask,
                                                               std::optional<bool> water) {
    const DtedCell &cell = *position.cell;
    const int longitude0 = position.longitude0;
    const int longitude1 = position.longitude1;
    const int latitude0 = position.latitude0;
    const int latitude1 = position.latitude1;
    const double longitudeFraction = position.longitudeFraction;
    const double latitudeFraction = position.latitudeFraction;

    const std::array<std::optional<double>, 4> samples{
        cell.elevation(longitude0, latitude0), cell.elevation(longitude1, latitude0),
        cell.elevation(longitude0, latitude1), cell.elevation(longitude1, latitude1)};
    const auto maskWater = [&cell, mask](int longitudeIndex,
                                         int latitudeIndex) -> std::optional<bool> {
        if (mask == nullptr) {
            return std::nullopt;
        }
        const double longitudeCellFraction = static_cast<double>(longitudeIndex) /
                                             static_cast<double>(cell.longitudeSampleCount - 1);
        const double latitudeCellFraction =
            static_cast<double>(latitudeIndex) / static_cast<double>(cell.latitudeSampleCount - 1);
        return mask->waterAtFraction(longitudeCellFraction, latitudeCellFraction);
    };
    const std::array<std::optional<bool>, 4> classifications{
        maskWater(longitude0, latitude0), maskWater(longitude1, latitude0),
        maskWater(longitude0, latitude1), maskWater(longitude1, latitude1)};
    const std::array<double, 4> weights{(1.0 - longitudeFraction) * (1.0 - latitudeFraction),
                                        longitudeFraction * (1.0 - latitudeFraction),
                                        (1.0 - longitudeFraction) * latitudeFraction,
                                        longitudeFraction * latitudeFraction};
    double weightedElevation = 0.0;
    double validWeight = 0.0;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const bool classificationMatches =
            !water || (classifications[index] && *classifications[index] == *water);
        if (samples[index] && classificationMatches) {
            weightedElevation += *samples[index] * weights[index];
            validWeight += weights[index];
        }
    }
    if (validWeight <= std::numeric_limits<double>::epsilon()) {
        return std::nullopt;
    }
    return weightedElevation / validWeight;
}

std::optional<double> DtedMosaicSampler::sampleRadians(double latitudeRadians,
                                                       double longitudeRadians) {
    const std::optional<SamplePosition> position = positionFor(latitudeRadians, longitudeRadians);
    if (!position) {
        return std::nullopt;
    }
    const std::optional<double> elevation = interpolatedElevation(*position);
    if (!elevation) {
        m_lastError = QStringLiteral("%1 samples at the requested coordinate contain no data.")
                          .arg(dtedLevelDisplayName(m_source.level()));
        return std::nullopt;
    }
    m_lastError.clear();
    return elevation;
}

std::optional<DtedSurfaceSample> DtedMosaicSampler::sampleSurfaceRadians(double latitudeRadians,
                                                                         double longitudeRadians) {
    const std::optional<SamplePosition> position = positionFor(latitudeRadians, longitudeRadians);
    if (!position) {
        return std::nullopt;
    }
    const std::shared_ptr<const DtedWaterMaskCell> mask =
        waterMaskFor(position->key, *position->cell);
    bool water = false;
    if (mask != nullptr) {
        const double longitudeCellFraction =
            (static_cast<double>(position->longitude0) + position->longitudeFraction) /
            static_cast<double>(position->cell->longitudeSampleCount - 1);
        const double latitudeCellFraction =
            (static_cast<double>(position->latitude0) + position->latitudeFraction) /
            static_cast<double>(position->cell->latitudeSampleCount - 1);
        water = mask->waterAtFraction(longitudeCellFraction, latitudeCellFraction).value_or(false);
    }
    std::optional<double> elevation =
        mask != nullptr ? interpolatedElevation(*position, mask.get(), water) : std::nullopt;
    if (!elevation) {
        elevation = interpolatedElevation(*position);
    }
    if (!elevation) {
        m_lastError = QStringLiteral("%1 samples at the requested coordinate contain no data.")
                          .arg(dtedLevelDisplayName(m_source.level()));
        return std::nullopt;
    }
    m_lastError.clear();
    return DtedSurfaceSample{water ? 0.0 : *elevation, water ? std::max(0.0, -*elevation) : 0.0,
                             water};
}
