
#include "viewer/map/map_camera.h"

#include "viewer/map/map_projection.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace lar::map {
namespace {

constexpr float MinimumMercatorZoom = 1.0F;
constexpr float MaximumMercatorZoom = 100000.0F;
constexpr float MinimumSphereZoom = 0.2F;
constexpr float MaximumSphereZoom = 5000.0F;

bool equal(float first, float second) noexcept {
    return qFuzzyCompare(first, second);
}

} // namespace

MapPresentation MapCamera::presentation() const noexcept {
    return m_presentation;
}

bool MapCamera::setPresentation(MapPresentation presentation) noexcept {
    if (m_presentation == presentation) {
        return false;
    }
    m_presentation = presentation;
    return true;
}

QPointF MapCamera::sphereCenter() const noexcept {
    return {m_sphereLongitude, m_sphereLatitude};
}

SphereProjectionParameters MapCamera::sphereProjectionParameters() const noexcept {
    const auto split = [](double value) {
        const float high = static_cast<float>(value);
        return std::pair{high, static_cast<float>(value - static_cast<double>(high))};
    };
    const auto [longitudeHigh, longitudeLow] = split(m_sphereLongitude);
    const auto [latitudeHigh, latitudeLow] = split(m_sphereLatitude);
    const double latitudeRadians = qDegreesToRadians(m_sphereLatitude);
    return {{longitudeHigh, latitudeHigh},
            {longitudeLow, latitudeLow},
            {static_cast<float>(std::sin(latitudeRadians)),
             static_cast<float>(std::cos(latitudeRadians))}};
}

bool MapCamera::setSphereCenter(double longitudeDegrees, double latitudeDegrees) noexcept {
    if (!std::isfinite(longitudeDegrees) || !std::isfinite(latitudeDegrees)) {
        return false;
    }
    const double longitude = MapProjection::wrapLongitude(longitudeDegrees);
    const double latitude = std::clamp(latitudeDegrees, -90.0, 90.0);
    if (m_sphereLongitude == longitude && m_sphereLatitude == latitude) {
        return false;
    }
    m_sphereLongitude = longitude;
    m_sphereLatitude = latitude;
    return true;
}

float MapCamera::bearingDegrees() const noexcept {
    return m_bearingDegrees;
}

bool MapCamera::setBearingDegrees(float bearingDegrees) noexcept {
    if (!std::isfinite(bearingDegrees)) {
        bearingDegrees = 0.0F;
    }
    const float normalized = std::remainder(bearingDegrees, 360.0F);
    if (equal(m_bearingDegrees, normalized)) {
        return false;
    }
    m_bearingDegrees = normalized;
    return true;
}

QRectF MapCamera::mercatorWorldBounds() const noexcept {
    return m_mercatorWorldBounds;
}

bool MapCamera::setMercatorWorldBounds(const QRectF &projectedBounds, int viewportWidth,
                                       int viewportHeight) noexcept {
    if (!projectedBounds.isValid() || projectedBounds.width() <= 0.0 ||
        projectedBounds.height() <= 0.0 || !std::isfinite(projectedBounds.left()) ||
        !std::isfinite(projectedBounds.right()) || !std::isfinite(projectedBounds.top()) ||
        !std::isfinite(projectedBounds.bottom())) {

        return false;
    }
    if (m_mercatorWorldBounds == projectedBounds) {
        return false;
    }
    m_mercatorWorldBounds = projectedBounds;
    m_mercatorCenter = projectedBounds.center();
    constrainMercatorCenter(viewportWidth, viewportHeight);
    return true;
}

QPointF MapCamera::mercatorCenter() const noexcept {
    return m_mercatorCenter;
}

bool MapCamera::setMercatorCenter(const QPointF &projectedCenter, int viewportWidth,
                                  int viewportHeight) noexcept {
    if (!std::isfinite(projectedCenter.x()) || !std::isfinite(projectedCenter.y())) {

        return false;
    }
    const QPointF previous = m_mercatorCenter;
    m_mercatorCenter = projectedCenter;
    constrainMercatorCenter(viewportWidth, viewportHeight);
    return previous != m_mercatorCenter;
}

float MapCamera::mercatorZoom() const noexcept {
    return m_mercatorZoom;
}

