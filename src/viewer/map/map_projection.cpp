
#include "viewer/map/map_projection.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace lar::map {

double MapProjection::projectLatitude(double latitudeDegrees) noexcept {
    if (!std::isfinite(latitudeDegrees)) {
        return 0.0;
    }
    const double latitude = std::clamp(latitudeDegrees, -MaximumMercatorLatitudeDegrees,
                                       MaximumMercatorLatitudeDegrees);
    const double radians = qDegreesToRadians(latitude);
    return qRadiansToDegrees(std::log(std::tan(M_PI / 4.0 + radians / 2.0)));
}

double MapProjection::unprojectLatitude(double projectedYDegrees) noexcept {
    if (!std::isfinite(projectedYDegrees)) {
        return 0.0;
    }
    const double radians =
        2.0 * std::atan(std::exp(qDegreesToRadians(projectedYDegrees))) - M_PI / 2.0;
    return std::clamp(qRadiansToDegrees(radians), -MaximumMercatorLatitudeDegrees,
                      MaximumMercatorLatitudeDegrees);
}

QPointF MapProjection::project(const QPointF &longitudeLatitude) noexcept {
    return {longitudeLatitude.x(), projectLatitude(longitudeLatitude.y())};
}

QPointF MapProjection::unproject(const QPointF &projected) noexcept {
    return {projected.x(), unprojectLatitude(projected.y())};
}

double MapProjection::unwrapLongitude(double longitudeDegrees,
                                      double referenceLongitudeDegrees) noexcept {
    if (!std::isfinite(longitudeDegrees) || !std::isfinite(referenceLongitudeDegrees)) {
        return longitudeDegrees;
    }
    while (longitudeDegrees - referenceLongitudeDegrees > HalfWorldDegrees) {
        longitudeDegrees -= WorldWidthDegrees;
    }
    while (longitudeDegrees - referenceLongitudeDegrees < -HalfWorldDegrees) {
        longitudeDegrees += WorldWidthDegrees;
    }
    return longitudeDegrees;
}

double MapProjection::wrapPeriodic(double value, double minimumInclusive, double period) noexcept {
    if (!std::isfinite(value) || !std::isfinite(minimumInclusive) || !std::isfinite(period) ||
        period <= 0.0) {

        return value;
    }

    double result = std::fmod(value - minimumInclusive, period);
    if (result < 0.0) {
        result += period;
    }
    return minimumInclusive + result;
}

double MapProjection::wrapLongitude(double longitudeDegrees) noexcept {
    return wrapPeriodic(longitudeDegrees, -HalfWorldDegrees, WorldWidthDegrees);
}

} // namespace lar::map
