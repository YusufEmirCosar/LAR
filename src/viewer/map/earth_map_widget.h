#pragma once

/**
 * @file earth_map_widget.h
 * @brief Interactive OpenGL map widget shared by Earth LAR presentations.
 */

#include "viewer/map/earth_map_gpu_renderer.h"
#include "viewer/map/map_camera.h"
#include "viewer/map/map_mesh.h"

#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPoint>
#include <QRectF>

#include <memory>

class QMouseEvent;
class QWheelEvent;

namespace lar::map {

/**
 * @brief Owns map camera interaction and delegates drawing to the GPU renderer.
 */
class EarthMapWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

  public:
    explicit EarthMapWidget(QWidget *parent = nullptr);
    ~EarthMapWidget() override;

    bool setMapMesh(std::shared_ptr<const MapMesh> mesh, QString *errorMessage = nullptr);

    void setMapPresentation(MapPresentation presentation);
    [[nodiscard]] MapPresentation mapPresentation() const noexcept;
    /** Centers the spherical presentation on a geographic coordinate in degrees. */
    void setRotation(double longitudeDegrees, double latitudeDegrees);
    [[nodiscard]] QPointF rotation() const noexcept;
    [[nodiscard]] float activeZoom() const noexcept;
    /** Sets clockwise map bearing in degrees after camera normalization. */
    void setCameraBearing(float bearingDegrees);
    [[nodiscard]] float cameraBearing() const noexcept;

    void setFlatWorldBounds(const QRectF &projectedBounds);
    void setFlatCenter(const QPointF &projectedCenter);
    [[nodiscard]] QPointF flatCenterProjected() const noexcept;
    /** Applies the flat-camera zoom after `MapCamera` validation and clamping. */
    void setFlatZoom(float zoom);
    /** Applies the sphere-camera zoom after `MapCamera` validation and clamping. */
    void setSphereZoom(float zoom);

    [[nodiscard]] MercatorViewport flatViewport(int targetWidth, int targetHeight) const noexcept;
    [[nodiscard]] WorldCopyRange visibleFlatWorldCopies(int targetWidth,
                                                        int targetHeight) const noexcept;
    [[nodiscard]] QPointF screenToFlatProjected(const QPointF &screenPosition, int targetWidth,
                                                int targetHeight) const noexcept;
    void panFlatMap(const QPoint &pixelDelta);

    [[nodiscard]] bool projectGeoToScreen(double longitudeDegrees, double latitudeDegrees,
                                          QPointF &screenPosition, bool &visible) const noexcept;
    [[nodiscard]] bool isShaderValid() const noexcept;

  public slots:
    virtual void cleanupGL();

  signals:
    void mapDragged(const QPoint &delta);
    void geoCoordinateChanged(double longitude, double latitude, double zoom, bool insideMap);
    void zoomChanged(double zoom);
    void rendererError(const QString &message);
    void rendererReady();

  protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    [[nodiscard]] virtual QPointF zoomAnchorForWheel(const QPointF &cursorPosition) const noexcept;

    [[nodiscard]] bool sphereCoordinateAt(const QPointF &screenPosition, double &longitude,
                                          double &latitude) const noexcept;
    [[nodiscard]] QMatrix4x4 getProjectionMatrix(int targetWidth, int targetHeight) const noexcept;
    [[nodiscard]] const MapCamera &mapCamera() const noexcept {
        return m_camera;
    }

  private:
    MapCamera m_camera;
    EarthMapGpuRenderer m_renderer;
    QPoint m_lastMousePosition;
    QMetaObject::Connection m_contextConnection;
    QOpenGLContext *m_owningContext = nullptr;
};

} // namespace lar::map
