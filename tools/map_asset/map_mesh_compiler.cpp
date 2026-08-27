
#include "map_mesh_compiler.h"

#include "polygon_triangulator.h"
#include "viewer/map/map_asset_limits.h"
#include "viewer/map/map_land_index.h"
#include "viewer/map/map_projection.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace lar::map::tool {
namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double MaximumSphereEdgeDegrees = 2.0;
constexpr double MaximumSphereChordError = 0.0005;
constexpr std::size_t MaximumRefinementPasses = 10U;

std::array<double, 3> unitSphere(const SourceCoordinate &coordinate) {
    const double longitude = coordinate.longitudeDegrees * Pi / 180.0;
    const double latitude = coordinate.latitudeDegrees * Pi / 180.0;
    const double cosineLatitude = std::cos(latitude);
    return {cosineLatitude * std::sin(longitude), std::sin(latitude),
            cosineLatitude * std::cos(longitude)};
}

bool edgeNeedsRefinement(const SourceCoordinate &first, const SourceCoordinate &second) {
    const std::array<double, 3> firstUnit = unitSphere(first);
    const std::array<double, 3> secondUnit = unitSphere(second);
    const double dot = std::clamp(firstUnit[0] * secondUnit[0] + firstUnit[1] * secondUnit[1] +
                                      firstUnit[2] * secondUnit[2],
                                  -1.0, 1.0);
    if (std::acos(dot) * 180.0 / Pi > MaximumSphereEdgeDegrees) {
        return true;
    }

    const SourceCoordinate midpoint{(first.longitudeDegrees + second.longitudeDegrees) * 0.5,
                                    (first.latitudeDegrees + second.latitudeDegrees) * 0.5};
    const std::array<double, 3> midpointUnit = unitSphere(midpoint);
    const double deltaX = midpointUnit[0] - (firstUnit[0] + secondUnit[0]) * 0.5;
    const double deltaY = midpointUnit[1] - (firstUnit[1] + secondUnit[1]) * 0.5;
    const double deltaZ = midpointUnit[2] - (firstUnit[2] + secondUnit[2]) * 0.5;
    return std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ) > MaximumSphereChordError;
}

std::uint64_t edgeKey(std::uint32_t first, std::uint32_t second) {
    const std::uint32_t low = std::min(first, second);
    const std::uint32_t high = std::max(first, second);
    return (static_cast<std::uint64_t>(low) << 32U) | static_cast<std::uint64_t>(high);
}

bool appendIndices(std::vector<std::uint32_t> &destination,
                   std::initializer_list<std::uint32_t> indices, std::size_t maximum) {
    if (indices.size() > maximum || destination.size() > maximum - indices.size()) {
        return false;
    }
    destination.insert(destination.end(), indices.begin(), indices.end());
    return true;
}

