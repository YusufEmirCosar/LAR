
#include "viewer/viewport/earth_lar_view.h"

#include "viewer/map/map_projection.h"
#include "viewer/viewport/lar_view_fit.h"

#include <QMouseEvent>
#include <QResizeEvent>
#include <QVector2D>

#include <algorithm>

void EarthLarView::applyTrackedCamera() {
    const auto tracked = EarthCameraPolicy::trackedCoordinate(m_cameraState, m_hasState);
    if (tracked) {
        constexpr double ToDegrees = 180.0 / LarGeodesicGeometry::Pi;
        const double latitude = tracked->first * ToDegrees;
        const double longitude = tracked->second * ToDegrees;
        if (m_viewMode == LarViewMode::Mercator) {
            setFlatCenter({longitude, lar::map::MapProjection::projectLatitude(latitude)});
        } else {
            setRotation(longitude, latitude);
        }
    }
    setCameraBearing(static_cast<float>(EarthCameraPolicy::bearingDegrees(m_cameraState)));
}

void EarthLarView::rotateSphere(const QPoint &delta) {
    if (m_viewMode != LarViewMode::Sphere || m_cameraState.mode != CameraTrackingMode::Free)
        return;
    const QPointF center = rotation();
    const double sensitivity = 0.25 / std::max(0.2, static_cast<double>(activeZoom()));
    const double longitude =
        lar::map::MapProjection::wrapLongitude(center.x() - delta.x() * sensitivity);
    const double latitude = std::clamp(center.y() + delta.y() * sensitivity, -90.0, 90.0);
    setRotation(longitude, latitude);
    markFallbackGeometryDirty();
    refreshOverlays();
}

double EarthLarView::navigationMetersPerPixel() const noexcept {
    return EarthCameraPolicy::navigationMetersPerPixel(mapCamera(), width(), height());
}

void EarthLarView::fitToData() {
    if (!m_hasState || width() <= 0 || height() <= 0)
        return;
    applyTrackedCamera();
    const auto tracked = EarthCameraPolicy::trackedCoordinate(m_cameraState, m_hasState);
    const auto coordinates =
        EarthCameraPolicy::fitCoordinates(m_plane, m_target, m_availableFields, m_hasState);
    if (m_viewMode == LarViewMode::Sphere) {
        const auto fit = LarViewFit::sphere(coordinates, tracked);
        if (!fit)
            return;
        setRotation(fit->longitudeDegrees, fit->latitudeDegrees);
        setSphereZoom(fit->zoom);
        if (fit->crossesHorizon) {
            emit diagnosticRaised(
                QStringLiteral("The selected tracking anchor cannot show all LAR geometry "
                               "on one globe hemisphere."));
        }
    } else {
        const auto fit = LarViewFit::mercator(coordinates, tracked,
                                              static_cast<double>(cameraBearing()), size());
        if (!fit)
            return;
        setFlatZoom(fit->zoom);
        setFlatCenter(fit->center);
    }
    updateZoneBackend();
    markFallbackGeometryDirty();
    update();
    refreshOverlays();
}

void EarthLarView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_cameraState.mode != CameraTrackingMode::Free) {
        emit freeMovementRequested();
    }
    lar::map::EarthMapWidget::mousePressEvent(event);
}

void EarthLarView::mouseDoubleClickEvent(QMouseEvent *event) {
    fitToData();
    event->accept();
}

void EarthLarView::resizeEvent(QResizeEvent *event) {
    lar::map::EarthMapWidget::resizeEvent(event);
    updateZoneBackend();
    if (m_markerOverlay) {
        m_markerOverlay->setGeometry(rect());
        m_markerOverlay->raise();
    }
    if (m_navigationOverlay) {
        m_navigationOverlay->setGeometry(rect());
        m_navigationOverlay->raise();
        m_navigationOverlay->update();
    }
    markFallbackGeometryDirty();
}

QPointF EarthLarView::zoomAnchorForWheel(const QPointF &cursorPosition) const noexcept {
    return EarthCameraPolicy::zoomAnchor(m_cameraState, m_hasState, width(), height(),
                                         cursorPosition);
}
