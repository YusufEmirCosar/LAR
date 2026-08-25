
#include "viewer/viewport/lar_view_fit.h"

#include "viewer/lar_geodesic_geometry.h"
#include "viewer/map/map_projection.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

constexpr double Pi = LarGeodesicGeometry::Pi;
constexpr double RadiansToDegrees = 180.0 / Pi;

double angularDistance(double latitudeA, double longitudeA, double latitudeB, double longitudeB) {
    const double deltaLongitude = longitudeB - longitudeA;
    const double cosine =
        std::clamp(std::sin(latitudeA) * std::sin(latitudeB) +
                       std::cos(latitudeA) * std::cos(latitudeB) * std::cos(deltaLongitude),
                   -1.0, 1.0);
    return std::acos(cosine);
}

} // namespace

std::optional<LarMercatorFit>
LarViewFit::mercator(const std::vector<Coordinate> &coordinatesRadians,
                     const std::optional<Coordinate> &trackedCenterRadians, double bearingDegrees,
                     const QSize &viewportSize) {
    if (coordinatesRadians.empty() || viewportSize.width() <= 0 || viewportSize.height() <= 0) {

        return std::nullopt;
    }

    const double referenceLongitude = (trackedCenterRadians)
                                          ? (trackedCenterRadians->second * RadiansToDegrees)
                                          : (coordinatesRadians.front().second * RadiansToDegrees);
    std::vector<QPointF> projected;
    projected.reserve(coordinatesRadians.size());
    double previousLongitude = referenceLongitude;
    for (const Coordinate &coordinate : coordinatesRadians) {
        const double longitude = lar::map::MapProjection::unwrapLongitude(
            coordinate.second * RadiansToDegrees, previousLongitude);
        previousLongitude = longitude;
        projected.emplace_back(longitude, lar::map::MapProjection::projectLatitude(
                                              coordinate.first * RadiansToDegrees));
    }

    QPointF center;
    if (trackedCenterRadians) {
        center = {referenceLongitude, lar::map::MapProjection::projectLatitude(
                                          trackedCenterRadians->first * RadiansToDegrees)};
    } else {
        double minimumX = projected.front().x();
        double maximumX = minimumX;
        double minimumY = projected.front().y();
        double maximumY = minimumY;
        for (const QPointF &point : projected) {
            minimumX = std::min(minimumX, point.x());
            maximumX = std::max(maximumX, point.x());
            minimumY = std::min(minimumY, point.y());
            maximumY = std::max(maximumY, point.y());
        }
        center = {(minimumX + maximumX) * 0.5, (minimumY + maximumY) * 0.5};
    }

    const double bearing = qDegreesToRadians(bearingDegrees);
    const double cosine = std::cos(bearing);
    const double sine = std::sin(bearing);
    double requiredX = 0.001;
    double requiredY = 0.001;
    for (const QPointF &point : projected) {
        const double deltaX = point.x() - center.x();
        const double deltaY = point.y() - center.y();
        requiredX = std::max(requiredX, std::abs(deltaX * cosine - deltaY * sine));
        requiredY = std::max(requiredY, std::abs(deltaX * sine + deltaY * cosine));
    }

    const double aspect =
        static_cast<double>(viewportSize.width()) / static_cast<double>(viewportSize.height());
    double baseHalfWidth = 180.0;
    double baseHalfHeight = 180.0;
    if (aspect >= 1.0) {
        baseHalfWidth = baseHalfHeight * aspect;
    } else if (aspect > 0.0) {
        baseHalfHeight = baseHalfWidth / aspect;
    }

    return LarMercatorFit{center, static_cast<float>(0.82 * std::min(baseHalfWidth / requiredX,
                                                                     baseHalfHeight / requiredY))};
}

std::optional<LarSphereFit>
LarViewFit::sphere(const std::vector<Coordinate> &coordinatesRadians,
                   const std::optional<Coordinate> &trackedCenterRadians) {
    if (coordinatesRadians.empty()) {
        return std::nullopt;
    }

    double centerLatitude = 0.0;
    double centerLongitude = 0.0;
    if (trackedCenterRadians) {
        centerLatitude = trackedCenterRadians->first;
        centerLongitude = trackedCenterRadians->second;
    } else {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        for (const Coordinate &coordinate : coordinatesRadians) {
            const double cosine = std::cos(coordinate.first);
            x += cosine * std::cos(coordinate.second);
            y += cosine * std::sin(coordinate.second);
            z += std::sin(coordinate.first);
        }
        const double horizontal = std::hypot(x, y);
        if (horizontal > 1.0e-12 || std::abs(z) > 1.0e-12) {
            centerLatitude = std::atan2(z, horizontal);
            centerLongitude = std::atan2(y, x);
        } else {
            centerLatitude = coordinatesRadians.front().first;
            centerLongitude = coordinatesRadians.front().second;
        }
    }

    double maximumSine = 1.0e-5;
    bool crossesHorizon = false;
    for (const Coordinate &coordinate : coordinatesRadians) {
        const double angle =
            angularDistance(centerLatitude, centerLongitude, coordinate.first, coordinate.second);
        maximumSine = std::max(maximumSine, std::sin(std::min(angle, Pi * 0.5)));
        crossesHorizon = crossesHorizon || angle >= Pi * 0.5;
    }

    return LarSphereFit{centerLongitude * RadiansToDegrees, centerLatitude * RadiansToDegrees,
                        static_cast<float>(0.82 / maximumSine), crossesHorizon};
}
