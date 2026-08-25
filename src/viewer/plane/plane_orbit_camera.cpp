
#include "viewer/plane/plane_orbit_camera.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {
constexpr double MinimumElevationDegrees = -82.0;
constexpr double MinimumSurfaceElevationDegrees = 3.0;
constexpr double MaximumElevationDegrees = 82.0;
constexpr double MinimumDistance = 2.6;
constexpr double MaximumDistance = 20'000'000.0;
constexpr double HorizontalOrbitDegrees = 180.0;
constexpr double VerticalOrbitDegrees = 120.0;
} // namespace

void PlaneOrbitCamera::orbit(const QPointF &pixelDelta, const QSize &viewportSize) noexcept {
    const double width = static_cast<double>(std::max(1, viewportSize.width()));
    const double height = static_cast<double>(std::max(1, viewportSize.height()));
    const double horizontalDelta =
        std::clamp(pixelDelta.x(), -width * 0.25, width * 0.25) * HorizontalOrbitDegrees / width;
    const double verticalDelta =
        std::clamp(pixelDelta.y(), -height * 0.25, height * 0.25) * VerticalOrbitDegrees / height;
    m_azimuthOffsetDegrees = std::remainder(m_azimuthOffsetDegrees + horizontalDelta, 360.0);
    const double minimumElevation =
        m_groundConstrained ? MinimumSurfaceElevationDegrees : MinimumElevationDegrees;
    m_elevationDegrees =
        std::clamp(m_elevationDegrees + verticalDelta, minimumElevation, MaximumElevationDegrees);
}

void PlaneOrbitCamera::zoom(int wheelDelta) noexcept {
    const double steps = static_cast<double>(wheelDelta) / 120.0;
    m_distance = std::clamp(m_distance * std::exp(-0.14 * steps), MinimumDistance, MaximumDistance);
}

void PlaneOrbitCamera::reset() noexcept {
    m_azimuthOffsetDegrees = 0.0;
    m_elevationDegrees = 16.0;
    m_distance = 6.0;
}

void PlaneOrbitCamera::setGroundConstrained(bool constrained) noexcept {
    m_groundConstrained = constrained;
    if (m_groundConstrained) {
        m_elevationDegrees = std::max(m_elevationDegrees, MinimumSurfaceElevationDegrees);
    }
}

QVector3D PlaneOrbitCamera::position(double headingRadians) const noexcept {
    const double azimuth = headingRadians + qDegreesToRadians(m_azimuthOffsetDegrees);
    const double elevation = qDegreesToRadians(m_elevationDegrees);
    const double horizontal = m_distance * std::cos(elevation);
    return {static_cast<float>(-std::sin(azimuth) * horizontal),
            static_cast<float>(std::sin(elevation) * m_distance),
            static_cast<float>(std::cos(azimuth) * horizontal)};
}

QMatrix4x4 PlaneOrbitCamera::viewMatrix(double headingRadians) const noexcept {
    QMatrix4x4 view;
    view.lookAt(position(headingRadians), QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F));
    return view;
}
