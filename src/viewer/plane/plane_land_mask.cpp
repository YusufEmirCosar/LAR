#include "viewer/plane/plane_land_mask.h"

#include "viewer/lar_projection.h"
#include "viewer/map/map_projection.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace {

constexpr double DegreesPerRadian = 180.0 / LarProjection::Pi;
constexpr double DesiredMetersPerPixel = 50.0;
constexpr int MinimumMaskResolution = 256;
constexpr int MaximumMaskResolution = 2048;

double edge(double ax, double ay, double bx, double by, double px, double py) noexcept {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

bool pointInTriangle(const std::array<double, 2> &a, const std::array<double, 2> &b,
                     const std::array<double, 2> &c, double x, double y) noexcept {
    constexpr double Epsilon = 1.0e-8;
    const double first = edge(a[0], a[1], b[0], b[1], x, y);
    const double second = edge(b[0], b[1], c[0], c[1], x, y);
    const double third = edge(c[0], c[1], a[0], a[1], x, y);
    const bool negative = first < -Epsilon || second < -Epsilon || third < -Epsilon;
    const bool positive = first > Epsilon || second > Epsilon || third > Epsilon;
    return !(negative && positive);
}

std::size_t texelIndex(int x, int y, int resolution) noexcept {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(resolution) +
           static_cast<std::size_t>(x);
}

} // namespace

bool PlaneLandMask::valid() const noexcept {
    if (!std::isfinite(halfExtentMeters) || halfExtentMeters <= 0.0 || resolution <= 0 ||
        resolution > MaximumMaskResolution) {
        return false;
    }
    const std::size_t expected =
        static_cast<std::size_t>(resolution) * static_cast<std::size_t>(resolution);
    return coverage == PlaneLandCoverage::Mixed ? texels.size() == expected : texels.empty();
}

bool PlaneLandMask::landAtLocal(double eastMeters, double northMeters) const noexcept {
    if (!valid() || !std::isfinite(eastMeters) || !std::isfinite(northMeters)) {
        return false;
    }
    if (coverage == PlaneLandCoverage::AllLand) {
        return true;
    }
    if (coverage == PlaneLandCoverage::AllWater || eastMeters < -halfExtentMeters ||
        eastMeters > halfExtentMeters || northMeters < -halfExtentMeters ||
        northMeters > halfExtentMeters) {
        return false;
    }
    const double scale = static_cast<double>(resolution) / (2.0 * halfExtentMeters);
    const int x =
        std::clamp(static_cast<int>((eastMeters + halfExtentMeters) * scale), 0, resolution - 1);
    const int y =
        std::clamp(static_cast<int>((northMeters + halfExtentMeters) * scale), 0, resolution - 1);
    return texels[texelIndex(x, y, resolution)] >= 128U;
}

PlaneLandMaskBuilder::PlaneLandMaskBuilder(lar::map::MapLandIndex landIndex)
    : m_landIndex(std::move(landIndex)) {}

int PlaneLandMaskBuilder::resolutionFor(double halfExtentMeters) noexcept {
    if (!std::isfinite(halfExtentMeters) || halfExtentMeters <= 0.0) {
        return 0;
    }
    const double desired = std::ceil(2.0 * halfExtentMeters / DesiredMetersPerPixel);
    int resolution = MinimumMaskResolution;
    while (resolution < MaximumMaskResolution && static_cast<double>(resolution) < desired) {
        resolution *= 2;
    }
    return std::clamp(resolution, MinimumMaskResolution, MaximumMaskResolution);
}