bool MapCamera::setMercatorZoom(float zoom, int viewportWidth, int viewportHeight) noexcept {
    if (!std::isfinite(zoom)) {
        return false;
    }
    const float bounded = std::clamp(zoom, MinimumMercatorZoom, MaximumMercatorZoom);
    if (equal(m_mercatorZoom, bounded)) {
        return false;
    }
    m_mercatorZoom = bounded;
    constrainMercatorCenter(viewportWidth, viewportHeight);
    return true;
}

float MapCamera::sphereZoom() const noexcept {
    return m_sphereZoom;
}

bool MapCamera::setSphereZoom(float zoom) noexcept {
    if (!std::isfinite(zoom)) {
        return false;
    }
    const float bounded = std::clamp(zoom, MinimumSphereZoom, MaximumSphereZoom);
    if (equal(m_sphereZoom, bounded)) {
        return false;
    }
    m_sphereZoom = bounded;
    return true;
}

float MapCamera::activeZoom() const noexcept {
    return m_presentation == MapPresentation::Mercator ? m_mercatorZoom : m_sphereZoom;
}

void MapCamera::setGeometryLongitudeBounds(double minimumLongitude,
                                           double maximumLongitude) noexcept {
    if (!std::isfinite(minimumLongitude) || !std::isfinite(maximumLongitude) ||
        minimumLongitude > maximumLongitude) {

        m_geometryMinimumLongitude = -180.0;
        m_geometryMaximumLongitude = 180.0;
        return;
    }
    m_geometryMinimumLongitude = minimumLongitude;
    m_geometryMaximumLongitude = maximumLongitude;
}

MercatorViewport MapCamera::mercatorViewport(int viewportWidth, int viewportHeight) const noexcept {
    const int safeWidth = std::max(1, viewportWidth);
    const int safeHeight = std::max(1, viewportHeight);
    const double halfWidth = m_mercatorWorldBounds.width() * 0.5;
    const double halfHeight = m_mercatorWorldBounds.height() * 0.5;
    const double aspect = static_cast<double>(safeWidth) / static_cast<double>(safeHeight);
    const double contentAspect = halfWidth / halfHeight;

    double viewHalfWidth = halfWidth;
    double viewHalfHeight = halfHeight;
    if (aspect >= contentAspect) {
        viewHalfWidth = halfHeight * aspect;
    } else {
        viewHalfHeight = halfWidth / aspect;
    }
    viewHalfWidth /= static_cast<double>(m_mercatorZoom);
    viewHalfHeight /= static_cast<double>(m_mercatorZoom);

    return {viewHalfWidth,
            viewHalfHeight,
            m_mercatorCenter.x() - viewHalfWidth,
            m_mercatorCenter.x() + viewHalfWidth,
            m_mercatorCenter.y() - viewHalfHeight,
            m_mercatorCenter.y() + viewHalfHeight};
}

WorldCopyRange MapCamera::visibleWorldCopies(int viewportWidth, int viewportHeight) const noexcept {
    if (m_presentation != MapPresentation::Mercator) {
        return {0, 0};
    }

    return visibleWorldCopiesForBounds(m_geometryMinimumLongitude, m_geometryMaximumLongitude,
                                       viewportWidth, viewportHeight);
}

