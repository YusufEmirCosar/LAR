#pragma once

/**
 * @file lar_view_fit.h
 * @brief Algorithms for fitting LAR coordinates in flat and globe views.
 */

#include <QPointF>
#include <QSize>

#include <optional>
#include <utility>
#include <vector>

/** @brief Flat-map center and zoom selected by a fit operation. */
struct LarMercatorFit final {
    QPointF center;
    float zoom = 1.0F;
};

/** @brief Globe rotation, zoom, and horizon result selected by a fit. */
struct LarSphereFit final {
    double longitudeDegrees = 0.0;
    double latitudeDegrees = 0.0;
    float zoom = 1.0F;
    bool crossesHorizon = false;
};

/** @brief Stateless camera-fit calculations with dateline awareness. */
class LarViewFit final {
  public:
    using Coordinate = std::pair<double, double>;

    [[nodiscard]] static std::optional<LarMercatorFit>
    mercator(const std::vector<Coordinate> &coordinatesRadians,
             const std::optional<Coordinate> &trackedCenterRadians, double bearingDegrees,
             const QSize &viewportSize);

    [[nodiscard]] static std::optional<LarSphereFit>
    sphere(const std::vector<Coordinate> &coordinatesRadians,
           const std::optional<Coordinate> &trackedCenterRadians);
};
