
#include "viewer/viewport/lar_zone_mesh_assembler.h"

#include "viewer/map/map_projection.h"
#include "viewer/viewport/lar_zone_mesh_limits.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

constexpr double ToDegrees = 180.0 / LarGeodesicGeometry::Pi;

class MeshAppender final {
  public:
    explicit MeshAppender(LarZoneMesh &mesh) : m_mesh(mesh) {}

    std::uint32_t vertex(double latitudeRadians, double longitudeDegrees) {
        if (m_failed || m_mesh.vertices.size() / 3U >= LarZoneMeshLimits::MaximumVertexCount ||
            !std::isfinite(latitudeRadians) || !std::isfinite(longitudeDegrees)) {
            m_failed = true;
            return 0U;
        }
        const double latitudeDegrees = latitudeRadians * ToDegrees;
        const double mercatorY = lar::map::MapProjection::projectLatitude(latitudeDegrees);
        if (!std::isfinite(latitudeDegrees) || !std::isfinite(mercatorY) ||
            std::abs(latitudeDegrees) > 90.0 || std::abs(longitudeDegrees) > 540.0) {
            m_failed = true;
            return 0U;
        }
        const auto index = static_cast<std::uint32_t>(m_mesh.vertices.size() / 3U);
        double storedLongitude = longitudeDegrees;
        double storedLatitude = latitudeDegrees;
        double storedMercatorY = mercatorY;
        if (m_mesh.coordinateSpace == LarZoneCoordinateSpace::MercatorCameraRelative) {
            storedLongitude -= m_mesh.coordinateOrigin.x();
            storedMercatorY -= m_mesh.coordinateOrigin.y();
        } else if (m_mesh.coordinateSpace == LarZoneCoordinateSpace::SphereCameraRelative) {
            storedLongitude -= m_mesh.coordinateOrigin.x();
            storedLatitude -= m_mesh.coordinateOrigin.y();
        }
        m_mesh.vertices.push_back(static_cast<float>(storedLongitude));
        m_mesh.vertices.push_back(static_cast<float>(storedLatitude));
        m_mesh.vertices.push_back(static_cast<float>(storedMercatorY));
        return index;
    }

    void index(std::uint32_t value) {
        if (m_failed || value >= m_mesh.vertices.size() / 3U ||
            m_mesh.indices.size() >= LarZoneMeshLimits::MaximumIndexCount) {
            m_failed = true;
            return;
        }
        m_mesh.indices.push_back(value);
    }
    void triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        index(a);
        index(b);
        index(c);
    }
    void line(std::uint32_t a, std::uint32_t b) {
        index(a);
        index(b);
    }
    void lineStrip(std::uint32_t first, int count) {
        for (int offset = 0; offset + 1 < count; ++offset) {
            line(first + static_cast<std::uint32_t>(offset),
                 first + static_cast<std::uint32_t>(offset + 1));
        }
    }
    bool failed() const noexcept {
        return m_failed;
    }

  private:
    LarZoneMesh &m_mesh;
    bool m_failed = false;
};

std::vector<double> unwrappedLongitudes(const GeodesicZoneSampleGrid &samples, std::size_t row,
                                        double reference) {
    std::vector<double> result;
    result.reserve(samples.columnCount());
    double previous = reference;
    for (std::size_t column = 0; column < samples.columnCount(); ++column) {
        const double longitude = lar::map::MapProjection::unwrapLongitude(
            samples.point(row, column).longitude * ToDegrees, previous);
        result.push_back(longitude);
        previous = longitude;
    }
    if (!result.empty()) {
        const auto bounds = std::minmax_element(result.begin(), result.end());
        const double midpoint = (*bounds.first + *bounds.second) * 0.5;
        const double shift =
            std::round((reference - midpoint) / lar::map::MapProjection::WorldWidthDegrees) *
            lar::map::MapProjection::WorldWidthDegrees;
        for (double &longitude : result)
            longitude += shift;
    }
    return result;
}

