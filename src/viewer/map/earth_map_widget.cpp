
#include "viewer/map/earth_map_widget.h"

#include "viewer/map/map_projection.h"

#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>

namespace lar::map {

EarthMapWidget::EarthMapWidget(QWidget *parent) : QOpenGLWidget(parent) {
    setMouseTracking(true);
}

EarthMapWidget::~EarthMapWidget() {
    cleanupGL();
}

bool EarthMapWidget::setMapMesh(std::shared_ptr<const MapMesh> mesh, QString *errorMessage) {
    if (mesh == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The map mesh is unavailable.");
        }
        return false;
    }

    double minimumLongitude = 180.0;
    double maximumLongitude = -180.0;
    for (std::size_t offset = 0U; offset + 2U < mesh->vertices.size(); offset += 3U) {
        minimumLongitude = std::min(minimumLongitude, static_cast<double>(mesh->vertices[offset]));
        maximumLongitude = std::max(maximumLongitude, static_cast<double>(mesh->vertices[offset]));
    }
    m_camera.setGeometryLongitudeBounds(minimumLongitude, maximumLongitude);
    if (!m_renderer.setMesh(std::move(mesh), errorMessage)) {
        return false;
    }

    if (isValid()) {
        makeCurrent();
        const bool uploaded = m_renderer.uploadPending(errorMessage);
        doneCurrent();
        if (!uploaded) {
            return false;
        }
        emit rendererReady();
    }
    update();
    return true;
}

void EarthMapWidget::setMapPresentation(MapPresentation presentation) {
    if (m_camera.setPresentation(presentation)) {
        update();
    }
}

MapPresentation EarthMapWidget::mapPresentation() const noexcept {
    return m_camera.presentation();
}

void EarthMapWidget::setRotation(double longitudeDegrees, double latitudeDegrees) {
    if (m_camera.setSphereCenter(longitudeDegrees, latitudeDegrees) &&
        mapPresentation() == MapPresentation::Sphere) {
        update();
    }
}

QPointF EarthMapWidget::rotation() const noexcept {
    return m_camera.sphereCenter();
}

float EarthMapWidget::activeZoom() const noexcept {
    return m_camera.activeZoom();
}

void EarthMapWidget::setCameraBearing(float bearingDegrees) {
    if (m_camera.setBearingDegrees(bearingDegrees)) {
        update();
    }
}

float EarthMapWidget::cameraBearing() const noexcept {
    return m_camera.bearingDegrees();
}

void EarthMapWidget::setFlatWorldBounds(const QRectF &projectedBounds) {
    if (m_camera.setMercatorWorldBounds(projectedBounds, width(), height())) {
        update();
    }
}

void EarthMapWidget::setFlatCenter(const QPointF &projectedCenter) {
    if (m_camera.setMercatorCenter(projectedCenter, width(), height())) {
        update();
    }
}

QPointF EarthMapWidget::flatCenterProjected() const noexcept {
    return m_camera.mercatorCenter();
}

void EarthMapWidget::setFlatZoom(float zoom) {
    if (m_camera.setMercatorZoom(zoom, width(), height())) {
        emit zoomChanged(m_camera.mercatorZoom());
        update();
    }
}

void EarthMapWidget::setSphereZoom(float zoom) {
    if (m_camera.setSphereZoom(zoom)) {
        emit zoomChanged(m_camera.sphereZoom());
        update();
    }
}

MercatorViewport EarthMapWidget::flatViewport(int targetWidth, int targetHeight) const noexcept {
    return m_camera.mercatorViewport(targetWidth, targetHeight);
}

WorldCopyRange EarthMapWidget::visibleFlatWorldCopies(int targetWidth,
                                                      int targetHeight) const noexcept {
    return m_camera.visibleWorldCopies(targetWidth, targetHeight);
}

QPointF EarthMapWidget::screenToFlatProjected(const QPointF &screenPosition, int targetWidth,
                                              int targetHeight) const noexcept {
    return m_camera.screenToMercator(screenPosition, targetWidth, targetHeight);
}

