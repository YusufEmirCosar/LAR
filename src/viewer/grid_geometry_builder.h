#pragma once

/**
 * @file grid_geometry_builder.h
 * @brief Adaptive grid spacing, line geometry, and distance-label helpers.
 */

#include <QPainter>
#include <QPointF>
#include <QString>

/** @brief Produces display-grid primitives for the painter-based viewport. */
class GridGeometryBuilder final {
  public:
    static double gridStep(double targetSpacing) noexcept;

    static double scaleBarDistance(double maximumDistance) noexcept;

    static QString formatDistance(double meters);
};
