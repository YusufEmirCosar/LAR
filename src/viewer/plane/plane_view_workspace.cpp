
#include "viewer/plane/plane_view_workspace.h"

#include "viewer/plane/plane_scene_widget.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QWheelEvent>

#include <algorithm>

PlaneViewWorkspace::PlaneViewWorkspace(QString packageDirectory, QWidget *parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("planeViewWorkspace"));
    setMinimumSize(420, 360);
    setFocusPolicy(Qt::StrongFocus);
    m_sceneWidget = new PlaneSceneWidget(std::move(packageDirectory), this);
    m_sceneWidget->setGeometry(rect());
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        m_sceneWidget->hide();
        m_openGlFallback =
            new QLabel(QStringLiteral("PLANE 3D REQUIRES A NATIVE OPENGL DISPLAY"), this);
        m_openGlFallback->setObjectName(QStringLiteral("planeOpenGlFallback"));
        m_openGlFallback->setAlignment(Qt::AlignCenter);
        m_openGlFallback->setStyleSheet(
            QStringLiteral("background:#111923;color:#dce8e3;font-weight:600;"));
    }

    m_uploadModelButton = new QPushButton(QStringLiteral("Upload Jet Model"), this);
    m_uploadModelButton->setObjectName(QStringLiteral("planeUploadModelButton"));
    m_uploadModelButton->setAccessibleName(QStringLiteral("Upload jet model"));
    m_uploadModelButton->setToolTip(QStringLiteral("Load a .gltf or .glb jet model"));

    m_surfaceButton = new QPushButton(QStringLiteral("Surface: Off"), this);
    m_surfaceButton->setObjectName(QStringLiteral("planeSurfaceButton"));
    m_surfaceButton->setCheckable(true);
    m_surfaceButton->setAccessibleName(QStringLiteral("Toggle tactical surface"));
    m_surfaceButton->setToolTip(
        QStringLiteral("Show or hide the metric ground, target, and LAR areas"));

    m_terrainButton = new QPushButton(QStringLiteral("Terrain: Off"), this);
    m_terrainButton->setObjectName(QStringLiteral("planeTerrainButton"));
    m_terrainButton->setCheckable(true);
    m_terrainButton->setAccessibleName(QStringLiteral("Toggle DTED terrain"));
    m_terrainButton->setToolTip(
        QStringLiteral("Render the nearby aircraft area from DTED0 elevation tiles"));
    m_terrainButton->setEnabled(m_sceneWidget->terrainAvailable());
    if (!m_sceneWidget->terrainAvailable()) {
        m_terrainButton->setText(QStringLiteral("Terrain: N/A"));
        m_terrainButton->setToolTip(
            QStringLiteral("No DTED0 root was found; set LAR_DTED0_ROOT to enable terrain"));
    }

    m_changeSkyboxButton = new QPushButton(QStringLiteral("Change Skybox"), this);
    m_changeSkyboxButton->setObjectName(QStringLiteral("planeChangeSkyboxButton"));
    const QString controlStyle = QStringLiteral(R"(
        QPushButton#planeUploadModelButton, QPushButton#planeChangeSkyboxButton,
        QPushButton#planeSurfaceButton, QPushButton#planeTerrainButton {
            min-height: 28px;
            padding: 3px 12px;
            background: rgba(245, 248, 246, 232);
            color: #263c31;
            border: 1px solid #789087;
            border-radius: 7px;
        }
        QPushButton#planeUploadModelButton:hover, QPushButton#planeChangeSkyboxButton:hover,
        QPushButton#planeSurfaceButton:hover, QPushButton#planeTerrainButton:hover {
            background: #c9e6d8;
            color: #154b3a;
        }
        QPushButton#planeSurfaceButton:checked, QPushButton#planeTerrainButton:checked {
            background: #2f745d;
            border-color: #c9e6d8;
            color: white;
        }
    )");
    m_uploadModelButton->setStyleSheet(controlStyle);
    m_terrainButton->setStyleSheet(controlStyle);
    m_surfaceButton->setStyleSheet(controlStyle);
    m_changeSkyboxButton->setStyleSheet(controlStyle);

    connect(m_uploadModelButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Upload Jet Model"), QString(),
            QStringLiteral("Jet Models (*.gltf *.glb);;glTF JSON (*.gltf);;Binary glTF (*.glb)"));
        if (!path.isEmpty()) {
            m_sceneWidget->loadModelFromFile(path);
        }
    });
    connect(m_surfaceButton, &QPushButton::toggled, this, [this](bool visible) {
        m_surfaceButton->setText(visible ? QStringLiteral("Surface: On")
                                         : QStringLiteral("Surface: Off"));
        m_sceneWidget->setSurfaceVisible(visible);
    });
    connect(m_terrainButton, &QPushButton::toggled, this, [this](bool visible) {
        if (m_sceneWidget->terrainAvailable()) {
            m_terrainButton->setText(visible ? QStringLiteral("Terrain: On")
                                             : QStringLiteral("Terrain: Off"));
        }
        m_sceneWidget->setTerrainVisible(visible);
    });
    connect(m_changeSkyboxButton, &QPushButton::clicked, m_sceneWidget,
            &PlaneSceneWidget::selectNextSkybox);
    connect(
        m_sceneWidget, &PlaneSceneWidget::skyboxSelectionChanged, this,
        [this](int, int count, const QString &) { m_changeSkyboxButton->setEnabled(count > 1); });
    connect(m_sceneWidget, &PlaneSceneWidget::surfaceVisibilityChanged, m_surfaceButton,
            &QPushButton::setChecked);
    connect(m_sceneWidget, &PlaneSceneWidget::terrainVisibilityChanged, m_terrainButton,
            &QPushButton::setChecked);
    connect(m_sceneWidget, &PlaneSceneWidget::frameRendered, &m_events,
            &LarViewportPageEvents::frameRendered);
    connect(m_sceneWidget, &PlaneSceneWidget::diagnosticRaised, &m_events,
            &LarViewportPageEvents::diagnosticRaised);

    m_changeSkyboxButton->setEnabled(m_sceneWidget->skyboxCount() > 1);
    resizeEvent(nullptr);
}

