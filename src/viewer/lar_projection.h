#pragma once

/**
 * @file lar_projection.h
 * @brief Local tangent-plane conversion for nearby geographic positions.
 */

#include <QPointF>

/** @brief Projects latitude/longitude radians to local metric east/north axes. */
class LarProjection final {
  public:
    static constexpr double EarthRadiusMeters = 6371008.8;
    static constexpr double Pi = 3.14159265358979323846;

    static QPointF geographicToPlaneWorld(const double position[3], const double planeLocation[3],
                                          double yaw, double originLatitude,
                                          bool hasOrigin) noexcept;

    static QPointF worldToScreen(const QPointF &world, double canvasWidth, double canvasHeight,
                                 double scale) noexcept;

    static QPointF screenToWorld(const QPointF &screen, double canvasWidth, double canvasHeight,
                                 double scale) noexcept;
};
