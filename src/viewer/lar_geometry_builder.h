#pragma once

/**
 * @file lar_geometry_builder.h
 * @brief Local Cartesian LAR ring and sector path construction.
 */

#include <QPointF>
#include <QPolygonF>

/** @brief Creates painter paths for in-range and in-zone LAR geometry. */
class LarGeometryBuilder final {
  public:
    static QPolygonF inZoneWedgePolygon(const QPointF &centerWorld, double r1Meters,
                                        double r2Meters, double theta1Rad, double theta2Rad,
                                        double yawRad);

  private:
    static QPointF radialPoint(const QPointF &center, double radius, double angle) noexcept;
};
