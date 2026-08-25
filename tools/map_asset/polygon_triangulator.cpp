
#include "polygon_triangulator.h"

#include "viewer/map/map_projection.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace lar::map::tool {
namespace {

constexpr double CoordinateTolerance = 1.0e-10;
constexpr double AreaTolerance = 1.0e-12;

bool samePoint(const SourceCoordinate &first, const SourceCoordinate &second) {
    return std::abs(first.longitudeDegrees - second.longitudeDegrees) <= CoordinateTolerance &&
           std::abs(first.latitudeDegrees - second.latitudeDegrees) <= CoordinateTolerance;
}

SourceRing prepareRing(const SourceRing &source, double referenceLongitude) {
    SourceRing ring;
    ring.reserve(source.size());
    double previousLongitude = referenceLongitude;
    for (const SourceCoordinate &coordinate : source) {
        SourceCoordinate unwrapped{lar::map::MapProjection::unwrapLongitude(
                                       coordinate.longitudeDegrees, previousLongitude),
                                   coordinate.latitudeDegrees};
        previousLongitude = unwrapped.longitudeDegrees;
        if (ring.empty() || !samePoint(ring.back(), unwrapped)) {
            ring.push_back(unwrapped);
        }
    }
    if (ring.size() > 1U && samePoint(ring.front(), ring.back())) {
        ring.pop_back();
    }
    if (ring.size() < 3U) {
        return {};
    }

    const double shift =
        360.0 * std::round((referenceLongitude - ring.front().longitudeDegrees) / 360.0);
    for (SourceCoordinate &coordinate : ring) {
        coordinate.longitudeDegrees += shift;
    }
    return ring;
}

double signedArea(const SourceRing &ring) {
    double twiceArea = 0.0;
    for (std::size_t index = 0U; index < ring.size(); ++index) {
        const SourceCoordinate &current = ring[index];
        const SourceCoordinate &next = ring[(index + 1U) % ring.size()];
        twiceArea += current.longitudeDegrees * next.latitudeDegrees -
                     next.longitudeDegrees * current.latitudeDegrees;
    }
    return twiceArea * 0.5;
}

struct RingNode final {
    std::uint32_t vertex = 0U;
    double x = 0.0;
    double y = 0.0;
    RingNode *previous = nullptr;
    RingNode *next = nullptr;
};

class NodePool final {
  public:
    RingNode *append(RingNode *last, std::uint32_t vertex, const SourceCoordinate &coordinate) {
        auto node = std::make_unique<RingNode>();
        node->vertex = vertex;
        node->x = coordinate.longitudeDegrees;
        node->y = coordinate.latitudeDegrees;
        RingNode *created = node.get();
        m_nodes.push_back(std::move(node));
        if (last == nullptr) {
            created->previous = created;
            created->next = created;
        } else {
            created->next = last->next;
            created->previous = last;
            last->next->previous = created;
            last->next = created;
        }
        return created;
    }

    RingNode *duplicate(RingNode &source) {
        auto node = std::make_unique<RingNode>();
        node->vertex = source.vertex;
        node->x = source.x;
        node->y = source.y;
        RingNode *created = node.get();
        m_nodes.push_back(std::move(node));
        return created;
    }

