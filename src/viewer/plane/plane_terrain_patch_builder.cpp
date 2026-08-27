#include "viewer/plane/plane_terrain_patch_builder.h"

#include "viewer/lar_projection.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace {

constexpr int MinimumResolution = 17;
constexpr int MaximumResolution = 257;
constexpr double MinimumHalfExtentMeters = 1000.0;
constexpr double MaximumHalfExtentMeters = 100000.0;

void setError(QString *destination, const QString &message) {
    if (destination != nullptr) {
        *destination = message;
    }
}

bool validRequest(const PlaneTerrainBuildRequest &request) noexcept {
    constexpr double HalfPi = LarProjection::Pi * 0.5;
    const bool validProjectionOrigin =
        std::isnan(request.projectionOriginLatitudeRadians) ||
        (std::isfinite(request.projectionOriginLatitudeRadians) &&
         request.projectionOriginLatitudeRadians >= -HalfPi &&
         request.projectionOriginLatitudeRadians <= HalfPi);
    return std::isfinite(request.latitudeRadians) && request.latitudeRadians >= -HalfPi &&
           request.latitudeRadians <= HalfPi && std::isfinite(request.longitudeRadians) &&
           request.longitudeRadians >= -LarProjection::Pi &&
           request.longitudeRadians <= LarProjection::Pi &&
           std::isfinite(request.halfExtentMeters) &&
           request.halfExtentMeters >= MinimumHalfExtentMeters &&
           request.halfExtentMeters <= MaximumHalfExtentMeters &&
           validProjectionOrigin &&
           std::isfinite(request.metersPerSceneUnit) && request.metersPerSceneUnit > 0.0 &&
           request.resolution >= MinimumResolution && request.resolution <= MaximumResolution;
}

std::size_t vertexIndex(int row, int column, int resolution) noexcept {
    return static_cast<std::size_t>(row) * static_cast<std::size_t>(resolution) +
           static_cast<std::size_t>(column);
}

} // namespace

PlaneTerrainPatchBuilder::PlaneTerrainPatchBuilder(QString dtedRootDirectory,
                                                   lar::map::MapLandIndex landIndex)
    : PlaneTerrainPatchBuilder(DtedDataset{std::move(dtedRootDirectory), DtedLevel::Level0},
                               std::move(landIndex)) {}

PlaneTerrainPatchBuilder::PlaneTerrainPatchBuilder(DtedDataset dataset,
                                                   lar::map::MapLandIndex landIndex)
    : m_sampler(DtedTileSource(std::move(dataset.rootDirectory), dataset.level)),
      m_landMaskBuilder(std::move(landIndex)),
      m_level(dataset.level) {}

