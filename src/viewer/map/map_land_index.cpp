#include "viewer/map/map_land_index.h"

#include "viewer/map/map_projection.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace lar::map {
namespace {

constexpr double CoordinateEpsilon = 1.0e-10;

int wrappedLongitudeCell(double longitudeDegrees) noexcept {
    const double wrapped = MapProjection::wrapLongitude(longitudeDegrees);
    const int cell = static_cast<int>(std::floor(wrapped)) + 180;
    return std::clamp(cell, 0, static_cast<int>(MapLandLongitudeCellCount) - 1);
}

int latitudeCell(double latitudeDegrees) noexcept {
    const double bounded = std::clamp(latitudeDegrees, -90.0, 90.0);
    const int cell = bounded >= 90.0 ? static_cast<int>(MapLandLatitudeCellCount) - 1
                                    : static_cast<int>(std::floor(bounded)) + 90;
    return std::clamp(cell, 0, static_cast<int>(MapLandLatitudeCellCount) - 1);
}

std::size_t cellIndex(int longitudeCell, int latitudeCellValue) noexcept {
    return static_cast<std::size_t>(latitudeCellValue) * MapLandLongitudeCellCount +
           static_cast<std::size_t>(longitudeCell);
}

double cross(double ax, double ay, double bx, double by, double px, double py) noexcept {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

bool triangleContains(const MapMesh &mesh, std::uint32_t triangle, double latitudeDegrees,
                      double longitudeDegrees) noexcept {
    const std::size_t indexOffset = static_cast<std::size_t>(triangle) * 3U;
    if (indexOffset + 2U >= mesh.mercatorFillIndices.size()) {
        return false;
    }

    double longitude[3]{};
    double latitude[3]{};
    for (std::size_t corner = 0U; corner < 3U; ++corner) {
        const std::size_t vertex = mesh.mercatorFillIndices[indexOffset + corner];
        const std::size_t vertexOffset = vertex * 3U;
        if (vertexOffset + 1U >= mesh.vertices.size()) {
            return false;
        }
        longitude[corner] =
            MapProjection::unwrapLongitude(mesh.vertices[vertexOffset], longitudeDegrees);
        latitude[corner] = mesh.vertices[vertexOffset + 1U];
    }

    const double first = cross(longitude[0], latitude[0], longitude[1], latitude[1],
                               longitudeDegrees, latitudeDegrees);
    const double second = cross(longitude[1], latitude[1], longitude[2], latitude[2],
                                longitudeDegrees, latitudeDegrees);
    const double third = cross(longitude[2], latitude[2], longitude[0], latitude[0],
                               longitudeDegrees, latitudeDegrees);
    const bool negative = first < -CoordinateEpsilon || second < -CoordinateEpsilon ||
                          third < -CoordinateEpsilon;
    const bool positive = first > CoordinateEpsilon || second > CoordinateEpsilon ||
                          third > CoordinateEpsilon;
    return !(negative && positive);
}

} // namespace

bool mapLandIndexIsValid(const MapMesh &mesh) noexcept {
    if (!mesh.hasLandIndex() ||
        mesh.landTriangleReferences.size() >
            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
        mesh.mercatorFillIndices.size() % 3U != 0U) {
        return false;
    }
    const std::size_t triangleCount = mesh.mercatorFillIndices.size() / 3U;
    std::size_t expectedFirst = 0U;
    for (const MapLandCellRange &range : mesh.landCellRanges) {
        if (expectedFirst > mesh.landTriangleReferences.size() ||
            range.firstReference != expectedFirst ||
            range.referenceCount > mesh.landTriangleReferences.size() - expectedFirst) {
            return false;
        }
        expectedFirst += range.referenceCount;
    }
    if (expectedFirst != mesh.landTriangleReferences.size()) {
        return false;
    }
    return std::all_of(mesh.landTriangleReferences.cbegin(),
                       mesh.landTriangleReferences.cend(), [triangleCount](std::uint32_t triangle) {
                           return static_cast<std::size_t>(triangle) < triangleCount;
                       });
}

MapLandIndex::MapLandIndex(std::shared_ptr<const MapMesh> mesh) : m_mesh(std::move(mesh)) {
    m_valid = m_mesh != nullptr && mapLandIndexIsValid(*m_mesh);
}

bool MapLandIndex::isValid() const noexcept {
    return m_valid;
}

bool MapLandIndex::contains(double latitudeDegrees, double longitudeDegrees) const noexcept {
    if (!isValid() || !std::isfinite(latitudeDegrees) || !std::isfinite(longitudeDegrees) ||
        latitudeDegrees < -90.0 || latitudeDegrees > 90.0) {
        return false;
    }
    const double wrappedLongitude = MapProjection::wrapLongitude(longitudeDegrees);
    const MapLandCellRange &range =
        m_mesh->landCellRanges[cellIndex(wrappedLongitudeCell(wrappedLongitude),
                                         latitudeCell(latitudeDegrees))];
    const std::size_t begin = range.firstReference;
    const std::size_t end = begin + range.referenceCount;
    for (std::size_t reference = begin; reference < end; ++reference) {
        if (triangleContains(*m_mesh, m_mesh->landTriangleReferences[reference], latitudeDegrees,
                             wrappedLongitude)) {
            return true;
        }
    }
    return false;
}

void MapLandIndex::appendCandidates(const MapGeographicBounds &bounds,
                                    std::vector<std::uint32_t> &destination) const {
    if (!isValid() || !std::isfinite(bounds.westDegrees) ||
        !std::isfinite(bounds.eastDegrees) || !std::isfinite(bounds.southDegrees) ||
        !std::isfinite(bounds.northDegrees) || bounds.westDegrees > bounds.eastDegrees ||
        bounds.southDegrees > bounds.northDegrees ||
        bounds.eastDegrees - bounds.westDegrees > 360.0 || bounds.northDegrees < -90.0 ||
        bounds.southDegrees > 90.0) {
        return;
    }

    const int south = latitudeCell(std::max(-90.0, bounds.southDegrees));
    const int north = latitudeCell(std::min(90.0, bounds.northDegrees));
    const int west = static_cast<int>(std::floor(bounds.westDegrees));
    const int east = static_cast<int>(std::floor(bounds.eastDegrees));
    for (int latitudeIndex = south; latitudeIndex <= north; ++latitudeIndex) {
        for (int longitude = west; longitude <= east; ++longitude) {
            const MapLandCellRange &range =
                m_mesh->landCellRanges[cellIndex(wrappedLongitudeCell(longitude), latitudeIndex)];
            const std::size_t begin = range.firstReference;
            destination.insert(destination.end(),
                               m_mesh->landTriangleReferences.begin() +
                                   static_cast<std::ptrdiff_t>(begin),
                               m_mesh->landTriangleReferences.begin() +
                                   static_cast<std::ptrdiff_t>(begin + range.referenceCount));
        }
    }
}

} // namespace lar::map
