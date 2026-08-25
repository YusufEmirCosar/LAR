
#include "viewer/plane/plane_scene_widget.h"

#include "domain/statefield.h"
#include "viewer/grid_geometry_builder.h"
#include "viewer/plane/glb_model_reader.h"
#include "viewer/plane/plane_aircraft_scale.h"
#include "viewer/plane/plane_attitude_transform.h"
#include "viewer/plane/plane_surface_projection.h"

#include <QCoreApplication>
#include <QDir>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QSurfaceFormat>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

bool fieldAvailable(const QBitArray &fields, int field) noexcept {
    return field >= 0 && field < fields.size() && fields.testBit(field);
}

bool validGroundPosition(double latitude, double longitude) noexcept {
    constexpr double Pi = 3.14159265358979323846;
    constexpr double HalfPi = Pi * 0.5;
    return std::isfinite(latitude) && std::isfinite(longitude) && latitude >= -HalfPi &&
           latitude <= HalfPi && longitude >= -Pi && longitude <= Pi;
}

} // namespace

PlaneSceneWidget::PlaneSceneWidget(QString packageDirectory, QWidget *parent)
    : QOpenGLWidget(parent), m_packageDirectory(QDir::cleanPath(std::move(packageDirectory))),
      m_skyboxes(QDir(m_packageDirectory).filePath(QStringLiteral("assets/cubemaps"))) {
    setObjectName(QStringLiteral("planeSceneWidget"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    QSurfaceFormat requested = format();
    requested.setDepthBufferSize(24);
    requested.setSamples(4);
    setFormat(requested);

    initializeTerrainSource();
}

PlaneSceneWidget::~PlaneSceneWidget() {
    stopTerrainWorker();
    cleanupGL();
}

void PlaneSceneWidget::setDiagnostic(const QString &message) {
    if (m_diagnostic == message) {
        return;
    }
    m_diagnostic = message;
    if (!message.isEmpty()) {
        emit diagnosticRaised(message);
    }
    update();
}

void PlaneSceneWidget::setSceneState(const LarSceneState &state) {
    if (!m_surfaceGroundOrigin && state.hasScene &&
        fieldAvailable(state.availableFields, StateField::Location0) &&
        fieldAvailable(state.availableFields, StateField::Location1) &&
        validGroundPosition(state.plane.location[0], state.plane.location[1])) {
        m_surfaceGroundOrigin =
            GeoCoordinateRadians{state.plane.location[0], state.plane.location[1]};
    }
    m_scene = state;
    refreshSurfaceState();
    updateTerrainPlacement();
    requestTerrainIfNeeded();
    const QQuaternion orientation = PlaneAttitudeTransform::orientation(
        m_scene.plane, m_scene.availableFields, &m_attitudeComplete);
    Q_UNUSED(orientation)
    update();
}

void PlaneSceneWidget::refreshSurfaceState() {
    m_surfaceState = PlaneSurfaceProjection::project(m_scene, m_surfaceMetersPerSceneUnit,
                                                     m_surfaceGroundOrigin);
    m_renderer.setSurfaceState(m_surfaceState);
}

void PlaneSceneWidget::clearScene() {
    ++m_terrainRevision;
    m_scene = {};
    m_surfaceGroundOrigin.reset();
    m_terrainRequestAnchor.reset();
    m_failedTerrainAnchor.reset();
    m_terrainPatch.reset();
    m_terrainPending = false;
    m_renderer.setTerrainPatch(nullptr);
    refreshSurfaceState();
    m_attitudeComplete = false;
    m_dragging = false;
    unsetCursor();
    if (QWidget::mouseGrabber() == this) {
        releaseMouse();
    }
    update();
}

void PlaneSceneWidget::resetCamera() {
    m_orbitCamera.reset();
    update();
}

void PlaneSceneWidget::setSurfaceVisible(bool visible) {
    if (m_surfaceVisible == visible) {
        return;
    }
    m_surfaceVisible = visible;
    m_orbitCamera.setGroundConstrained(visible || m_terrainVisible);
    m_renderer.setSurfaceVisible(visible);
    emit surfaceVisibilityChanged(visible);
    update();
}

bool PlaneSceneWidget::loadSkybox(int index) {
    CubemapFaces faces;
    QString error;
    if (!m_skyboxes.load(index, &faces, &error) || !m_renderer.setSkybox(faces, &error)) {
        setDiagnostic(error.isEmpty() ? QStringLiteral("The selected skybox is unavailable.")
                                      : error);
        return false;
    }
    m_skyboxIndex = index;
    if (m_diagnostic.startsWith(QStringLiteral("The selected skybox"))) {
        m_diagnostic.clear();
    }
    emit skyboxSelectionChanged(m_skyboxIndex, m_skyboxes.count(), skyboxName());
    update();
    return true;
}

bool PlaneSceneWidget::selectNextSkybox() {
    if (m_skyboxes.count() <= 0) {
        setDiagnostic(QStringLiteral("No packaged skyboxes were found."));
        return false;
    }
    return loadSkybox((m_skyboxIndex + 1) % m_skyboxes.count());
}

bool PlaneSceneWidget::loadModelFromFile(const QString &path) {
    const GlbModelReadResult model = GlbModelReader::readFile(path);
    if (!model.succeeded()) {
        setDiagnostic(model.message.isEmpty()
                          ? QStringLiteral("The selected jet model could not be loaded.")
                          : model.message);
        return false;
    }

    m_model = model.mesh;
    m_surfaceMetersPerSceneUnit =
        PlaneAircraftScale::metersPerSceneUnit(m_model->forwardExtentSceneUnits);
    refreshSurfaceState();
    m_terrainPatch.reset();
    m_renderer.setTerrainPatch(nullptr);
    requestTerrainIfNeeded(true);
    m_renderer.setModel(m_model);
    m_renderer.setSurfaceVisible(m_surfaceVisible);
    m_renderer.setTerrainVisible(m_terrainVisible);
    m_diagnostic.clear();
    update();
    return true;
}

void PlaneSceneWidget::initializeGL() {
    if (context() != nullptr) {
        m_contextConnection = connect(context(), &QOpenGLContext::aboutToBeDestroyed, this,
                                      &PlaneSceneWidget::cleanupGL, Qt::UniqueConnection);
    }
    if (m_model == nullptr) {
        const QString modelPath =
            QDir(m_packageDirectory).filePath(QStringLiteral("assets/models/f16_3.glb"));
        if (!loadModelFromFile(modelPath)) {
            return;
        }
    } else {
        m_renderer.setModel(m_model);
    }
    m_renderer.setSurfaceVisible(m_surfaceVisible);

    if (m_skyboxes.count() > 0) {
        loadSkybox(std::clamp(m_skyboxIndex, 0, m_skyboxes.count() - 1));
    } else {
        emit diagnosticRaised(QStringLiteral("No packaged skyboxes were found."));
    }
    if (m_skyboxes.rejectedSetCount() > 0) {
        emit diagnosticRaised(QStringLiteral("Skipped %1 incomplete or mismatched cubemap set(s).")
                                  .arg(m_skyboxes.rejectedSetCount()));
    }
    QString error;
    if (!m_renderer.initialize(&error)) {
        setDiagnostic(error);
        return;
    }
    m_diagnostic.clear();
}

double PlaneSceneWidget::headingRadians() const noexcept {
    if (!m_scene.hasScene || !fieldAvailable(m_scene.availableFields, StateField::Euler0) ||
        !std::isfinite(m_scene.plane.euler[0])) {
        return 0.0;
    }
    return m_scene.plane.euler[0];
}

void PlaneSceneWidget::paintGL() {
    if (m_renderer.ready()) {
        const double cameraDistance = m_orbitCamera.distance();
        const float nearClip = static_cast<float>(std::max(0.1, cameraDistance / 100'000.0));
        const double surfaceDistance =
            std::hypot(static_cast<double>(m_surfaceState.surfaceHalfExtent),
                       static_cast<double>(m_surfaceState.surfaceHeight));
        double terrainDistance = 0.0;
        if (m_terrainVisible && m_terrainPatch) {
            double altitudeMeters = m_terrainPatch->centerElevationMeters -
                                    static_cast<double>(m_surfaceState.surfaceHeight) *
                                        m_terrainPatch->metersPerSceneUnit;
            if (fieldAvailable(m_scene.availableFields, StateField::Location2) &&
                std::isfinite(m_scene.plane.location[2])) {
                altitudeMeters = m_scene.plane.location[2];
            }
            const double verticalMeters =
                std::max(std::abs(m_terrainPatch->minimumElevationMeters - altitudeMeters),
                         std::abs(m_terrainPatch->maximumElevationMeters - altitudeMeters));
            terrainDistance = std::hypot(m_terrainPatch->halfExtentMeters * 1.5, verticalMeters) /
                              m_terrainPatch->metersPerSceneUnit;
        }
        const float farClip = static_cast<float>(
            std::max({250.0, cameraDistance * 4.0, surfaceDistance * 2.0, terrainDistance * 1.5}));
        QMatrix4x4 projection;
        projection.perspective(45.0F,
                               static_cast<float>(std::max(1, width())) /
                                   static_cast<float>(std::max(1, height())),
                               nearClip, farClip);
        const QMatrix4x4 view = m_orbitCamera.viewMatrix(headingRadians());
        const QQuaternion attitude = PlaneAttitudeTransform::orientation(
            m_scene.plane, m_scene.availableFields, &m_attitudeComplete);
        QString error;
        if (!m_renderer.draw(view, projection, attitude, &error)) {
            setDiagnostic(error.isEmpty() ? QStringLiteral("Plane GPU upload failed.") : error);
        }
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!m_diagnostic.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#ffb4a8")));
        painter.setBrush(QColor(8, 12, 20, 220));
        painter.drawText(rect().adjusted(30, 70, -30, -30),
                         Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_diagnostic);
    } else if (!m_scene.hasScene) {
        painter.setPen(QColor(QStringLiteral("#e5ece9")));
        painter.drawText(rect().adjusted(30, 80, -30, -30), Qt::AlignCenter,
                         QStringLiteral("WAITING FOR PLANE DATA"));
    } else if (!m_attitudeComplete) {
        painter.setPen(QColor(QStringLiteral("#ffd27a")));
        painter.drawText(rect().adjusted(24, 72, -24, -24), Qt::AlignHCenter | Qt::AlignTop,
                         QStringLiteral("ATTITUDE DATA INCOMPLETE"));
    }
    const QString instructions =
        QStringLiteral("Drag to orbit  •  Wheel to zoom  •  Double-click to reset");
    painter.setPen(QColor(235, 242, 239, 210));
    if (m_surfaceVisible || m_terrainVisible) {
        const QFontMetrics metrics(painter.font());
        const QRect instructionsRect(18, height() - 86,
                                     metrics.horizontalAdvance(instructions) + 22, 31);
        painter.setBrush(QColor(8, 12, 20, 190));
        painter.drawRoundedRect(instructionsRect, 7.0, 7.0);
        painter.drawText(instructionsRect, Qt::AlignCenter, instructions);
        QString statusLabel;
        if (m_surfaceVisible && m_terrainVisible) {
            statusLabel =
                QStringLiteral("Grid: %1  •  Terrain: %2")
                    .arg(GridGeometryBuilder::formatDistance(m_surfaceState.gridSpacingMeters),
                         terrainPatchReady() ? QStringLiteral("DTED0") : QStringLiteral("Loading"));
        } else if (m_surfaceVisible) {
            statusLabel =
                QStringLiteral("Grid: %1")
                    .arg(GridGeometryBuilder::formatDistance(m_surfaceState.gridSpacingMeters));
        } else {
            statusLabel = terrainPatchReady() ? QStringLiteral("Terrain: DTED0")
                                              : QStringLiteral("Terrain: Loading");
        }
        const QRect labelRect(18, height() - 49, metrics.horizontalAdvance(statusLabel) + 22, 31);
        painter.setPen(QColor(235, 242, 239, 235));
        painter.setBrush(QColor(8, 12, 20, 190));
        painter.drawRoundedRect(labelRect, 7.0, 7.0);
        painter.drawText(labelRect, Qt::AlignCenter, statusLabel);
    } else {
        painter.drawText(rect().adjusted(18, 18, -18, -14), Qt::AlignLeft | Qt::AlignBottom,
                         instructions);
    }
    painter.end();
    emit frameRendered();
}

void PlaneSceneWidget::resizeGL(int widthValue, int heightValue) {
    glViewport(0, 0, widthValue, heightValue);
}

void PlaneSceneWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastMousePosition = event->position();
        m_dragging = true;
        setCursor(Qt::ClosedHandCursor);
        grabMouse();
        event->accept();
        return;
    }
    QOpenGLWidget::mousePressEvent(event);
}

void PlaneSceneWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPointF delta = event->position() - m_lastMousePosition;
        m_lastMousePosition = event->position();
        m_orbitCamera.orbit(delta, size());
        update();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void PlaneSceneWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        unsetCursor();
        if (QWidget::mouseGrabber() == this) {
            releaseMouse();
        }
        event->accept();
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void PlaneSceneWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    resetCamera();
    event->accept();
}

void PlaneSceneWidget::wheelEvent(QWheelEvent *event) {
    m_orbitCamera.zoom(event->angleDelta().y());
    update();
    event->accept();
}

void PlaneSceneWidget::cleanupGL() {
    if (m_contextConnection) {
        disconnect(m_contextConnection);
        m_contextConnection = {};
    }
    if (isValid()) {
        makeCurrent();
        m_renderer.cleanup();
        doneCurrent();
    }
}