PlaneTerrainPatchPtr PlaneTerrainPatchBuilder::build(const PlaneTerrainBuildRequest &request,
                                                     QString *errorMessage,
                                                     const std::function<bool()> &cancelled) {
    if (!validRequest(request)) {
        setError(errorMessage, QStringLiteral("Terrain patch request is outside bounded limits."));
        return nullptr;
    }
    const int resolution = request.resolution;
    const std::size_t sampleCount =
        static_cast<std::size_t>(resolution) * static_cast<std::size_t>(resolution);
    std::vector<double> elevations(sampleCount, 0.0);
    std::vector<double> waterDepths(sampleCount, 0.0);
    std::vector<unsigned char> valid(sampleCount, 0U);
    std::vector<unsigned char> water(sampleCount, 0U);
    const double sampleSpacing =
        request.halfExtentMeters * 2.0 / static_cast<double>(resolution - 1);
    const double anchorPosition[3]{request.latitudeRadians, request.longitudeRadians, 0.0};
    const double projectionOriginLatitude =
        std::isfinite(request.projectionOriginLatitudeRadians)
            ? request.projectionOriginLatitudeRadians
            : request.latitudeRadians;
    PlaneLandMask landMask = m_landMaskBuilder.build(
        request.latitudeRadians, request.longitudeRadians, projectionOriginLatitude,
        request.halfExtentMeters, cancelled);
    if (!landMask.valid()) {
        setError(errorMessage,
                 cancelled && cancelled()
                     ? QStringLiteral("Terrain patch request was superseded.")
                     : QStringLiteral("The packaged vector land map is unavailable."));
        return nullptr;
    }

    double minimumElevation = std::numeric_limits<double>::infinity();
    double maximumElevation = -std::numeric_limits<double>::infinity();
    double maximumWaterDepth = 0.0;
    std::size_t validSamples = 0U;
    std::size_t waterSamples = 0U;
    for (int row = 0; row < resolution; ++row) {
        if (cancelled && cancelled()) {
            setError(errorMessage, QStringLiteral("Terrain patch request was superseded."));
            return nullptr;
        }
        const double northMeters = -request.halfExtentMeters + sampleSpacing * row;
        for (int column = 0; column < resolution; ++column) {
            const double eastMeters = -request.halfExtentMeters + sampleSpacing * column;
            // The Plane surface, target marker, and LAR rings all use this local flat metric.
            // Sampling with a spherical destination while retaining east/north vertex coordinates
            // shifts terrain against those overlays (up to hundreds of metres at patch corners).
            // Invert the exact renderer projection so the DTED elevation belongs to the
            // geographic point represented by this vertex.
            const std::optional<GeoCoordinateRadians> coordinate =
                LarProjection::planeWorldToGeographic({eastMeters, northMeters}, anchorPosition,
                                                      0.0, projectionOriginLatitude, true);
            if (!coordinate) {
                continue;
            }
            const std::optional<double> sampledElevation =
                m_sampler.sampleRadians(coordinate->latitude, coordinate->longitude);
            const std::size_t index = vertexIndex(row, column, resolution);
            if (!sampledElevation || !std::isfinite(*sampledElevation)) {
                continue;
            }
            const bool isLand = landMask.landAtLocal(eastMeters, northMeters);
            const double surfaceElevation = isLand ? *sampledElevation : 0.0;
            const double waterDepth = isLand ? 0.0 : std::max(0.0, -*sampledElevation);
            elevations[index] = *sampledElevation;
            waterDepths[index] = waterDepth;
            valid[index] = 1U;
            water[index] = isLand ? 0U : 1U;
            minimumElevation = std::min(minimumElevation, surfaceElevation);
            maximumElevation = std::max(maximumElevation, surfaceElevation);
            maximumWaterDepth = std::max(maximumWaterDepth, waterDepth);
            waterSamples += isLand ? 0U : 1U;
            ++validSamples;
        }
    }

    const int centerIndex = resolution / 2;
    const std::size_t centerOffset = vertexIndex(centerIndex, centerIndex, resolution);
    if (validSamples == 0U || valid[centerOffset] == 0U) {
        const QString detail = m_sampler.lastError();
        setError(errorMessage,
                 detail.isEmpty()
                     ? QStringLiteral("No %1 elevation is available near the aircraft.")
                           .arg(dtedLevelDisplayName(m_level))
                     : detail);
        return nullptr;
    }

    auto patch = std::make_shared<PlaneTerrainPatch>();
    patch->anchorLatitudeRadians = request.latitudeRadians;
    patch->anchorLongitudeRadians = request.longitudeRadians;
    patch->projectionOriginLatitudeRadians = projectionOriginLatitude;
    patch->halfExtentMeters = request.halfExtentMeters;
    patch->metersPerSceneUnit = request.metersPerSceneUnit;
    patch->minimumElevationMeters = minimumElevation;
    patch->maximumElevationMeters = maximumElevation;
    patch->centerElevationMeters =
        water[centerOffset] != 0U ? 0.0 : elevations[centerOffset];
    patch->maximumWaterDepthMeters = maximumWaterDepth;
    patch->validSampleCount = validSamples;
    patch->waterSampleCount = waterSamples;
    patch->resolution = resolution;
    patch->sourceLevel = m_level;
    patch->landMask = std::move(landMask);
    patch->vertices.reserve(sampleCount * PlaneTerrainVertexStrideFloats);

    for (int row = 0; row < resolution; ++row) {
        for (int column = 0; column < resolution; ++column) {
            const std::size_t index = vertexIndex(row, column, resolution);
            const double center = elevations[index];
            const auto usableNeighbor = [&valid, &water, index, resolution](int neighborRow,
                                                                            int neighborColumn) {
                const std::size_t neighbor =
                    vertexIndex(neighborRow, neighborColumn, resolution);
                return valid[neighbor] != 0U && water[neighbor] == water[index];
            };
            const int leftColumn = column > 0 && usableNeighbor(row, column - 1)
                                       ? column - 1
                                       : column;
            const int rightColumn =
                column + 1 < resolution && usableNeighbor(row, column + 1)
                    ? column + 1
                    : column;
            const int southRow = row > 0 && usableNeighbor(row - 1, column) ? row - 1 : row;
            const int northRow =
                row + 1 < resolution && usableNeighbor(row + 1, column) ? row + 1 : row;
            const double eastSpan =
                std::max(sampleSpacing, (rightColumn - leftColumn) * sampleSpacing);
            const double northSpan = std::max(sampleSpacing, (northRow - southRow) * sampleSpacing);
            const double eastSlope = valid[index] != 0U && water[index] == 0U
                                         ? (elevations[vertexIndex(row, rightColumn, resolution)] -
                                            elevations[vertexIndex(row, leftColumn, resolution)]) /
                                               eastSpan
                                         : 0.0;
            const double northSlope =
                valid[index] != 0U && water[index] == 0U
                    ? (elevations[vertexIndex(northRow, column, resolution)] -
                       elevations[vertexIndex(southRow, column, resolution)]) /
                          northSpan
                    : 0.0;
            const double normalLength =
                std::sqrt(eastSlope * eastSlope + 1.0 + northSlope * northSlope);
            const double eastMeters = -request.halfExtentMeters + sampleSpacing * column;
            const double northMeters = -request.halfExtentMeters + sampleSpacing * row;
            patch->vertices.insert(patch->vertices.end(),
                                   {static_cast<float>(eastMeters / request.metersPerSceneUnit),
                                    static_cast<float>(center / request.metersPerSceneUnit),
                                    static_cast<float>(-northMeters / request.metersPerSceneUnit),
                                    static_cast<float>(-eastSlope / normalLength),
                                    static_cast<float>(1.0 / normalLength),
                                    static_cast<float>(northSlope / normalLength),
                                    static_cast<float>(waterDepths[index])});
        }
    }

    patch->indices.reserve(static_cast<std::size_t>(resolution - 1) *
                           static_cast<std::size_t>(resolution - 1) * 6U);
    for (int row = 0; row + 1 < resolution; ++row) {
        for (int column = 0; column + 1 < resolution; ++column) {
            const std::size_t southWest = vertexIndex(row, column, resolution);
            const std::size_t southEast = vertexIndex(row, column + 1, resolution);
            const std::size_t northWest = vertexIndex(row + 1, column, resolution);
            const std::size_t northEast = vertexIndex(row + 1, column + 1, resolution);
            if (valid[southWest] == 0U || valid[southEast] == 0U || valid[northWest] == 0U ||
                valid[northEast] == 0U) {
                continue;
            }
            patch->indices.insert(
                patch->indices.end(),
                {static_cast<std::uint32_t>(southWest), static_cast<std::uint32_t>(southEast),
                 static_cast<std::uint32_t>(northWest), static_cast<std::uint32_t>(southEast),
                 static_cast<std::uint32_t>(northEast), static_cast<std::uint32_t>(northWest)});
        }
    }
    if (patch->empty()) {
        setError(errorMessage, QStringLiteral("Available %1 samples do not form terrain triangles.")
                                   .arg(dtedLevelDisplayName(m_level)));
        return nullptr;
    }
    patch->sampleValidity = std::move(valid);
    setError(errorMessage, {});
    return patch;
}