WorldCopyRange MapCamera::visibleWorldCopiesForBounds(double minimumLongitude,
                                                      double maximumLongitude, int viewportWidth,
                                                      int viewportHeight) const noexcept {
    if (m_presentation != MapPresentation::Mercator || !std::isfinite(minimumLongitude) ||
        !std::isfinite(maximumLongitude) || minimumLongitude > maximumLongitude) {

        return m_presentation == MapPresentation::Mercator ? WorldCopyRange{0, -1}
                                                           : WorldCopyRange{0, 0};
    }

    const QPointF corners[] = {
        screenToMercator({0.0, 0.0}, viewportWidth, viewportHeight),
        screenToMercator({static_cast<double>(viewportWidth), 0.0}, viewportWidth, viewportHeight),
        screenToMercator({static_cast<double>(viewportWidth), static_cast<double>(viewportHeight)},
                         viewportWidth, viewportHeight),
        screenToMercator({0.0, static_cast<double>(viewportHeight)}, viewportWidth,
                         viewportHeight)};

    double viewportLeft = corners[0].x();
    double viewportRight = corners[0].x();
    for (const QPointF &corner : corners) {
        viewportLeft = std::min(viewportLeft, corner.x());
        viewportRight = std::max(viewportRight, corner.x());
    }

    constexpr double Epsilon = 1.0e-6;
    const double firstValue =
        std::ceil((viewportLeft - maximumLongitude - Epsilon) / MapProjection::WorldWidthDegrees);
    const double lastValue =
        std::floor((viewportRight - minimumLongitude + Epsilon) / MapProjection::WorldWidthDegrees);
    if (firstValue < std::numeric_limits<int>::min() ||
        firstValue > std::numeric_limits<int>::max() ||
        lastValue < std::numeric_limits<int>::min() ||
        lastValue > std::numeric_limits<int>::max()) {

        return {0, 0};
    }
    const int first = static_cast<int>(firstValue);
    const int last = static_cast<int>(lastValue);
    return first <= last ? WorldCopyRange{first, last} : WorldCopyRange{0, -1};
}

QPointF MapCamera::screenToMercator(const QPointF &screenPosition, int viewportWidth,
                                    int viewportHeight) const noexcept {
    const int safeWidth = std::max(1, viewportWidth);
    const int safeHeight = std::max(1, viewportHeight);
    const MercatorViewport viewport = mercatorViewport(safeWidth, safeHeight);
    const double normalizedX = screenPosition.x() / static_cast<double>(safeWidth) * 2.0 - 1.0;
    const double normalizedY = 1.0 - screenPosition.y() / static_cast<double>(safeHeight) * 2.0;
    const double viewDeltaX = normalizedX * viewport.halfWidth;
    const double viewDeltaY = normalizedY * viewport.halfHeight;
    const double bearing = qDegreesToRadians(m_bearingDegrees);
    const double cosine = std::cos(bearing);
    const double sine = std::sin(bearing);
    return {m_mercatorCenter.x() + viewDeltaX * cosine + viewDeltaY * sine,
            m_mercatorCenter.y() - viewDeltaX * sine + viewDeltaY * cosine};
}

bool MapCamera::panMercator(const QPoint &pixelDelta, int viewportWidth,
                            int viewportHeight) noexcept {
    const int safeWidth = std::max(1, viewportWidth);
    const int safeHeight = std::max(1, viewportHeight);
    const MercatorViewport viewport = mercatorViewport(safeWidth, safeHeight);
    const double viewOffsetX = static_cast<double>(pixelDelta.x()) * 2.0 * viewport.halfWidth /
                               static_cast<double>(safeWidth);
    const double viewOffsetY = -static_cast<double>(pixelDelta.y()) * 2.0 * viewport.halfHeight /
                               static_cast<double>(safeHeight);
    const double bearing = qDegreesToRadians(m_bearingDegrees);
    const double cosine = std::cos(bearing);
    const double sine = std::sin(bearing);
    const QPointF previous = m_mercatorCenter;
    m_mercatorCenter.rx() -= viewOffsetX * cosine + viewOffsetY * sine;
    m_mercatorCenter.ry() -= -viewOffsetX * sine + viewOffsetY * cosine;
    constrainMercatorCenter(viewportWidth, viewportHeight);
    return previous != m_mercatorCenter;
}

bool MapCamera::zoomAt(const QPointF &screenPosition, int wheelDelta, int viewportWidth,
                       int viewportHeight) noexcept {
    if (wheelDelta == 0) {
        return false;
    }
    const float factor = std::pow(1.0015F, static_cast<float>(wheelDelta));
    if (m_presentation == MapPresentation::Sphere) {
        return setSphereZoom(m_sphereZoom * factor);
    }

    const QPointF before = screenToMercator(screenPosition, viewportWidth, viewportHeight);
    const float previousZoom = m_mercatorZoom;
    m_mercatorZoom = std::clamp(m_mercatorZoom * factor, MinimumMercatorZoom, MaximumMercatorZoom);
    const QPointF after = screenToMercator(screenPosition, viewportWidth, viewportHeight);
    m_mercatorCenter += before - after;
    constrainMercatorCenter(viewportWidth, viewportHeight);
    return !equal(previousZoom, m_mercatorZoom);
}

