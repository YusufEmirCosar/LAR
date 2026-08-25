
#include "viewer/lar_geodesic_geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

double LarGeodesicGeometry::wrapLongitude(double longitudeRadians) noexcept {
    if (!std::isfinite(longitudeRadians)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    double wrapped = std::remainder(longitudeRadians, TwoPi);
    if (wrapped >= Pi) {
        wrapped -= TwoPi;
    }
    if (wrapped < 0.0) {
        return wrapped < -Pi ? wrapped + TwoPi : wrapped;
    }
    return wrapped;
}

double LarGeodesicGeometry::positiveAngularSpan(double startRadians, double endRadians) noexcept {
    if (!std::isfinite(startRadians) || !std::isfinite(endRadians)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double normalizedStart = std::remainder(startRadians, TwoPi);
    const double normalizedEnd = std::remainder(endRadians, TwoPi);
    double span = std::fmod(normalizedEnd - normalizedStart, TwoPi);
    if (span < 0.0) {
        span += TwoPi;
    }
    if (span < 1.0e-12) {
        return TwoPi;
    }
    return std::min(span, TwoPi);
}

GeoCoordinateRadians LarGeodesicGeometry::destination(const GeoCoordinateRadians &center,
                                                      double distanceMeters,
                                                      double bearingRadians) noexcept {
    if (!std::isfinite(center.latitude) || !std::isfinite(center.longitude) ||
        !std::isfinite(distanceMeters) || !std::isfinite(bearingRadians) || distanceMeters < 0.0) {

        return {};
    }

    const double angularDistance = distanceMeters / EarthRadiusMeters;
    const double sinLatitude = std::sin(center.latitude);
    const double cosLatitude = std::cos(center.latitude);
    const double sinDistance = std::sin(angularDistance);
    const double cosDistance = std::cos(angularDistance);

    const double destinationLatitude = std::asin(
        std::clamp(sinLatitude * cosDistance + cosLatitude * sinDistance * std::cos(bearingRadians),
                   -1.0, 1.0));
    const double longitudeOffset =
        std::atan2(std::sin(bearingRadians) * sinDistance * cosLatitude,
                   cosDistance - sinLatitude * std::sin(destinationLatitude));

    return {destinationLatitude, wrapLongitude(center.longitude + longitudeOffset)};
}

std::vector<GeoCoordinateRadians> LarGeodesicGeometry::arc(const GeoCoordinateRadians &center,
                                                           double distanceMeters,
                                                           double startBearingRadians,
                                                           double spanRadians, int segmentCount) {
    const int safeSegments = std::clamp(segmentCount, 1, 2048);
    std::vector<GeoCoordinateRadians> points;
    points.reserve(static_cast<size_t>(safeSegments + 1));
    for (int index = 0; index <= safeSegments; ++index) {
        const double fraction = static_cast<double>(index) / static_cast<double>(safeSegments);
        points.push_back(
            destination(center, distanceMeters, startBearingRadians + spanRadians * fraction));
    }
    return points;
}
