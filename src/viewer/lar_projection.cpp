
#include "viewer/lar_projection.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

QPointF LarProjection::geographicToPlaneWorld(const double position[3],
                                              const double planeLocation[3], double yaw,
                                              double originLatitude, bool hasOrigin) noexcept {
    const double latRef = hasOrigin ? originLatitude : planeLocation[0];
    const double cosineLatitude = qMax(0.01, std::cos(latRef));
    double longitudeDelta = position[1] - planeLocation[1];
    while (longitudeDelta > Pi)
        longitudeDelta -= 2.0 * Pi;
    while (longitudeDelta < -Pi)
        longitudeDelta += 2.0 * Pi;
    const double east = EarthRadiusMeters * longitudeDelta * cosineLatitude;
    const double north = EarthRadiusMeters * (position[0] - planeLocation[0]);
    return {east * std::cos(yaw) - north * std::sin(yaw),
            east * std::sin(yaw) + north * std::cos(yaw)};
}

std::optional<GeoCoordinateRadians>
LarProjection::planeWorldToGeographic(const QPointF &world, const double planeLocation[3],
                                      double yaw, double originLatitude, bool hasOrigin) noexcept {
    if (planeLocation == nullptr || !std::isfinite(world.x()) || !std::isfinite(world.y()) ||
        !std::isfinite(planeLocation[0]) || !std::isfinite(planeLocation[1]) ||
        !std::isfinite(yaw)) {
        return std::nullopt;
    }

    const double latRef = hasOrigin ? originLatitude : planeLocation[0];
    if (!std::isfinite(latRef) || latRef < -Pi * 0.5 || latRef > Pi * 0.5 ||
        planeLocation[0] < -Pi * 0.5 || planeLocation[0] > Pi * 0.5 || planeLocation[1] < -Pi ||
        planeLocation[1] > Pi) {
        return std::nullopt;
    }

    const double cosineLatitude = std::max(0.01, std::cos(latRef));
    const double cosineYaw = std::cos(yaw);
    const double sineYaw = std::sin(yaw);
    // geographicToPlaneWorld rotates east/north into world x/y. Undo that rotation here.
    const double east = world.x() * cosineYaw + world.y() * sineYaw;
    const double north = -world.x() * sineYaw + world.y() * cosineYaw;
    const double latitude = planeLocation[0] + north / EarthRadiusMeters;
    if (!std::isfinite(latitude) || latitude < -Pi * 0.5 || latitude > Pi * 0.5) {
        return std::nullopt;
    }

    const double longitude = LarGeodesicGeometry::wrapLongitude(
        planeLocation[1] + east / (EarthRadiusMeters * cosineLatitude));
    if (!std::isfinite(longitude)) {
        return std::nullopt;
    }
    return GeoCoordinateRadians{latitude, longitude};
}

QPointF LarProjection::worldToScreen(const QPointF &world, double canvasWidth, double canvasHeight,
                                     double scale) noexcept {
    return {canvasWidth * 0.5 + world.x() * scale, canvasHeight * 0.5 - world.y() * scale};
}

QPointF LarProjection::screenToWorld(const QPointF &screen, double canvasWidth, double canvasHeight,
                                     double scale) noexcept {
    return {(screen.x() - canvasWidth * 0.5) / scale, -(screen.y() - canvasHeight * 0.5) / scale};
}
