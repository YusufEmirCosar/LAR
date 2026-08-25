#pragma once

/**
 * @file grid_camera_transform.h
 * @brief Pan, zoom, tracking, and coordinate transforms for the grid page.
 */

#include "viewer/viewport/viewport_camera_controller.h"

#include <QPoint>
#include <QPointF>
#include <QSize>

#include <array>

/** @brief Mutable local-plane camera used by GridLarView. */
class GridCameraTransform final {
  public:
    void clear() noexcept;
    /**
     * @brief Sets origin.
     *
     * @param[in] latitudeRadians Angle value expressed in radians.
     * @param[in] longitudeRadians Angle value expressed in radians.
     */
    void setOrigin(double latitudeRadians, double longitudeRadians) noexcept;
    void applyCameraState(const ViewportCameraState &state) noexcept;

    [[nodiscard]] bool hasLocation() const noexcept;
    [[nodiscard]] bool hasOrigin() const noexcept;
    [[nodiscard]] double bearingRadians() const noexcept;
    [[nodiscard]] double scale() const noexcept;
    [[nodiscard]] double originLatitude() const noexcept;
    [[nodiscard]] double originLongitude() const noexcept;
    [[nodiscard]] const std::array<double, 3> &location() const noexcept;

    /**
     * @brief Sets scale.
     *
     * @param[in] scale Finite numeric value used by the operation.
     */
    void setScale(double scale) noexcept;
    void zoom(int wheelDelta) noexcept;
    void pan(const QPoint &pixelDelta) noexcept;

    [[nodiscard]] QPointF geographicToWorld(const double position[3]) const;
    [[nodiscard]] QPointF worldToScreen(const QPointF &world, const QSize &viewportSize) const;
    [[nodiscard]] QPointF screenToWorld(const QPointF &screen, const QSize &viewportSize) const;

  private:
    std::array<double, 3> m_location{};
    bool m_hasLocation = false;
    double m_bearingRadians = 0.0;
    double m_scale = 0.01;
    double m_originLatitude = 0.0;
    double m_originLongitude = 0.0;
    bool m_hasOrigin = false;
};
