
#include "viewer/viewport/earth_lar_view.h"

#include "domain/statefield.h"

#include <QMouseEvent>
#include <QResizeEvent>
#include <QtMath>

#include <utility>

EarthLarView::EarthLarView(std::shared_ptr<const lar::map::IMapAssetSource> assetSource,
                           QWidget *parent)
    : lar::map::EarthMapWidget(parent) {
    m_mapLoader = new EarthMapLoadController(std::move(assetSource), this);
    m_overlayCoordinator = new EarthOverlayCoordinator(this);
    setFlatWorldBounds(QRectF(-180.0, -180.0, 360.0, 360.0));
    m_zoneGpuLayer = new LarZoneGpuLayer(this);
    m_parametricZoneGpuLayer = new LarParametricZoneGpuLayer(this);
    m_markerOverlay = new LarMarkerLayer(
        [this](double latitude, double longitude, QPointF &screen, bool &visible) {
            return projectMarker(latitude, longitude, screen, visible);
        },
        [this] { return markerState(); }, this);
    m_navigationOverlay = new LarNavigationOverlay(
        [this] { return navigationMetersPerPixel(); },
        [this] { return qDegreesToRadians(static_cast<double>(cameraBearing())); }, this);
    for (QWidget *overlay :
         {static_cast<QWidget *>(m_markerOverlay), static_cast<QWidget *>(m_navigationOverlay)}) {
        overlay->setGeometry(rect());
        overlay->show();
        overlay->raise();
    }

    connect(m_mapLoader, &EarthMapLoadController::loadStarted, this, [this] {
        m_rendererMessage = QStringLiteral("Loading world map…");
        refreshOverlays();
    });
    connect(m_mapLoader, &EarthMapLoadController::assetReady, this, &EarthLarView::installMapAsset);
    connect(m_overlayCoordinator, &EarthOverlayCoordinator::meshAvailable, this,
            [this](bool inputRejected) {
                const QString prefix = QStringLiteral("Invalid or excessive LAR zone");
                if (inputRejected) {
                    const QString message = prefix + QStringLiteral(" geometry was ignored.");
                    if (m_rendererMessage != message)
                        setRendererDiagnostic(message);
                } else if (m_rendererMessage.startsWith(prefix)) {
                    m_rendererMessage.clear();
                }
                update();
                refreshOverlays();
            });

    m_dragConnection =
        connect(this, &lar::map::EarthMapWidget::mapDragged, this, &EarthLarView::rotateSphere);
    connect(this, &lar::map::EarthMapWidget::rendererError, this, [this](const QString &message) {
        m_mapLoader->markFailed(message);
        setRendererDiagnostic(message);
        emit mapUnavailable();
    });
    connect(this, &lar::map::EarthMapWidget::zoomChanged, this, [this] {
        updateZoneBackend();
        markFallbackGeometryDirty();
        refreshOverlays();
    });
    connect(this, &lar::map::EarthMapWidget::mapDragged, this, [this] { updateZoneBackend(); });
    connect(m_zoneGpuLayer, &LarZoneGpuLayer::diagnosticRaised, this,
            &EarthLarView::setRendererDiagnostic);
    connect(m_parametricZoneGpuLayer, &LarParametricZoneGpuLayer::diagnosticRaised, this,
            &EarthLarView::setRendererDiagnostic);

    connect(this, &EarthLarView::frameRendered, &m_pageEvents,
            &LarViewportPageEvents::frameRendered);
    connect(this, &EarthLarView::freeMovementRequested, &m_pageEvents,
            &LarViewportPageEvents::freeMovementRequested);
    connect(this, &EarthLarView::diagnosticRaised, &m_pageEvents,
            &LarViewportPageEvents::diagnosticRaised);
    connect(this, &EarthLarView::mapUnavailable, &m_pageEvents,
            &LarViewportPageEvents::pageUnavailable);
}

EarthLarView::~EarthLarView() {
    delete m_overlayCoordinator;
    m_overlayCoordinator = nullptr;
    delete m_mapLoader;
    m_mapLoader = nullptr;
    cleanupGL();
}

void EarthLarView::refreshOverlays() {
    if (m_markerOverlay)
        m_markerOverlay->update();
    if (m_navigationOverlay)
        m_navigationOverlay->update();
}

