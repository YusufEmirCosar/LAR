#pragma once

/**
 * @file earth_lar_view.h
 * @brief Mercator/sphere LAR page combining map, zones, and marker overlays.
 */

#include "domain/state.h"
#include "viewer/lar_camera.h"
#include "viewer/lar_geodesic_geometry.h"
#include "viewer/map/earth_map_widget.h"
#include "viewer/map/map_asset_source.h"
#include "viewer/viewport/earth_camera_policy.h"
#include "viewer/viewport/earth_map_load_controller.h"
#include "viewer/viewport/earth_overlay_coordinator.h"
#include "viewer/viewport/lar_marker_layer.h"
#include "viewer/viewport/lar_navigation_overlay.h"
#include "viewer/viewport/lar_parametric_zone_gpu_layer.h"
#include "viewer/viewport/lar_viewport_page.h"
#include "viewer/viewport/lar_zone_gpu_layer.h"
#include "viewer/viewport/viewport_camera_controller.h"

#include <QBitArray>
#include <QMetaObject>

#include <memory>

class QMouseEvent;
class QPainter;
class QResizeEvent;

/**
 * @brief Earth-backed viewport page with asynchronous map loading.
 *
 * Base map and LAR zones render in OpenGL; labels and entity markers render in
 * a transparent child widget using the same MapCamera projection.
 */
class EarthLarView final : public lar::map::EarthMapWidget, public IEarthLarViewportPage {
    Q_OBJECT

  public:
    explicit EarthLarView(std::shared_ptr<const lar::map::IMapAssetSource> assetSource,
                          QWidget *parent = nullptr);
    ~EarthLarView() override;

    bool ensureMapLoaded();
    bool isMapLoaded() const noexcept {
        return m_mapLoader && m_mapLoader->isLoaded();
    }

    QWidget &widget() noexcept override {
        return *this;
    }
    LarViewportPageEvents &events() noexcept override {
        return m_pageEvents;
    }
    bool ensureAvailable() override {
        return ensureMapLoaded();
    }
    void setEarthViewMode(LarViewMode mode) override {
        setLarViewMode(mode);
    }
    void setSceneState(const LarSceneState &state) override;
    void clearScene() override;
    void setLarViewMode(LarViewMode mode);
    void setCameraState(const ViewportCameraState &state) override;
    void setState(const Plane &plane, const Target &target, const QBitArray &availableFields);
    void clearState();
    void fitToData() override;

  signals:
    void frameRendered();
    void freeMovementRequested();
    void diagnosticRaised(const QString &message);
    /**
     * @brief Maps unavailable.
     */
    void mapUnavailable();

  public slots:
    void cleanupGL() override;

  protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    [[nodiscard]] QPointF zoomAnchorForWheel(const QPointF &cursorPosition) const noexcept override;

  private:
    void refreshOverlays();
    void setRendererDiagnostic(const QString &message);
    void markFallbackGeometryDirty(bool requestNow = true);
    void applyTrackedCamera();
    void rotateSphere(const QPoint &delta);
    void installMapAsset(quint64 revision, const lar::map::MapAssetReadResult &result);
    void requestOverlayPreparation();
    void updateZoneBackend();
    LarZoneRenderState zoneRenderState() const;
    bool projectMarker(double latitudeRadians, double longitudeRadians, QPointF &screen,
                       bool &visible) const;
    LarMarkerState markerState() const;
    [[nodiscard]] double navigationMetersPerPixel() const noexcept;

    Plane m_plane{};
    Target m_target{};
    QBitArray m_availableFields;
    LarViewMode m_viewMode = LarViewMode::Mercator;
    ViewportCameraState m_cameraState;
    bool m_hasState = false;
    QString m_rendererMessage;

    LarZoneGpuLayer *m_zoneGpuLayer = nullptr;
    LarParametricZoneGpuLayer *m_parametricZoneGpuLayer = nullptr;
    LarViewportPageEvents m_pageEvents;

    LarMarkerLayer *m_markerOverlay = nullptr;
    LarNavigationOverlay *m_navigationOverlay = nullptr;
    QMetaObject::Connection m_dragConnection;
    EarthMapLoadController *m_mapLoader = nullptr;
    EarthOverlayCoordinator *m_overlayCoordinator = nullptr;
    bool m_parametricBackendReady = false;
    bool m_useParametricBackend = false;
};
