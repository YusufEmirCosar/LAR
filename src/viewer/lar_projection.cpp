
#include "viewer/lar_projection.h"

#include <QtGlobal>
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

QPointF LarProjection::worldToScreen(const QPointF &world, double canvasWidth, double canvasHeight,
                                     double scale) noexcept {
    return {canvasWidth * 0.5 + world.x() * scale, canvasHeight * 0.5 - world.y() * scale};
}

QPointF LarProjection::screenToWorld(const QPointF &screen, double canvasWidth, double canvasHeight,
                                     double scale) noexcept {
    return {(screen.x() - canvasWidth * 0.5) / scale, -(screen.y() - canvasHeight * 0.5) / scale};
}