void EarthMapWidget::panFlatMap(const QPoint &pixelDelta) {
    if (m_camera.panMercator(pixelDelta, width(), height())) {
        update();
    }
}

bool EarthMapWidget::projectGeoToScreen(double longitudeDegrees, double latitudeDegrees,
                                        QPointF &screenPosition, bool &visible) const noexcept {
    return m_camera.projectGeoToScreen(longitudeDegrees, latitudeDegrees, width(), height(),
                                       screenPosition, visible);
}

bool EarthMapWidget::isShaderValid() const noexcept {
    return m_renderer.isShaderValid();
}

void EarthMapWidget::cleanupGL() {
    if (m_contextConnection) {
        disconnect(m_contextConnection);
        m_contextConnection = {};
    }

    if (m_owningContext != nullptr && isValid()) {
        makeCurrent();
        m_renderer.cleanup();
        doneCurrent();
    }
    m_owningContext = nullptr;
}

void EarthMapWidget::initializeGL() {
    initializeOpenGLFunctions();
    m_owningContext = context();
    if (m_contextConnection) {
        disconnect(m_contextConnection);
    }
    if (m_owningContext != nullptr) {
        m_contextConnection = connect(m_owningContext, &QOpenGLContext::aboutToBeDestroyed, this,
                                      &EarthMapWidget::cleanupGL, Qt::UniqueConnection);
    }

    QString error;
    if (!m_renderer.initialize(&error)) {
        emit rendererError(error);
        return;
    }
    if (m_renderer.hasUploadedMesh()) {
        emit rendererReady();
    }
}

void EarthMapWidget::resizeGL(int widthValue, int heightValue) {
    glViewport(0, 0, widthValue, heightValue);
    m_camera.constrainMercatorCenter(widthValue, heightValue);
}

void EarthMapWidget::paintGL() {
    m_renderer.draw(m_camera, width(), height());
}

void EarthMapWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastMousePosition = event->pos();
        event->accept();
        return;
    }
    QOpenGLWidget::mousePressEvent(event);
}

void EarthMapWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        const QPoint delta = event->pos() - m_lastMousePosition;
        m_lastMousePosition = event->pos();
        if (mapPresentation() == MapPresentation::Mercator) {
            panFlatMap(delta);
        } else {
            emit mapDragged(delta);
        }
        event->accept();
        return;
    }

    if (mapPresentation() == MapPresentation::Mercator) {
        const QPointF projected = screenToFlatProjected(event->position(), width(), height());
        const double longitude = MapProjection::wrapLongitude(projected.x());
        const double latitude = MapProjection::unprojectLatitude(projected.y());
        const bool insideMap = (projected.y() >= -MapProjection::HalfWorldDegrees) &&
                               (projected.y() <= MapProjection::HalfWorldDegrees);
        emit geoCoordinateChanged(longitude, latitude, activeZoom(), insideMap);
    } else {
        double longitude = 0.0;
        double latitude = 0.0;
        const bool insideMap = sphereCoordinateAt(event->position(), longitude, latitude);
        emit geoCoordinateChanged(longitude, latitude, activeZoom(), insideMap);
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void EarthMapWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        event->accept();
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void EarthMapWidget::wheelEvent(QWheelEvent *event) {
    const QPointF anchor = zoomAnchorForWheel(event->position());
    if (m_camera.zoomAt(anchor, event->angleDelta().y(), width(), height())) {

        emit zoomChanged(activeZoom());
        update();
        event->accept();
        return;
    }
    QOpenGLWidget::wheelEvent(event);
}

QPointF EarthMapWidget::zoomAnchorForWheel(const QPointF &cursorPosition) const noexcept {
    return cursorPosition;
}

bool EarthMapWidget::sphereCoordinateAt(const QPointF &screenPosition, double &longitude,
                                        double &latitude) const noexcept {
    return m_camera.sphereCoordinateAt(screenPosition, width(), height(), longitude, latitude);
}

QMatrix4x4 EarthMapWidget::getProjectionMatrix(int targetWidth, int targetHeight) const noexcept {
    return m_camera.projectionMatrix(targetWidth, targetHeight);
}

} // namespace lar::map