bool refineForSphere(PlanarPolygonMesh &mesh, std::size_t remainingVertexCount,
                     std::size_t remainingIndexCount) {
    if (mesh.empty() || mesh.fillIndices.size() % 3U != 0U ||
        mesh.coordinates.size() > remainingVertexCount ||
        mesh.fillIndices.size() > remainingIndexCount) {
        return false;
    }

    std::vector<std::uint32_t> current = mesh.fillIndices;
    for (std::size_t pass = 0U; pass < MaximumRefinementPasses; ++pass) {
        bool subdivided = false;
        std::vector<std::uint32_t> next;
        next.reserve(std::min(remainingIndexCount, current.size() <= remainingIndexCount / 2U
                                                       ? current.size() * 2U
                                                       : remainingIndexCount));
        std::unordered_map<std::uint64_t, std::uint32_t> midpoints;
        midpoints.reserve(current.size() / 2U);
        bool limitExceeded = false;

        const auto midpointIndex = [&](std::uint32_t first, std::uint32_t second) {
            const std::uint64_t key = edgeKey(first, second);
            const auto existing = midpoints.find(key);
            if (existing != midpoints.end()) {
                return existing->second;
            }
            if (mesh.coordinates.size() >= remainingVertexCount ||
                mesh.coordinates.size() >=
                    static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                limitExceeded = true;
                return first;
            }
            const SourceCoordinate &a = mesh.coordinates[first];
            const SourceCoordinate &b = mesh.coordinates[second];
            const std::uint32_t result = static_cast<std::uint32_t>(mesh.coordinates.size());
            mesh.coordinates.push_back({(a.longitudeDegrees + b.longitudeDegrees) * 0.5,
                                        (a.latitudeDegrees + b.latitudeDegrees) * 0.5});
            midpoints.emplace(key, result);
            return result;
        };

        for (std::size_t offset = 0U; offset < current.size(); offset += 3U) {
            const std::uint32_t a = current[offset];
            const std::uint32_t b = current[offset + 1U];
            const std::uint32_t c = current[offset + 2U];
            if (a >= mesh.coordinates.size() || b >= mesh.coordinates.size() ||
                c >= mesh.coordinates.size()) {
                return false;
            }
            const bool splitAb = edgeNeedsRefinement(mesh.coordinates[a], mesh.coordinates[b]);
            const bool splitBc = edgeNeedsRefinement(mesh.coordinates[b], mesh.coordinates[c]);
            const bool splitCa = edgeNeedsRefinement(mesh.coordinates[c], mesh.coordinates[a]);
            const unsigned mask = (splitAb ? 1U : 0U) | (splitBc ? 2U : 0U) | (splitCa ? 4U : 0U);
            if (mask == 0U) {
                if (!appendIndices(next, {a, b, c}, remainingIndexCount)) {
                    return false;
                }
                continue;
            }

            subdivided = true;
            const std::uint32_t ab = splitAb ? midpointIndex(a, b) : 0U;
            const std::uint32_t bc = splitBc ? midpointIndex(b, c) : 0U;
            const std::uint32_t ca = splitCa ? midpointIndex(c, a) : 0U;
            if (limitExceeded) {
                return false;
            }
            bool appended = false;
            switch (mask) {
            case 1U:
                appended = appendIndices(next, {a, ab, c, ab, b, c}, remainingIndexCount);
                break;
            case 2U:
                appended = appendIndices(next, {a, b, bc, a, bc, c}, remainingIndexCount);
                break;
            case 3U:
                appended =
                    appendIndices(next, {b, bc, ab, a, ab, c, ab, bc, c}, remainingIndexCount);
                break;
            case 4U:
                appended = appendIndices(next, {a, b, ca, ca, b, c}, remainingIndexCount);
                break;
            case 5U:
                appended =
                    appendIndices(next, {a, ab, ca, ab, b, ca, b, c, ca}, remainingIndexCount);
                break;
            case 6U:
                appended =
                    appendIndices(next, {c, ca, bc, a, b, ca, b, bc, ca}, remainingIndexCount);
                break;
            case 7U:
                appended = appendIndices(next, {a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca},
                                         remainingIndexCount);
                break;
            default:
                break;
            }
            if (!appended) {
                return false;
            }
        }

        current = std::move(next);
        if (!subdivided) {
            break;
        }
    }
    mesh.fillIndices = std::move(current);
    return true;
}

bool checkedAppendCount(std::size_t current, std::size_t addition, std::size_t maximum) {
    return addition <= maximum && current <= maximum - addition;
}

int normalizedLongitudeCell(int longitudeDegrees) noexcept {
    int normalized = (longitudeDegrees + 180) % static_cast<int>(MapLandLongitudeCellCount);
    if (normalized < 0) {
        normalized += static_cast<int>(MapLandLongitudeCellCount);
    }
    return normalized;
}

int boundedLatitudeCell(double latitudeDegrees) noexcept {
    if (latitudeDegrees >= 90.0) {
        return static_cast<int>(MapLandLatitudeCellCount) - 1;
    }
    return std::clamp(static_cast<int>(std::floor(latitudeDegrees)) + 90, 0,
                      static_cast<int>(MapLandLatitudeCellCount) - 1);
}

