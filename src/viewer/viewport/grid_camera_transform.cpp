
#include "viewer/viewport/grid_camera_transform.h"

#include "viewer/lar_projection.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace {

constexpr double EarthRadiusMeters = 6371008.8;
constexpr double Pi = 3.14159265358979323846;

} // namespace

void GridCameraTransform::clear() noexcept {
    m_hasLocation = false;
    m_hasOrigin = false;
    m_bearingRadians = 0.0;
}

void GridCameraTransform::setOrigin(double latitudeRadians, double longitudeRadians) noexcept {
    if (m_hasOrigin) {
        return;
    }
    m_originLatitude = latitudeRadians;
    m_originLongitude = longitudeRadians;
    m_hasOrigin = true;
}

void GridCameraTransform::applyCameraState(const ViewportCameraState &state) noexcept {
    m_bearingRadians =
        std::isfinite(state.bearingRadians) ? std::remainder(state.bearingRadians, 2.0 * Pi) : 0.0;
    const bool anchorValid =
        std::isfinite(state.anchorRadians[0]) && std::isfinite(state.anchorRadians[1]) &&
        state.anchorRadians[0] >= -Pi * 0.5 && state.anchorRadians[0] <= Pi * 0.5 &&
        state.anchorRadians[1] >= -Pi && state.anchorRadians[1] <= Pi;

    if (state.hasAnchor && anchorValid && (state.trackingActive || !m_hasLocation)) {

        m_location = state.anchorRadians;
        m_hasLocation = true;
    }
}

bool GridCameraTransform::hasLocation() const noexcept {
    return m_hasLocation;
}

bool GridCameraTransform::hasOrigin() const noexcept {
    return m_hasOrigin;
}

double GridCameraTransform::bearingRadians() const noexcept {
    return m_bearingRadians;
}

double GridCameraTransform::scale() const noexcept {
    return m_scale;
}

double GridCameraTransform::originLatitude() const noexcept {
    return m_originLatitude;
}

double GridCameraTransform::originLongitude() const noexcept {
    return m_originLongitude;
}

const std::array<double, 3> &GridCameraTransform::location() const noexcept {
    return m_location;
}

void GridCameraTransform::setScale(double scale) noexcept {
    m_scale = std::clamp(scale, 0.00001, 100.0);
}

void GridCameraTransform::zoom(int wheelDelta) noexcept {
    setScale(m_scale * std::pow(1.0015, wheelDelta));
}

void GridCameraTransform::pan(const QPoint &pixelDelta) noexcept {
    if (!m_hasLocation || m_scale <= 0.0) {
        return;
    }
    const double eastMeters = -static_cast<double>(pixelDelta.x()) / m_scale;
    const double northMeters = static_cast<double>(pixelDelta.y()) / m_scale;
    m_location[0] =
        std::clamp(m_location[0] + northMeters / EarthRadiusMeters, -Pi * 0.5, Pi * 0.5);
    const double cosineLatitude = qMax(0.01, std::cos(m_location[0]));
    m_location[1] += eastMeters / (EarthRadiusMeters * cosineLatitude);
    while (m_location[1] > Pi) {
        m_location[1] -= 2.0 * Pi;
    }
    while (m_location[1] < -Pi) {
        m_location[1] += 2.0 * Pi;
    }
}

QPointF GridCameraTransform::geographicToWorld(const double position[3]) const {
    if (!m_hasLocation) {
        return {};
    }
    return LarProjection::geographicToPlaneWorld(position, m_location.data(), m_bearingRadians,
                                                 m_originLatitude, m_hasOrigin);
}

QPointF GridCameraTransform::worldToScreen(const QPointF &world, const QSize &viewportSize) const {
    return LarProjection::worldToScreen(world, viewportSize.width(), viewportSize.height(),
                                        m_scale);
}

QPointF GridCameraTransform::screenToWorld(const QPointF &screen, const QSize &viewportSize) const {
    return LarProjection::screenToWorld(screen, viewportSize.width(), viewportSize.height(),
                                        m_scale);
}
