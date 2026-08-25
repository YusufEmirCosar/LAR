
#include "viewer/lar_geometry_builder.h"

#include "viewer/lar_geodesic_geometry.h"

#include <QtGlobal>
#include <cmath>

QPointF LarGeometryBuilder::radialPoint(const QPointF &center, double radius,
                                        double angle) noexcept {
    return center + QPointF(radius * std::sin(angle), radius * std::cos(angle));
}

QPolygonF LarGeometryBuilder::inZoneWedgePolygon(const QPointF &centerWorld, double r1Meters,
                                                 double r2Meters, double theta1Rad,
                                                 double theta2Rad, double yawRad) {
    if (!std::isfinite(r1Meters) || !std::isfinite(r2Meters) || !std::isfinite(theta1Rad) ||
        !std::isfinite(theta2Rad) || !std::isfinite(yawRad)) {
        return {};
    }
    const double start = std::remainder(theta1Rad, LarGeodesicGeometry::TwoPi);
    const double span = LarGeodesicGeometry::positiveAngularSpan(theta1Rad, theta2Rad);
    if (!std::isfinite(span)) {
        return {};
    }

    const int pointCount = qMax(32, int(std::ceil(span * 48.0)));
    QPolygonF polygon;
    polygon.reserve((pointCount + 1) * 2);
    for (int i = 0; i <= pointCount; ++i) {
        const double angle = start + span * double(i) / double(pointCount) - yawRad;
        polygon.append(radialPoint(centerWorld, r2Meters, angle));
    }
    for (int i = pointCount; i >= 0; --i) {
        const double angle = start + span * double(i) / double(pointCount) - yawRad;
        polygon.append(radialPoint(centerWorld, r1Meters, angle));
    }
    return polygon;
}
