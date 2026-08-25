#include "viewer/plane/plane_terrain_patch_builder.h"

#include "viewer/lar_geodesic_geometry.h"
#include "viewer/lar_projection.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
    return std::isfinite(request.latitudeRadians) && request.latitudeRadians >= -HalfPi &&
           request.latitudeRadians <= HalfPi && std::isfinite(request.longitudeRadians) &&
           request.longitudeRadians >= -LarProjection::Pi &&
           request.longitudeRadians <= LarProjection::Pi &&
           std::isfinite(request.halfExtentMeters) &&
           request.halfExtentMeters >= MinimumHalfExtentMeters &&
           request.halfExtentMeters <= MaximumHalfExtentMeters &&
           std::isfinite(request.metersPerSceneUnit) && request.metersPerSceneUnit > 0.0 &&
           request.resolution >= MinimumResolution && request.resolution <= MaximumResolution;
}

std::size_t vertexIndex(int row, int column, int resolution) noexcept {
    return static_cast<std::size_t>(row) * static_cast<std::size_t>(resolution) +
           static_cast<std::size_t>(column);
}

} // namespace

PlaneTerrainPatchBuilder::PlaneTerrainPatchBuilder(QString dtedRootDirectory,
                                                   QString waterMaskPackPath)
    : m_sampler(DtedTileSource(std::move(dtedRootDirectory)),
                DtedWaterMaskSource(std::move(waterMaskPackPath))) {}

PlaneTerrainPatchPtr PlaneTerrainPatchBuilder::build(const PlaneTerrainBuildRequest &request,
                                                     QString *errorMessage) {
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
    const GeoCoordinateRadians anchor{request.latitudeRadians, request.longitudeRadians};

    double minimumElevation = std::numeric_limits<double>::infinity();
    double maximumElevation = -std::numeric_limits<double>::infinity();
    double maximumWaterDepth = 0.0;
    std::size_t validSamples = 0U;
    std::size_t waterSamples = 0U;
    for (int row = 0; row < resolution; ++row) {
        const double northMeters = -request.halfExtentMeters + sampleSpacing * row;
        for (int column = 0; column < resolution; ++column) {
            const double eastMeters = -request.halfExtentMeters + sampleSpacing * column;
            const double distanceMeters = std::hypot(eastMeters, northMeters);
            const double bearingRadians = std::atan2(eastMeters, northMeters);
            const GeoCoordinateRadians coordinate =
                distanceMeters > 0.0
                    ? LarGeodesicGeometry::destination(anchor, distanceMeters, bearingRadians)
                    : anchor;
            const std::optional<DtedSurfaceSample> surface =
                m_sampler.sampleSurfaceRadians(coordinate.latitude, coordinate.longitude);
            const std::size_t index = vertexIndex(row, column, resolution);
            if (!surface || !std::isfinite(surface->elevationMeters) ||
                !std::isfinite(surface->waterDepthMeters)) {
                continue;
            }
            elevations[index] = surface->elevationMeters;
            waterDepths[index] = surface->waterDepthMeters;
            valid[index] = 1U;
            water[index] = surface->water ? 1U : 0U;
            minimumElevation = std::min(minimumElevation, surface->elevationMeters);
            maximumElevation = std::max(maximumElevation, surface->elevationMeters);
            maximumWaterDepth = std::max(maximumWaterDepth, surface->waterDepthMeters);
            waterSamples += surface->water ? 1U : 0U;
            ++validSamples;
        }
    }

    const int centerIndex = resolution / 2;
    const std::size_t centerOffset = vertexIndex(centerIndex, centerIndex, resolution);
    if (validSamples == 0U || valid[centerOffset] == 0U) {
        const QString detail = m_sampler.lastError();
        setError(errorMessage,
                 detail.isEmpty()
                     ? QStringLiteral("No DTED0 elevation is available near the aircraft.")
                     : detail);
        return nullptr;
    }

    auto patch = std::make_shared<PlaneTerrainPatch>();
    patch->anchorLatitudeRadians = request.latitudeRadians;
    patch->anchorLongitudeRadians = request.longitudeRadians;
    patch->halfExtentMeters = request.halfExtentMeters;
    patch->metersPerSceneUnit = request.metersPerSceneUnit;
    patch->minimumElevationMeters = minimumElevation;
    patch->maximumElevationMeters = maximumElevation;
    patch->centerElevationMeters = elevations[centerOffset];
    patch->maximumWaterDepthMeters = maximumWaterDepth;
    patch->validSampleCount = validSamples;
    patch->waterSampleCount = waterSamples;
    patch->resolution = resolution;
    patch->vertices.reserve(sampleCount * PlaneTerrainVertexStrideFloats);

    for (int row = 0; row < resolution; ++row) {
        for (int column = 0; column < resolution; ++column) {
            const std::size_t index = vertexIndex(row, column, resolution);
            const double center = elevations[index];
            const int leftColumn =
                column > 0 && valid[vertexIndex(row, column - 1, resolution)] ? column - 1 : column;
            const int rightColumn =
                column + 1 < resolution && valid[vertexIndex(row, column + 1, resolution)]
                    ? column + 1
                    : column;
            const int southRow =
                row > 0 && valid[vertexIndex(row - 1, column, resolution)] ? row - 1 : row;
            const int northRow =
                row + 1 < resolution && valid[vertexIndex(row + 1, column, resolution)] ? row + 1
                                                                                        : row;
            const double eastSpan =
                std::max(sampleSpacing, (rightColumn - leftColumn) * sampleSpacing);
            const double northSpan = std::max(sampleSpacing, (northRow - southRow) * sampleSpacing);
            const double eastSlope = valid[index] != 0U
                                         ? (elevations[vertexIndex(row, rightColumn, resolution)] -
                                            elevations[vertexIndex(row, leftColumn, resolution)]) /
                                               eastSpan
                                         : 0.0;
            const double northSlope =
                valid[index] != 0U ? (elevations[vertexIndex(northRow, column, resolution)] -
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
                                    water[index] != 0U ? 1.0F : 0.0F,
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
        setError(errorMessage,
                 QStringLiteral("Available DTED0 samples do not form terrain triangles."));
        return nullptr;
    }
    setError(errorMessage, {});
    return patch;
}
