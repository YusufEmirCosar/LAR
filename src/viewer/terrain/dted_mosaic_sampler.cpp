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

std::optional<double> DtedMosaicSampler::interpolatedElevation(const SamplePosition &position) {
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
    const std::array<double, 4> weights{(1.0 - longitudeFraction) * (1.0 - latitudeFraction),
                                        longitudeFraction * (1.0 - latitudeFraction),
                                        (1.0 - longitudeFraction) * latitudeFraction,
                                        longitudeFraction * latitudeFraction};
    double weightedElevation = 0.0;
    double validWeight = 0.0;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        if (samples[index]) {
            weightedElevation += *samples[index] * weights[index];
            validWeight += weights[index];
        }
    }
    if (validWeight <= std::numeric_limits<double>::epsilon()) {
        return std::nullopt;
    }
    // Void posts contribute neither value nor weight, so divide by the
    // remaining weight rather than depressing the surface toward zero.
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
