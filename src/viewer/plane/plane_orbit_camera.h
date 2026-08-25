#pragma once

/**
 * @file plane_orbit_camera.h
 * @brief Bounded chase/orbit camera anchored at the aircraft center.
 */

#include <QMatrix4x4>
#include <QPointF>
#include <QSize>
#include <QVector3D>

/** @brief Stores user orbit offset while automatically following aircraft heading. */
class PlaneOrbitCamera final {
  public:
    void orbit(const QPointF &pixelDelta, const QSize &viewportSize) noexcept;
    void zoom(int wheelDelta) noexcept;
    void reset() noexcept;
    void setGroundConstrained(bool constrained) noexcept;

    [[nodiscard]] QMatrix4x4 viewMatrix(double headingRadians) const noexcept;
    [[nodiscard]] QVector3D position(double headingRadians) const noexcept;
    [[nodiscard]] double azimuthOffsetDegrees() const noexcept {
        return m_azimuthOffsetDegrees;
    }
    [[nodiscard]] double elevationDegrees() const noexcept {
        return m_elevationDegrees;
    }
    [[nodiscard]] double distance() const noexcept {
        return m_distance;
    }
    [[nodiscard]] bool groundConstrained() const noexcept {
        return m_groundConstrained;
    }

  private:
    double m_azimuthOffsetDegrees = 0.0;
    double m_elevationDegrees = 16.0;
    double m_distance = 6.0;
    bool m_groundConstrained = false;
};