void EarthLarView::setRendererDiagnostic(const QString &message) {
    m_rendererMessage = message;
    emit diagnosticRaised(message);
    refreshOverlays();
}

void EarthLarView::markFallbackGeometryDirty(bool requestNow) {
    if (!m_overlayCoordinator)
        return;
    m_overlayCoordinator->markDirty();
    if (requestNow && !m_useParametricBackend)
        requestOverlayPreparation();
}

bool EarthLarView::ensureMapLoaded() {
    const auto previousState = m_mapLoader->state();
    if (m_mapLoader->ensureLoaded())
        return true;
    if (previousState != EarthMapLoadController::State::Failed) {
        setRendererDiagnostic(m_mapLoader->failureMessage());
        emit mapUnavailable();
    }
    return false;
}

void EarthLarView::installMapAsset(quint64 revision, const lar::map::MapAssetReadResult &result) {
    if (!result.succeeded()) {
        setRendererDiagnostic(result.message.isEmpty()
                                  ? QStringLiteral("The packaged world map could not be loaded.")
                                  : result.message);
        emit mapUnavailable();
        return;
    }

    QString error;
    if (!setMapMesh(result.mesh, &error)) {
        const QString message = QStringLiteral("The GPU rejected the world map: %1").arg(error);
        m_mapLoader->completeInstallation(revision, false, message);
        setRendererDiagnostic(message);
        emit mapUnavailable();
        return;
    }

    m_mapLoader->completeInstallation(revision, true);
    m_rendererMessage.clear();
    applyTrackedCamera();
    markFallbackGeometryDirty();
    refreshOverlays();
    update();
}

void EarthLarView::setLarViewMode(LarViewMode mode) {
    if (mode == LarViewMode::Grid)
        return;
    m_viewMode = mode;
    setMapPresentation(mode == LarViewMode::Sphere ? lar::map::MapPresentation::Sphere
                                                   : lar::map::MapPresentation::Mercator);
    applyTrackedCamera();
    updateZoneBackend();
    markFallbackGeometryDirty();
    update();
    refreshOverlays();
}

void EarthLarView::setSceneState(const LarSceneState &state) {
    if (!state.hasScene) {
        clearState();
        return;
    }
    setState(state.plane, state.target, state.availableFields);
}

void EarthLarView::clearScene() {
    clearState();
}

void EarthLarView::setCameraState(const ViewportCameraState &state) {
    if (m_cameraState.mode == state.mode && m_cameraState.turnWithPlane == state.turnWithPlane &&
        m_cameraState.trackingActive == state.trackingActive &&
        m_cameraState.hasAnchor == state.hasAnchor &&
        m_cameraState.anchorRadians == state.anchorRadians &&
        m_cameraState.bearingRadians == state.bearingRadians)
        return;
    m_cameraState = state;
    applyTrackedCamera();
    updateZoneBackend();
    markFallbackGeometryDirty();
    update();
    refreshOverlays();
}

void EarthLarView::setState(const Plane &plane, const Target &target,
                            const QBitArray &availableFields) {
    const bool geometryChanged =
        !m_hasState ||
        EarthCameraPolicy::fieldsChanged(
            m_plane, m_target, m_availableFields, m_hasState, plane, target, availableFields,
            {StateField::IrPos0, StateField::IrPos1, StateField::IrR, StateField::IzPos0,
             StateField::IzPos1, StateField::IzTheta1, StateField::IzTheta2, StateField::IzR1,
             StateField::IzR2});
    const bool markerChanged =
        !m_hasState || EarthCameraPolicy::fieldsChanged(m_plane, m_target, m_availableFields,
                                                        m_hasState, plane, target, availableFields,
                                                        {StateField::Location0,
                                                         StateField::Location1, StateField::Euler0,
                                                         StateField::IzPos0, StateField::IzPos1});
    m_plane = plane;
    m_target = target;
    m_availableFields = availableFields;
    m_hasState = true;
    updateZoneBackend();
    if (geometryChanged)
        markFallbackGeometryDirty();
    if (geometryChanged || markerChanged) {
        update();
        refreshOverlays();
    }
}

void EarthLarView::clearState() {
    m_hasState = false;
    m_availableFields.clear();
    updateZoneBackend();
    markFallbackGeometryDirty();
    update();
    refreshOverlays();
}