template <typename Callback>
bool forEachTriangleCell(const MapMesh &mesh, std::size_t triangle, Callback callback) {
    const std::size_t indexOffset = triangle * 3U;
    if (indexOffset + 2U >= mesh.mercatorFillIndices.size()) {
        return false;
    }
    std::array<double, 3> longitude{};
    std::array<double, 3> latitude{};
    for (std::size_t corner = 0U; corner < 3U; ++corner) {
        const std::size_t vertex = mesh.mercatorFillIndices[indexOffset + corner];
        const std::size_t vertexOffset = vertex * 3U;
        if (vertexOffset + 1U >= mesh.vertices.size()) {
            return false;
        }
        longitude[corner] = mesh.vertices[vertexOffset];
        latitude[corner] = mesh.vertices[vertexOffset + 1U];
    }
    longitude[1] = MapProjection::unwrapLongitude(longitude[1], longitude[0]);
    longitude[2] = MapProjection::unwrapLongitude(longitude[2], longitude[0]);
    const auto [minimumLongitude, maximumLongitude] =
        std::minmax_element(longitude.cbegin(), longitude.cend());
    const auto [minimumLatitude, maximumLatitude] =
        std::minmax_element(latitude.cbegin(), latitude.cend());
    if (*maximumLongitude - *minimumLongitude > 360.0 || *minimumLatitude < -90.0 ||
        *maximumLatitude > 90.0) {
        return false;
    }

    const int west = static_cast<int>(std::floor(*minimumLongitude));
    const int east = static_cast<int>(std::floor(*maximumLongitude));
    const int south = boundedLatitudeCell(*minimumLatitude);
    const int north = boundedLatitudeCell(*maximumLatitude);
    for (int latitudeCell = south; latitudeCell <= north; ++latitudeCell) {
        for (int longitudeCell = west; longitudeCell <= east; ++longitudeCell) {
            const std::size_t index = static_cast<std::size_t>(latitudeCell) *
                                          MapLandLongitudeCellCount +
                                      static_cast<std::size_t>(
                                          normalizedLongitudeCell(longitudeCell));
            if (!callback(index)) {
                return false;
            }
        }
    }
    return true;
}

bool buildLandIndex(MapMesh &mesh) {
    if (mesh.mercatorFillIndices.empty() || mesh.mercatorFillIndices.size() % 3U != 0U) {
        return false;
    }
    std::vector<std::uint32_t> counts(MapLandCellCount, 0U);
    std::size_t referenceCount = 0U;
    const std::size_t triangleCount = mesh.mercatorFillIndices.size() / 3U;
    for (std::size_t triangle = 0U; triangle < triangleCount; ++triangle) {
        const bool counted = forEachTriangleCell(mesh, triangle, [&](std::size_t cell) {
            if (counts[cell] == std::numeric_limits<std::uint32_t>::max() ||
                referenceCount >= limits::MaximumLandTriangleReferenceCount) {
                return false;
            }
            ++counts[cell];
            ++referenceCount;
            return true;
        });
        if (!counted) {
            return false;
        }
    }

    mesh.landCellRanges.resize(MapLandCellCount);
    std::size_t firstReference = 0U;
    for (std::size_t cell = 0U; cell < MapLandCellCount; ++cell) {
        if (firstReference > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        mesh.landCellRanges[cell] = {static_cast<std::uint32_t>(firstReference), counts[cell]};
        firstReference += counts[cell];
    }
    if (firstReference != referenceCount) {
        return false;
    }

    mesh.landTriangleReferences.resize(referenceCount);
    std::vector<std::uint32_t> cursors(MapLandCellCount, 0U);
    for (std::size_t cell = 0U; cell < MapLandCellCount; ++cell) {
        cursors[cell] = mesh.landCellRanges[cell].firstReference;
    }
    for (std::size_t triangle = 0U; triangle < triangleCount; ++triangle) {
        const bool stored = forEachTriangleCell(mesh, triangle, [&](std::size_t cell) {
            const std::size_t destination = cursors[cell]++;
            if (destination >= mesh.landTriangleReferences.size() ||
                triangle > std::numeric_limits<std::uint32_t>::max()) {
                return false;
            }
            mesh.landTriangleReferences[destination] = static_cast<std::uint32_t>(triangle);
            return true;
        });
        if (!stored) {
            return false;
        }
    }
    return mapLandIndexIsValid(mesh);
}

} // namespace