PlaneLandMask PlaneLandMaskBuilder::build(double anchorLatitudeRadians,
                                          double anchorLongitudeRadians,
                                          double projectionOriginLatitudeRadians,
                                          double halfExtentMeters,
                                          const std::function<bool()> &cancelled) const {
    PlaneLandMask result;
    result.halfExtentMeters = halfExtentMeters;
    result.resolution = resolutionFor(halfExtentMeters);
    if (!m_landIndex.isValid() || result.resolution <= 0 || !std::isfinite(anchorLatitudeRadians) ||
        !std::isfinite(anchorLongitudeRadians) || !std::isfinite(projectionOriginLatitudeRadians)) {
        result.resolution = 0;
        return result;
    }

    const double anchor[3]{anchorLatitudeRadians, anchorLongitudeRadians, 0.0};
    std::array<GeoCoordinateRadians, 4> corners{};
    const std::array<QPointF, 4> localCorners{
        QPointF(-halfExtentMeters, -halfExtentMeters), QPointF(halfExtentMeters, -halfExtentMeters),
        QPointF(halfExtentMeters, halfExtentMeters), QPointF(-halfExtentMeters, halfExtentMeters)};
    for (std::size_t index = 0U; index < corners.size(); ++index) {
        const auto coordinate = LarProjection::planeWorldToGeographic(
            localCorners[index], anchor, 0.0, projectionOriginLatitudeRadians, true);
        if (!coordinate) {
            result.resolution = 0;
            return result;
        }
        corners[index] = *coordinate;
    }

    const double anchorLongitudeDegrees = anchorLongitudeRadians * DegreesPerRadian;
    lar::map::MapGeographicBounds bounds;
    bounds.westDegrees = std::numeric_limits<double>::infinity();
    bounds.eastDegrees = -std::numeric_limits<double>::infinity();
    bounds.southDegrees = std::numeric_limits<double>::infinity();
    bounds.northDegrees = -std::numeric_limits<double>::infinity();
    for (const GeoCoordinateRadians &corner : corners) {
        const double longitude = lar::map::MapProjection::unwrapLongitude(
            corner.longitude * DegreesPerRadian, anchorLongitudeDegrees);
        const double latitude = corner.latitude * DegreesPerRadian;
        bounds.westDegrees = std::min(bounds.westDegrees, longitude);
        bounds.eastDegrees = std::max(bounds.eastDegrees, longitude);
        bounds.southDegrees = std::min(bounds.southDegrees, latitude);
        bounds.northDegrees = std::max(bounds.northDegrees, latitude);
    }

    std::vector<std::uint32_t> candidates;
    m_landIndex.appendCandidates(bounds, candidates);
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    if (candidates.empty()) {
        return result;
    }

    const std::size_t texelCount =
        static_cast<std::size_t>(result.resolution) * static_cast<std::size_t>(result.resolution);
    result.texels.assign(texelCount, 0U);
    const std::shared_ptr<const lar::map::MapMesh> &mesh = m_landIndex.mesh();
    const double pixelsPerMeter = static_cast<double>(result.resolution) / (2.0 * halfExtentMeters);
    for (const std::uint32_t triangle : candidates) {
        if (cancelled && cancelled()) {
            result.resolution = 0;
            result.texels.clear();
            return result;
        }
        const std::size_t indexOffset = static_cast<std::size_t>(triangle) * 3U;
        if (indexOffset + 2U >= mesh->mercatorFillIndices.size()) {
            continue;
        }
        std::array<std::array<double, 2>, 3> pixel{};
        bool finite = true;
        for (std::size_t corner = 0U; corner < 3U; ++corner) {
            const std::size_t vertex = mesh->mercatorFillIndices[indexOffset + corner];
            const std::size_t vertexOffset = vertex * 3U;
            if (vertexOffset + 1U >= mesh->vertices.size()) {
                finite = false;
                break;
            }
            const double longitudeDegrees = lar::map::MapProjection::unwrapLongitude(
                mesh->vertices[vertexOffset], anchorLongitudeDegrees);
            const double position[3]{mesh->vertices[vertexOffset + 1U] / DegreesPerRadian,
                                     longitudeDegrees / DegreesPerRadian, 0.0};
            const QPointF local = LarProjection::geographicToPlaneWorld(
                position, anchor, 0.0, projectionOriginLatitudeRadians, true);
            pixel[corner] = {(local.x() + halfExtentMeters) * pixelsPerMeter,
                             (local.y() + halfExtentMeters) * pixelsPerMeter};
            finite = finite && std::isfinite(pixel[corner][0]) && std::isfinite(pixel[corner][1]);
        }
        if (!finite) {
            continue;
        }

        const double minimumX = std::min({pixel[0][0], pixel[1][0], pixel[2][0]});
        const double maximumX = std::max({pixel[0][0], pixel[1][0], pixel[2][0]});
        const double minimumY = std::min({pixel[0][1], pixel[1][1], pixel[2][1]});
        const double maximumY = std::max({pixel[0][1], pixel[1][1], pixel[2][1]});
        if (maximumX < 0.0 || maximumY < 0.0 ||
            minimumX >= static_cast<double>(result.resolution) ||
            minimumY >= static_cast<double>(result.resolution)) {
            continue;
        }
        const int firstX =
            std::clamp(static_cast<int>(std::floor(minimumX)), 0, result.resolution - 1);
        const int lastX =
            std::clamp(static_cast<int>(std::ceil(maximumX)), 0, result.resolution - 1);
        const int firstY =
            std::clamp(static_cast<int>(std::floor(minimumY)), 0, result.resolution - 1);
        const int lastY =
            std::clamp(static_cast<int>(std::ceil(maximumY)), 0, result.resolution - 1);
        for (int y = firstY; y <= lastY; ++y) {
            if (cancelled && cancelled()) {
                result.resolution = 0;
                result.texels.clear();
                return result;
            }
            const double sampleY = static_cast<double>(y) + 0.5;
            for (int x = firstX; x <= lastX; ++x) {
                if (pointInTriangle(pixel[0], pixel[1], pixel[2], static_cast<double>(x) + 0.5,
                                    sampleY)) {
                    result.texels[texelIndex(x, y, result.resolution)] = 255U;
                }
            }
        }
    }

    const std::size_t landCount =
        static_cast<std::size_t>(std::count_if(result.texels.cbegin(), result.texels.cend(),
                                               [](unsigned char value) { return value >= 128U; }));
    if (landCount == 0U) {
        result.coverage = PlaneLandCoverage::AllWater;
        result.texels.clear();
    } else if (landCount == texelCount) {
        result.coverage = PlaneLandCoverage::AllLand;
        result.texels.clear();
    } else {
        result.coverage = PlaneLandCoverage::Mixed;
    }
    return result;
}