void appendRadialLine(MeshAppender &appender, std::uint32_t firstVertex, std::size_t rowCount,
                      std::size_t columnCount, std::size_t column, std::size_t firstRow) {
    for (std::size_t row = firstRow; row + 1U < rowCount; ++row) {
        appender.line(firstVertex + static_cast<std::uint32_t>(row * columnCount + column),
                      firstVertex + static_cast<std::uint32_t>((row + 1U) * columnCount + column));
    }
}

} // namespace

bool LarZoneMeshAssembler::append(const LarZoneDefinition &zone,
                                  const GeodesicZoneSampleGrid &samples, LarZoneMesh &mesh,
                                  LarZoneDrawRange &fillRange, LarZoneDrawRange &lineRange) const {
    const std::size_t rows = samples.rowCount();
    const std::size_t columns = samples.columnCount();
    if (rows < 2U || columns < 2U || samples.points.size() != rows * columns)
        return false;
    MeshAppender appender(mesh);
    const auto firstVertex = static_cast<std::uint32_t>(mesh.vertices.size() / 3U);
    const double reference = zone.center.longitude * ToDegrees;
    for (std::size_t row = 0; row < rows; ++row) {
        const auto longitudes = unwrappedLongitudes(samples, row, reference);
        for (std::size_t column = 0; column < columns; ++column) {
            const auto &point = samples.point(row, column);
            if (mesh.coordinateSpace == LarZoneCoordinateSpace::MercatorCameraRelative &&
                std::abs(point.latitude * ToDegrees) >
                    lar::map::MapProjection::MaximumMercatorLatitudeDegrees) {
                mesh.mercatorGeometryClipped = true;
            }
            appender.vertex(point.latitude, longitudes[column]);
        }
    }

    fillRange.firstIndex = mesh.indices.size();
    for (std::size_t row = 0; row + 1U < rows; ++row) {
        for (std::size_t column = 0; column + 1U < columns; ++column) {
            const std::uint32_t inner0 =
                firstVertex + static_cast<std::uint32_t>(row * columns + column);
            const std::uint32_t inner1 = inner0 + 1U;
            const std::uint32_t outer0 = inner0 + static_cast<std::uint32_t>(columns);
            const std::uint32_t outer1 = outer0 + 1U;
            if (samples.radii[row] <= LarZoneMeshLimits::SampleTolerance) {
                appender.triangle(inner0, outer0, outer1);
            } else {
                appender.triangle(inner0, outer0, inner1);
                appender.triangle(outer0, outer1, inner1);
            }
        }
    }
    fillRange.indexCount = mesh.indices.size() - fillRange.firstIndex;

    lineRange.firstIndex = mesh.indices.size();
    const auto outerFirst = firstVertex + static_cast<std::uint32_t>((rows - 1U) * columns);
    appender.lineStrip(outerFirst, static_cast<int>(columns));
    if (zone.innerRadiusMeters > LarZoneMeshLimits::SampleTolerance) {
        appender.lineStrip(firstVertex, static_cast<int>(columns));
    }
    if (!samples.fullCircle) {
        appendRadialLine(appender, firstVertex, rows, columns, 0U, 0U);
        appendRadialLine(appender, firstVertex, rows, columns, columns - 1U, 0U);
    } else if (mesh.coordinateSpace == LarZoneCoordinateSpace::MercatorCameraRelative &&
               zone.outerRadiusMeters >
                   samples.seamPoleDistanceMeters + LarZoneMeshLimits::SampleTolerance) {
        const auto seamStart =
            std::lower_bound(samples.radii.begin(), samples.radii.end(),
                             std::max(zone.innerRadiusMeters, samples.seamPoleDistanceMeters) -
                                 LarZoneMeshLimits::SampleTolerance);
        const std::size_t firstSeamRow =
            static_cast<std::size_t>(std::distance(samples.radii.begin(), seamStart));
        appendRadialLine(appender, firstVertex, rows, columns, 0U, firstSeamRow);
        appendRadialLine(appender, firstVertex, rows, columns, columns - 1U, firstSeamRow);
    }
    lineRange.indexCount = mesh.indices.size() - lineRange.firstIndex;
    return !appender.failed();
}