MapMeshCompileResult MapMeshCompiler::compile(const SourceMap &source) {
    if (source.empty()) {
        return {{}, QStringLiteral("The map source contains no geometry.")};
    }

    MapMesh output;
    try {
        output.vertices.reserve(std::min(source.coordinateCount, limits::MaximumVertexCount) * 3U);
        output.mercatorFillIndices.reserve(
            std::min(source.coordinateCount * 3U, limits::MaximumMercatorIndexCount));
        output.sphereFillIndices.reserve(
            std::min(source.coordinateCount * 4U, limits::MaximumSphereIndexCount));
        output.borderIndices.reserve(
            std::min(source.coordinateCount * 2U, limits::MaximumBorderIndexCount));

        for (std::size_t polygonIndex = 0; polygonIndex < source.polygons.size(); ++polygonIndex) {
            const SourcePolygon &polygon = source.polygons[polygonIndex];
            PlanarPolygonMesh planar = PolygonTriangulator::triangulate(polygon);
            if (planar.empty()) {
                return {{},
                        QStringLiteral("Source polygon %1 could not be triangulated completely.")
                            .arg(polygonIndex)};
            }
            const std::vector<std::uint32_t> mercatorIndices = planar.fillIndices;
            const std::size_t usedVertices = output.vertexCount();
            if (usedVertices > limits::MaximumVertexCount ||
                !refineForSphere(planar, limits::MaximumVertexCount - usedVertices,
                                 limits::MaximumSphereIndexCount -
                                     output.sphereFillIndices.size()) ||
                !checkedAppendCount(output.mercatorFillIndices.size(), mercatorIndices.size(),
                                    limits::MaximumMercatorIndexCount) ||
                !checkedAppendCount(output.sphereFillIndices.size(), planar.fillIndices.size(),
                                    limits::MaximumSphereIndexCount) ||
                !checkedAppendCount(output.borderIndices.size(), planar.borderIndices.size(),
                                    limits::MaximumBorderIndexCount)) {
                return {{}, QStringLiteral("Map triangulation exceeded its resource budget.")};
            }

            const std::uint32_t baseVertex = static_cast<std::uint32_t>(usedVertices);
            for (const SourceCoordinate &coordinate : planar.coordinates) {
                const double mercatorY =
                    lar::map::MapProjection::projectLatitude(coordinate.latitudeDegrees);
                if (!std::isfinite(coordinate.longitudeDegrees) ||
                    !std::isfinite(coordinate.latitudeDegrees) || !std::isfinite(mercatorY) ||
                    std::abs(coordinate.longitudeDegrees) > limits::MaximumAbsoluteLongitude ||
                    std::abs(coordinate.latitudeDegrees) > limits::MaximumAbsoluteLatitude ||
                    std::abs(mercatorY) > limits::MaximumAbsoluteMercatorY) {
                    return {{},
                            QStringLiteral("Map triangulation produced "
                                           "an invalid coordinate.")};
                }
                output.vertices.push_back(static_cast<float>(coordinate.longitudeDegrees));
                output.vertices.push_back(static_cast<float>(coordinate.latitudeDegrees));
                output.vertices.push_back(static_cast<float>(mercatorY));
            }
            for (const std::uint32_t index : mercatorIndices) {
                output.mercatorFillIndices.push_back(baseVertex + index);
            }
            for (const std::uint32_t index : planar.fillIndices) {
                output.sphereFillIndices.push_back(baseVertex + index);
            }
            for (const std::uint32_t index : planar.borderIndices) {
                output.borderIndices.push_back(baseVertex + index);
            }
        }
    } catch (const std::bad_alloc &) {
        return {{}, QStringLiteral("There is not enough memory to compile the map.")};
    } catch (const std::length_error &) {
        return {{}, QStringLiteral("The compiled map dimensions are invalid.")};
    }

    if (output.empty() || !buildLandIndex(output) ||
        output.byteSize() > limits::MaximumPayloadBytes) {
        return {{}, QStringLiteral("Map compilation produced no valid bounded mesh.")};
    }
    return {std::move(output), {}};
}

} // namespace lar::map::tool