void MapCamera::normalizeMercatorCenter() noexcept {
    m_mercatorCenter.setX(MapProjection::wrapLongitude(m_mercatorCenter.x()));
}

void MapCamera::clampMercatorCenterY(int viewportWidth, int viewportHeight) noexcept {
    const MercatorViewport viewport = mercatorViewport(viewportWidth, viewportHeight);
    const double boundsMinimum =
        std::min(m_mercatorWorldBounds.top(), m_mercatorWorldBounds.bottom());
    const double boundsMaximum =
        std::max(m_mercatorWorldBounds.top(), m_mercatorWorldBounds.bottom());
    const double bearing = qDegreesToRadians(m_bearingDegrees);
    const double verticalExtent = std::abs(std::sin(bearing)) * viewport.halfWidth +
                                  std::abs(std::cos(bearing)) * viewport.halfHeight;
    const double minimum = boundsMinimum + verticalExtent;
    const double maximum = boundsMaximum - verticalExtent;
    if (minimum >= maximum) {
        m_mercatorCenter.setY(m_mercatorWorldBounds.center().y());
    } else {
        m_mercatorCenter.setY(std::clamp(m_mercatorCenter.y(), minimum, maximum));
    }
}

void MapCamera::constrainMercatorCenter(int viewportWidth, int viewportHeight) noexcept {
    normalizeMercatorCenter();
    clampMercatorCenterY(viewportWidth, viewportHeight);
}

QMatrix4x4 MapCamera::projectionMatrix(int viewportWidth, int viewportHeight) const noexcept {
    QMatrix4x4 projection;
    const float aspect =
        viewportHeight > 0 ? static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight)
                           : 1.0F;
    if (m_presentation == MapPresentation::Sphere) {
        float horizontalScale = m_sphereZoom;
        float verticalScale = m_sphereZoom;
        if (aspect > 1.0F) {
            horizontalScale /= aspect;
        } else if (aspect > 0.0F && aspect < 1.0F) {
            verticalScale *= aspect;
        }
        projection.scale(horizontalScale, verticalScale, 1.0F);
    } else {
        const MercatorViewport viewport = mercatorViewport(viewportWidth, viewportHeight);
        projection.ortho(static_cast<float>(m_mercatorCenter.x() - viewport.halfWidth),
                         static_cast<float>(m_mercatorCenter.x() + viewport.halfWidth),
                         static_cast<float>(m_mercatorCenter.y() - viewport.halfHeight),
                         static_cast<float>(m_mercatorCenter.y() + viewport.halfHeight), -1.0F,
                         1.0F);
    }
    return projection;
}