  private:
    std::vector<std::unique_ptr<RingNode>> m_nodes;
};
double cross(const RingNode &first, const RingNode &middle, const RingNode &last) {
    return (middle.x - first.x) * (last.y - first.y) - (middle.y - first.y) * (last.x - first.x);
}

void detach(RingNode &node) {
    node.previous->next = node.next;
    node.next->previous = node.previous;
}

RingNode *removeRedundantNodes(RingNode *start) {
    if (start == nullptr) {
        return nullptr;
    }
    RingNode *node = start;
    bool changed = false;
    do {
        changed = false;
        RingNode *next = node->next;
        const bool duplicate = std::abs(node->x - next->x) <= CoordinateTolerance &&
                               std::abs(node->y - next->y) <= CoordinateTolerance;
        const bool collinear =
            std::abs(cross(*node->previous, *node, *node->next)) <= AreaTolerance;
        if (node != node->next && node->previous != node->next && (duplicate || collinear)) {
            if (node == start) {
                start = next;
            }
            detach(*node);
            node = next;
            changed = true;
        } else {
            node = next;
        }
    } while (changed || node != start);
    return start;
}

bool pointInsideTriangle(const RingNode &a, const RingNode &b, const RingNode &c,
                         const RingNode &point) {
    const double ab = cross(a, b, point);
    const double bc = cross(b, c, point);
    const double ca = cross(c, a, point);
    return ab >= -AreaTolerance && bc >= -AreaTolerance && ca >= -AreaTolerance;
}

bool isEar(const RingNode &candidate) {
    const RingNode &a = *candidate.previous;
    const RingNode &b = candidate;
    const RingNode &c = *candidate.next;
    if (cross(a, b, c) <= AreaTolerance) {
        return false;
    }

    const RingNode *point = candidate.next->next;
    while (point != candidate.previous) {
        if (pointInsideTriangle(a, b, c, *point) &&
            cross(*point->previous, *point, *point->next) <= AreaTolerance) {
            return false;
        }
        point = point->next;
    }
    return true;
}

int orientation(double ax, double ay, double bx, double by, double cx, double cy) {
    const double value = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (std::abs(value) <= AreaTolerance) {
        return 0;
    }
    return value > 0.0 ? 1 : -1;
}

bool segmentsIntersect(const RingNode &a, const RingNode &b, const RingNode &c, const RingNode &d) {
    const int first = orientation(a.x, a.y, b.x, b.y, c.x, c.y);
    const int second = orientation(a.x, a.y, b.x, b.y, d.x, d.y);
    const int third = orientation(c.x, c.y, d.x, d.y, a.x, a.y);
    const int fourth = orientation(c.x, c.y, d.x, d.y, b.x, b.y);
    return first != 0 && second != 0 && third != 0 && fourth != 0 && first != second &&
           third != fourth;
}

bool bridgeCrossesRing(const RingNode &first, const RingNode &second, const RingNode &ring) {
    const RingNode *edge = &ring;
    do {
        const RingNode *next = edge->next;
        const bool sharesEndpoint =
            edge == &first || edge == &second || next == &first || next == &second;
        if (!sharesEndpoint && segmentsIntersect(first, second, *edge, *next)) {
            return true;
        }
        edge = next;
    } while (edge != &ring);
    return false;
}

bool pointInsideRing(double x, double y, const RingNode &ring) {
    bool inside = false;
    const RingNode *first = &ring;
    do {
        const RingNode *second = first->next;
        if ((first->y > y) != (second->y > y)) {
            const double intersectionX =
                first->x + (second->x - first->x) * (y - first->y) / (second->y - first->y);
            if (x < intersectionX) {
                inside = !inside;
            }
        }
        first = second;
    } while (first != &ring);
    return inside;
}

RingNode *leftmost(RingNode &ring) {
    RingNode *result = &ring;
    RingNode *node = ring.next;
    while (node != &ring) {
        if (node->x < result->x || (node->x == result->x && node->y < result->y)) {
            result = node;
        }
        node = node->next;
    }
    return result;
}

RingNode *joinHole(RingNode &outer, RingNode &hole, NodePool &pool) {
    RingNode *holeAnchor = leftmost(hole);
    RingNode *outerAnchor = nullptr;
    double bestDistance = std::numeric_limits<double>::max();
    RingNode *candidate = &outer;
    do {
        const double dx = candidate->x - holeAnchor->x;
        const double dy = candidate->y - holeAnchor->y;
        const double distance = dx * dx + dy * dy;
        const double midpointX = (candidate->x + holeAnchor->x) * 0.5;
        const double midpointY = (candidate->y + holeAnchor->y) * 0.5;
        if (distance < bestDistance && !bridgeCrossesRing(*candidate, *holeAnchor, outer) &&
            !bridgeCrossesRing(*candidate, *holeAnchor, hole) &&
            pointInsideRing(midpointX, midpointY, outer) &&
            !pointInsideRing(midpointX, midpointY, hole)) {
            outerAnchor = candidate;
            bestDistance = distance;
        }
        candidate = candidate->next;
    } while (candidate != &outer);
    if (outerAnchor == nullptr) {
        return nullptr;
    }

    RingNode *outerAfter = outerAnchor->next;
    RingNode *holeBefore = holeAnchor->previous;
    RingNode *outerCopy = pool.duplicate(*outerAnchor);
    RingNode *holeCopy = pool.duplicate(*holeAnchor);

    outerAnchor->next = holeAnchor;
    holeAnchor->previous = outerAnchor;
    holeBefore->next = holeCopy;
    holeCopy->previous = holeBefore;
    holeCopy->next = outerCopy;
    outerCopy->previous = holeCopy;
    outerCopy->next = outerAfter;
    outerAfter->previous = outerCopy;
    return outerAnchor;
}

RingNode *makeRing(const SourceRing &ring, std::uint32_t firstVertex, bool counterClockwise,
                   NodePool &pool) {
    const bool reverse = (signedArea(ring) > 0.0) != counterClockwise;
    RingNode *last = nullptr;
    RingNode *first = nullptr;
    for (std::size_t offset = 0U; offset < ring.size(); ++offset) {
        const std::size_t sourceIndex = reverse ? ring.size() - 1U - offset : offset;
        last = pool.append(last, firstVertex + static_cast<std::uint32_t>(sourceIndex),
                           ring[sourceIndex]);
        if (first == nullptr) {
            first = last;
        }
    }
    return first;
}

RingNode *clonePath(const std::vector<RingNode *> &nodes, std::size_t begin, std::size_t end,
                    NodePool &pool) {
    RingNode *first = nullptr;
    RingNode *last = nullptr;
    for (std::size_t index = begin; index <= end; ++index) {
        const RingNode &source = *nodes[index];
        last = pool.append(last, source.vertex, {source.x, source.y});
        if (first == nullptr) {
            first = last;
        }
    }
    return first;
}

bool triangulateLinkedRing(RingNode *start, std::vector<std::uint32_t> &indices, NodePool &pool,
                           int depth = 0);

bool splitAndTriangulate(RingNode &start, std::vector<std::uint32_t> &indices, NodePool &pool,
                         int depth) {
    if (depth >= 16) {
        return false;
    }
    std::vector<RingNode *> nodes;
    RingNode *node = &start;
    do {
        nodes.push_back(node);
        node = node->next;
    } while (node != &start);
    if (nodes.size() < 4U) {
        return false;
    }

    for (std::size_t first = 0U; first + 2U < nodes.size(); ++first) {
        for (std::size_t second = first + 2U; second < nodes.size(); ++second) {
            if (first == 0U && second + 1U == nodes.size()) {
                continue;
            }
            RingNode &a = *nodes[first];
            RingNode &b = *nodes[second];
            const double midpointX = (a.x + b.x) * 0.5;
            const double midpointY = (a.y + b.y) * 0.5;
            if (samePoint({a.x, a.y}, {b.x, b.y}) || bridgeCrossesRing(a, b, start) ||
                !pointInsideRing(midpointX, midpointY, start)) {
                continue;
            }

            RingNode *firstRing = clonePath(nodes, first, second, pool);
            std::vector<RingNode *> wrapped;
            wrapped.reserve(nodes.size() - second + first + 1U);
            wrapped.insert(wrapped.end(), nodes.begin() + static_cast<std::ptrdiff_t>(second),
                           nodes.end());
            wrapped.insert(wrapped.end(), nodes.begin(),
                           nodes.begin() + static_cast<std::ptrdiff_t>(first + 1U));
            RingNode *secondRing = clonePath(wrapped, 0U, wrapped.size() - 1U, pool);
            return triangulateLinkedRing(firstRing, indices, pool, depth + 1) &&
                   triangulateLinkedRing(secondRing, indices, pool, depth + 1);
        }
    }
    return false;
}

bool triangulateLinkedRing(RingNode *start, std::vector<std::uint32_t> &indices, NodePool &pool,
                           int depth) {
    start = removeRedundantNodes(start);
    if (start == nullptr) {
        return false;
    }
    RingNode *candidate = start;
    RingNode *stop = start;
    std::size_t stalledPasses = 0U;
    while (candidate->previous != candidate->next) {
        RingNode *next = candidate->next;
        if (isEar(*candidate)) {
            indices.push_back(candidate->previous->vertex);
            indices.push_back(candidate->vertex);
            indices.push_back(candidate->next->vertex);
            detach(*candidate);
            candidate = next->next;
            stop = candidate;
            stalledPasses = 0U;
            continue;
        }
        candidate = next;
        if (candidate == stop) {
            candidate = removeRedundantNodes(candidate);
            stop = candidate;
            if (++stalledPasses > 1U) {
                return candidate != nullptr &&
                       splitAndTriangulate(*candidate, indices, pool, depth);
            }
        }
    }
    return true;
}

} // namespace