void PlaneViewWorkspace::setSceneState(const LarSceneState &state) {
    m_sceneWidget->setSceneState(state);
}

void PlaneViewWorkspace::clearScene() {
    m_sceneWidget->clearScene();
}

void PlaneViewWorkspace::setCameraState(const ViewportCameraState &state) {
    Q_UNUSED(state)
}

void PlaneViewWorkspace::fitToData() {
    m_sceneWidget->resetCamera();
}

void PlaneViewWorkspace::resizeEvent(QResizeEvent *event) {
    if (event != nullptr) {
        QWidget::resizeEvent(event);
    }
    m_sceneWidget->setGeometry(rect());
    if (m_openGlFallback != nullptr) {
        m_openGlFallback->setGeometry(rect());
        m_openGlFallback->show();
    }
    constexpr int ViewportMargin = 18;
    constexpr int ControlGap = 8;
    m_uploadModelButton->adjustSize();
    m_terrainButton->adjustSize();
    m_surfaceButton->adjustSize();
    m_changeSkyboxButton->adjustSize();
    const int controlHeight = std::max({m_uploadModelButton->height(), m_terrainButton->height(),
                                        m_surfaceButton->height(), m_changeSkyboxButton->height()});
    const int controlsY = std::max(0, height() - controlHeight - ViewportMargin);
    const int skyboxX = std::max(0, width() - m_changeSkyboxButton->width() - ViewportMargin);
    const int surfaceX = std::max(0, skyboxX - ControlGap - m_surfaceButton->width());
    const int terrainX = std::max(0, surfaceX - ControlGap - m_terrainButton->width());
    const int unwrappedUploadX = terrainX - ControlGap - m_uploadModelButton->width();
    const bool wrapUpload = unwrappedUploadX < ViewportMargin;
    const int uploadX = wrapUpload
                            ? std::max(0, width() - m_uploadModelButton->width() - ViewportMargin)
                            : unwrappedUploadX;
    const int uploadY =
        wrapUpload ? std::max(0, controlsY - controlHeight - ControlGap) : controlsY;
    m_uploadModelButton->setGeometry(uploadX, uploadY, m_uploadModelButton->width(),
                                     m_uploadModelButton->height());
    m_terrainButton->move(terrainX, controlsY);
    m_surfaceButton->move(surfaceX, controlsY);
    m_changeSkyboxButton->move(skyboxX, controlsY);
    if (m_openGlFallback != nullptr) {
        m_openGlFallback->raise();
    } else {
        m_sceneWidget->raise();
    }
    m_surfaceButton->raise();
    m_terrainButton->raise();
    m_changeSkyboxButton->raise();
    m_uploadModelButton->raise();
}

void PlaneViewWorkspace::forwardEvent(QEvent &event) {
    QCoreApplication::sendEvent(m_sceneWidget, &event);
    event.accept();
}

void PlaneViewWorkspace::wheelEvent(QWheelEvent *event) {
    forwardEvent(*event);
}

void PlaneViewWorkspace::mouseDoubleClickEvent(QMouseEvent *event) {
    forwardEvent(*event);
}

void PlaneViewWorkspace::mousePressEvent(QMouseEvent *event) {
    forwardEvent(*event);
}

void PlaneViewWorkspace::mouseMoveEvent(QMouseEvent *event) {
    forwardEvent(*event);
}

void PlaneViewWorkspace::mouseReleaseEvent(QMouseEvent *event) {
    forwardEvent(*event);
}