bool MapCamera::projectGeoToScreen(double longitudeDegrees, double latitudeDegrees,
                                   int viewportWidth, int viewportHeight, QPointF &screenPosition,
                                   bool &visible) const noexcept {
    if (!std::isfinite(longitudeDegrees) || !std::isfinite(latitudeDegrees) ||
        latitudeDegrees < -90.0 || latitudeDegrees > 90.0 || viewportWidth <= 0 ||
        viewportHeight <= 0) {

        visible = false;
        return false;
    }

    const double bearing = qDegreesToRadians(m_bearingDegrees);
    const double cosine = std::cos(bearing);
    const double sine = std::sin(bearing);
    if (m_presentation == MapPresentation::Mercator) {
        const MercatorViewport viewport = mercatorViewport(viewportWidth, viewportHeight);
        const double longitude = MapProjection::unwrapLongitude(
            MapProjection::wrapLongitude(longitudeDegrees), m_mercatorCenter.x());
        const double projectedY = MapProjection::projectLatitude(latitudeDegrees);
        const double deltaX = longitude - m_mercatorCenter.x();
        const double deltaY = projectedY - m_mercatorCenter.y();
        const double viewX = deltaX * cosine - deltaY * sine;
        const double viewY = deltaX * sine + deltaY * cosine;
        const double normalizedX = viewX / viewport.halfWidth;
        const double normalizedY = viewY / viewport.halfHeight;

        screenPosition = {(normalizedX + 1.0) * 0.5 * viewportWidth,
                          (1.0 - normalizedY) * 0.5 * viewportHeight};

        visible = std::abs(latitudeDegrees) <= MapProjection::MaximumMercatorLatitudeDegrees &&
                  normalizedX >= -1.0 && normalizedX <= 1.0 && normalizedY >= -1.0 &&
                  normalizedY <= 1.0;
        return true;
    }

    const double longitude = qDegreesToRadians(longitudeDegrees);
    const double latitude = qDegreesToRadians(latitudeDegrees);
    const double centerLongitude = qDegreesToRadians(m_sphereLongitude);
    const double centerLatitude = qDegreesToRadians(m_sphereLatitude);
    const double x = std::cos(latitude) * std::sin(longitude - centerLongitude);
    const double y =
        std::cos(centerLatitude) * std::sin(latitude) -
        std::sin(centerLatitude) * std::cos(latitude) * std::cos(longitude - centerLongitude);
    const double z =
        std::sin(centerLatitude) * std::sin(latitude) +
        std::cos(centerLatitude) * std::cos(latitude) * std::cos(longitude - centerLongitude);
    const double viewX = x * cosine - y * sine;
    const double viewY = x * sine + y * cosine;
    const double aspect = static_cast<double>(viewportWidth) / static_cast<double>(viewportHeight);
    double horizontalScale = m_sphereZoom;
    double verticalScale = m_sphereZoom;
    if (aspect > 1.0) {
        horizontalScale /= aspect;
    } else if (aspect < 1.0) {
        verticalScale *= aspect;
    }
    const double normalizedX = viewX * horizontalScale;
    const double normalizedY = viewY * verticalScale;

    screenPosition = {(normalizedX + 1.0) * 0.5 * viewportWidth,
                      (1.0 - normalizedY) * 0.5 * viewportHeight};

    visible = z >= 0.0 && normalizedX >= -1.0 && normalizedX <= 1.0 && normalizedY >= -1.0 &&
              normalizedY <= 1.0;
    return true;
}

bool MapCamera::sphereCoordinateAt(const QPointF &screenPosition, int viewportWidth,
                                   int viewportHeight, double &longitudeDegrees,
                                   double &latitudeDegrees) const noexcept {
    const int safeWidth = std::max(1, viewportWidth);
    const int safeHeight = std::max(1, viewportHeight);
    const float aspect = static_cast<float>(safeWidth) / static_cast<float>(safeHeight);
    float horizontalScale = m_sphereZoom;
    float verticalScale = m_sphereZoom;
    if (aspect > 1.0F) {
        horizontalScale /= aspect;
    } else if (aspect < 1.0F) {
        verticalScale *= aspect;
    }

    const double normalizedX = screenPosition.x() / static_cast<double>(safeWidth) * 2.0 - 1.0;
    const double normalizedY = 1.0 - screenPosition.y() / static_cast<double>(safeHeight) * 2.0;
    const double viewX = normalizedX / horizontalScale;
    const double viewY = normalizedY / verticalScale;
    const double bearing = qDegreesToRadians(m_bearingDegrees);
    const double cosine = std::cos(bearing);
    const double sine = std::sin(bearing);
    const double sphereX = viewX * cosine + viewY * sine;
    const double sphereY = -viewX * sine + viewY * cosine;
    const double radiusSquared = sphereX * sphereX + sphereY * sphereY;
    if (radiusSquared > 1.0) {
        return false;
    }

    const double sphereZ = std::sqrt(std::max(0.0, 1.0 - radiusSquared));
    const double centerLatitude = qDegreesToRadians(m_sphereLatitude);
    const double sinLatitude =
        std::cos(centerLatitude) * sphereY + std::sin(centerLatitude) * sphereZ;
    latitudeDegrees = qRadiansToDegrees(std::asin(std::clamp(sinLatitude, -1.0, 1.0)));
    const double longitudeDelta = std::atan2(sphereX, -std::sin(centerLatitude) * sphereY +
                                                          std::cos(centerLatitude) * sphereZ);
    longitudeDegrees =
        MapProjection::wrapLongitude(m_sphereLongitude + qRadiansToDegrees(longitudeDelta));
    return true;
}

} // namespace lar::map