PlanarPolygonMesh PolygonTriangulator::triangulate(const SourcePolygon &polygon) {
    PlanarPolygonMesh mesh;
    if (polygon.exterior.empty()) {
        return mesh;
    }
    const double referenceLongitude = polygon.exterior.front().longitudeDegrees;
    SourceRing exterior = prepareRing(polygon.exterior, referenceLongitude);
    if (exterior.size() < 3U) {
        return mesh;
    }

    std::vector<SourceRing> holes;
    holes.reserve(polygon.holes.size());
    std::size_t coordinateCount = exterior.size();
    for (const SourceRing &sourceHole : polygon.holes) {
        SourceRing hole = prepareRing(sourceHole, referenceLongitude);
        if (hole.size() >= 3U) {
            coordinateCount += hole.size();
            holes.push_back(std::move(hole));
        }
    }
    if (coordinateCount > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return {};
    }

    mesh.coordinates.reserve(coordinateCount);
    mesh.borderIndices.reserve(coordinateCount * 2U);
    const auto appendCoordinates = [&mesh](const SourceRing &ring) {
        const std::uint32_t first = static_cast<std::uint32_t>(mesh.coordinates.size());
        mesh.coordinates.insert(mesh.coordinates.end(), ring.begin(), ring.end());
        for (std::size_t index = 0U; index < ring.size(); ++index) {
            mesh.borderIndices.push_back(first + static_cast<std::uint32_t>(index));
            mesh.borderIndices.push_back(first +
                                         static_cast<std::uint32_t>((index + 1U) % ring.size()));
        }
        return first;
    };

    NodePool pool;
    const std::uint32_t exteriorFirst = appendCoordinates(exterior);
    RingNode *outer = makeRing(exterior, exteriorFirst, true, pool);
    struct PendingHole final {
        RingNode *ring = nullptr;
        double leftmostX = 0.0;
    };
    std::vector<PendingHole> pendingHoles;
    pendingHoles.reserve(holes.size());
    for (const SourceRing &hole : holes) {
        const std::uint32_t holeFirst = appendCoordinates(hole);
        RingNode *holeRing = makeRing(hole, holeFirst, false, pool);
        pendingHoles.push_back({holeRing, leftmost(*holeRing)->x});
    }
    std::sort(pendingHoles.begin(), pendingHoles.end(),
              [](const PendingHole &first, const PendingHole &second) {
                  return first.leftmostX < second.leftmostX;
              });
    for (const PendingHole &hole : pendingHoles) {
        outer = joinHole(*outer, *hole.ring, pool);
        if (outer == nullptr) {
            return {};
        }
    }

    mesh.fillIndices.reserve(mesh.coordinates.size() >= 3U ? (mesh.coordinates.size() - 2U) * 3U
                                                           : 0U);
    if (!triangulateLinkedRing(outer, mesh.fillIndices, pool) || mesh.fillIndices.empty()) {
        return {};
    }
    return mesh;
}

} // namespace lar::map::tool
