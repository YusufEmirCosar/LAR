
#include "viewer/viewport/earth_lar_view.h"

#include <QtMath>

#include <algorithm>

void EarthLarView::initializeGL() {
    lar::map::EarthMapWidget::initializeGL();
    if (!m_zoneGpuLayer->initialize()) {
        refreshOverlays();
        return;
    }
    m_parametricBackendReady = m_parametricZoneGpuLayer->initialize();
    updateZoneBackend();
    markFallbackGeometryDirty();
}

void EarthLarView::cleanupGL() {
    if (isValid()) {
        makeCurrent();
        m_zoneGpuLayer->cleanup();
        m_parametricZoneGpuLayer->cleanup();
        doneCurrent();
    }
    m_parametricBackendReady = false;
    m_useParametricBackend = false;
    lar::map::EarthMapWidget::cleanupGL();
}

void EarthLarView::requestOverlayPreparation() {
    if (!m_overlayCoordinator || m_useParametricBackend)
        return;
    m_overlayCoordinator->request(m_target, m_availableFields, mapCamera(), width(), height());
}

void EarthLarView::updateZoneBackend() {
    if (!m_parametricZoneGpuLayer || !m_overlayCoordinator)
        return;
    const bool eligible = m_parametricZoneGpuLayer->setZones(m_target, m_availableFields,
                                                             mapCamera(), width(), height());
    const bool useParametric = m_parametricBackendReady && eligible;
    if (useParametric == m_useParametricBackend)
        return;
    m_useParametricBackend = useParametric;
    m_overlayCoordinator->setFallbackEnabled(!useParametric);
}

LarZoneRenderState EarthLarView::zoneRenderState() const {
    const bool sphere = m_viewMode == LarViewMode::Sphere;
    QMatrix4x4 projection;
    if (sphere) {
        projection = getProjectionMatrix(width(), height());
    } else {
        const lar::map::MercatorViewport viewport = flatViewport(width(), height());
        projection.ortho(static_cast<float>(-viewport.halfWidth),
                         static_cast<float>(viewport.halfWidth),
                         static_cast<float>(-viewport.halfHeight),
                         static_cast<float>(viewport.halfHeight), -1.0F, 1.0F);
    }
    lar::map::WorldCopyRange copies =
        sphere ? lar::map::WorldCopyRange{0, 0} : visibleFlatWorldCopies(width(), height());
    std::optional<std::pair<double, double>> bounds;
    if (!sphere && m_useParametricBackend && m_parametricZoneGpuLayer) {
        double minimum = 0.0;
        double maximum = 0.0;
        if (m_parametricZoneGpuLayer->longitudeBounds(&minimum, &maximum)) {
            bounds = {{minimum, maximum}};
        }
    } else if (!sphere && m_overlayCoordinator) {
        bounds = m_overlayCoordinator->longitudeBounds();
    }
    if (bounds) {
        const auto zoneCopies = mapCamera().visibleWorldCopiesForBounds(
            bounds->first, bounds->second, width(), height());
        if (zoneCopies.count() > 0) {
            if (copies.count() == 0)
                copies = zoneCopies;
            else {
                copies.first = std::min(copies.first, zoneCopies.first);
                copies.last = std::max(copies.last, zoneCopies.last);
            }
        }
    }
    return {sphere,
            mapCamera().sphereProjectionParameters(),
            mapCamera().sphereCenter(),
            qDegreesToRadians(cameraBearing()),
            flatCenterProjected(),
            projection,
            copies};
}

void EarthLarView::paintGL() {
    lar::map::EarthMapWidget::paintGL();
    LarZoneMesh prepared;
    if (!m_useParametricBackend && m_overlayCoordinator &&
        m_overlayCoordinator->takePreparedMesh(&prepared)) {
        m_zoneGpuLayer->upload(prepared);
    }
    if (!m_useParametricBackend && m_overlayCoordinator && m_overlayCoordinator->dirty() &&
        !m_overlayCoordinator->pending()) {
        requestOverlayPreparation();
    }
    const LarZoneRenderState state = zoneRenderState();
    if (m_useParametricBackend) {
        m_parametricZoneGpuLayer->draw(state);
    } else {
        m_zoneGpuLayer->draw(state);
    }
    emit frameRendered();
    if (m_markerOverlay) {
        m_markerOverlay->update();
        m_markerOverlay->raise();
    }
    if (m_navigationOverlay) {
        m_navigationOverlay->update();
        m_navigationOverlay->raise();
    }
}

bool EarthLarView::projectMarker(double latitudeRadians, double longitudeRadians, QPointF &screen,
                                 bool &visible) const {
    constexpr double ToDegrees = 180.0 / LarGeodesicGeometry::Pi;
    if (projectGeoToScreen(longitudeRadians * ToDegrees, latitudeRadians * ToDegrees, screen,
                           visible))
        return true;
    visible = false;
    return false;
}

LarMarkerState EarthLarView::markerState() const {
    return {m_plane,           m_target,   m_availableFields,
            m_rendererMessage, m_hasState, static_cast<double>(cameraBearing())};
}
