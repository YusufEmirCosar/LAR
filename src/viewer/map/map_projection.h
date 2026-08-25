#pragma once

/**
 * @file map_projection.h
 * @brief Web-Mercator projection and periodic longitude helpers.
 */

#include <QPointF>

namespace lar::map {

/** @brief Stateless conversions between geographic and projected degrees. */
class MapProjection final {
  public:
    static constexpr double MaximumMercatorLatitudeDegrees = 85.05112878;
    static constexpr double HalfWorldDegrees = 180.0;
    static constexpr double WorldWidthDegrees = 360.0;

    static double projectLatitude(double latitudeDegrees) noexcept;
    static double unprojectLatitude(double projectedYDegrees) noexcept;
    static QPointF project(const QPointF &longitudeLatitude) noexcept;
    static QPointF unproject(const QPointF &projected) noexcept;
    static double unwrapLongitude(double longitudeDegrees,
                                  double referenceLongitudeDegrees) noexcept;
    static double wrapPeriodic(double value, double minimumInclusive, double period) noexcept;
    static double wrapLongitude(double longitudeDegrees) noexcept;
};

} // namespace lar::map
