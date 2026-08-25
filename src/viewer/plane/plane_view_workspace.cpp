
#include "viewer/plane/plane_view_workspace.h"

#include "viewer/plane/plane_scene_widget.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
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

    m_uploadPanel = new QGroupBox(this);
    m_uploadPanel->setObjectName(QStringLiteral("planeUploadPanel"));
    m_uploadPanel->setAccessibleName(QStringLiteral("Upload controls"));
    auto *uploadLayout = new QVBoxLayout(m_uploadPanel);
    uploadLayout->setContentsMargins(4, 4, 4, 4);
    uploadLayout->setSpacing(0);

    auto *uploadHeader = new QLabel(QStringLiteral("Upload"), m_uploadPanel);
    uploadHeader->setObjectName(QStringLiteral("planeUploadHeader"));
    uploadHeader->setAlignment(Qt::AlignCenter);
    uploadLayout->addWidget(uploadHeader);

    m_uploadModelButton = new QPushButton(QStringLiteral("Jet Model"), m_uploadPanel);
    m_uploadModelButton->setObjectName(QStringLiteral("planeUploadModelButton"));
    m_uploadModelButton->setAccessibleName(QStringLiteral("Upload jet model"));
    m_uploadModelButton->setToolTip(QStringLiteral("Load a .gltf or .glb jet model"));

    m_uploadTerrainButton = new QPushButton(QStringLiteral("DTED Folder"), m_uploadPanel);
    m_uploadTerrainButton->setObjectName(QStringLiteral("planeUploadTerrainButton"));
    m_uploadTerrainButton->setAccessibleName(QStringLiteral("Upload DTED terrain folder"));
    m_uploadTerrainButton->setToolTip(
        QStringLiteral("Use a DTED Level 1 or Level 2 folder for this session"));
    uploadLayout->addWidget(m_uploadModelButton);
    uploadLayout->addWidget(m_uploadTerrainButton);

    m_displayPanel = new QFrame(this);
    m_displayPanel->setObjectName(QStringLiteral("planeDisplayPanel"));
    auto *displayLayout = new QVBoxLayout(m_displayPanel);
    displayLayout->setContentsMargins(4, 4, 4, 4);
    displayLayout->setSpacing(0);

    m_terrainButton = new QPushButton(QStringLiteral("Terrain"), m_displayPanel);
    m_terrainButton->setObjectName(QStringLiteral("planeTerrainButton"));
    m_terrainButton->setCheckable(true);
    m_terrainButton->setAccessibleName(QStringLiteral("Toggle DTED terrain"));

    m_surfaceButton = new QPushButton(QStringLiteral("Target"), m_displayPanel);
    m_surfaceButton->setObjectName(QStringLiteral("planeSurfaceButton"));
    m_surfaceButton->setCheckable(true);
    m_surfaceButton->setAccessibleName(QStringLiteral("Toggle tactical surface"));
    m_surfaceButton->setToolTip(
        QStringLiteral("Show or hide the metric ground, target, and LAR areas"));

    m_changeSkyboxButton = new QPushButton(QStringLiteral("Skybox"), m_displayPanel);
    m_changeSkyboxButton->setObjectName(QStringLiteral("planeChangeSkyboxButton"));
    m_changeSkyboxButton->setAccessibleName(QStringLiteral("Change skybox"));
    m_changeSkyboxButton->setToolTip(QStringLiteral("Change to the next available skybox"));
    displayLayout->addWidget(m_terrainButton);
    displayLayout->addWidget(m_surfaceButton);
    displayLayout->addWidget(m_changeSkyboxButton);

    setStyleSheet(QStringLiteral(R"(
        QGroupBox#planeUploadPanel, QFrame#planeDisplayPanel {
            background: rgba(245, 248, 246, 232);
            border: 1px solid #bdc8c1;
            border-radius: 8px;
        }
        QGroupBox#planeUploadPanel {
            margin: 0;
            padding: 0;
        }
        QLabel#planeUploadHeader {
            min-height: 24px;
            background: transparent;
            border: 0;
            color: #263c31;
            font-weight: 600;
        }
        QGroupBox#planeUploadPanel QPushButton,
        QFrame#planeDisplayPanel QPushButton {
            min-width: 82px;
            min-height: 24px;
            padding: 2px 7px;
            background: #ffffff;
            color: #263c31;
            border: 1px solid #bfc8c2;
            border-radius: 0;
        }
        QGroupBox#planeUploadPanel QPushButton:hover,
        QFrame#planeDisplayPanel QPushButton:hover {
            background: #f1f5f2;
            color: #154b3a;
            border-color: #789087;
        }
        QFrame#planeDisplayPanel QPushButton:checked {
            background: #dfece6;
            border-color: #357461;
            color: #154b3a;
            font-weight: 600;
        }
        QPushButton#planeUploadModelButton, QPushButton#planeTerrainButton {
            border-top-left-radius: 5px;
            border-top-right-radius: 5px;
        }
        QPushButton#planeUploadTerrainButton, QPushButton#planeChangeSkyboxButton {
            border-bottom-left-radius: 5px;
            border-bottom-right-radius: 5px;
        }
    )"));

    connect(m_uploadModelButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Upload Jet Model"), QString(),
            QStringLiteral("Jet Models (*.gltf *.glb);;glTF JSON (*.gltf);;Binary glTF (*.glb)"));
        if (!path.isEmpty()) {
            m_sceneWidget->loadModelFromFile(path);
        }
    });
    connect(m_uploadTerrainButton, &QPushButton::clicked, this, [this] {
        const QStringList formats{QStringLiteral("DTED Level 1 (.dt1)"),
                                  QStringLiteral("DTED Level 2 (.dt2)")};
        bool accepted = false;
        const QString selected =
            QInputDialog::getItem(this, QStringLiteral("Select DTED Format"),
                                  QStringLiteral("Which format does the terrain folder contain?"),
                                  formats, 0, false, &accepted);
        if (!accepted || selected.isEmpty()) {
            return;
        }
        const DtedLevel level = selected == formats.front() ? DtedLevel::Level1 : DtedLevel::Level2;
        const QString path = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Upload %1 Folder").arg(dtedLevelDisplayName(level)), QString(),
            QFileDialog::ShowDirsOnly);
        if (!path.isEmpty()) {
            m_sceneWidget->loadTerrainFromDirectory(path, level);
        }
    });
    connect(m_surfaceButton, &QPushButton::toggled, this,
            [this](bool visible) { m_sceneWidget->setSurfaceVisible(visible); });
    connect(m_terrainButton, &QPushButton::toggled, this, [this](bool visible) {
        m_sceneWidget->setTerrainVisible(visible);
        refreshTerrainControls();
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
    connect(m_sceneWidget, &PlaneSceneWidget::terrainVisibilityChanged, this,
            [this](bool) { refreshTerrainControls(); });
    connect(m_sceneWidget, &PlaneSceneWidget::terrainAvailabilityChanged, this,
            [this](bool) { refreshTerrainControls(); });
    connect(m_sceneWidget, &PlaneSceneWidget::terrainSourceChanged, this,
            [this](DtedLevel, const QString &) { refreshTerrainControls(); });
    connect(m_sceneWidget, &PlaneSceneWidget::frameRendered, &m_events,
            &LarViewportPageEvents::frameRendered);
    connect(m_sceneWidget, &PlaneSceneWidget::diagnosticRaised, &m_events,
            &LarViewportPageEvents::diagnosticRaised);

    m_changeSkyboxButton->setEnabled(m_sceneWidget->skyboxCount() > 1);
    refreshTerrainControls();
    resizeEvent(nullptr);
}

void PlaneViewWorkspace::refreshTerrainControls() {
    const bool available = m_sceneWidget->terrainAvailable();
    m_terrainButton->setEnabled(available);
    if (!available) {
        m_terrainButton->setToolTip(
            QStringLiteral("No DTED0 root was found; upload a DT1/DT2 folder to enable terrain"));
        return;
    }
    m_terrainButton->setToolTip(
        QStringLiteral("Show or hide the nearby aircraft area from the active %1 source")
            .arg(dtedLevelDisplayName(m_sceneWidget->terrainLevel())));
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
    m_uploadPanel->adjustSize();
    m_displayPanel->adjustSize();
    m_uploadPanel->move(ViewportMargin,
                        std::max(0, height() - m_uploadPanel->height() - ViewportMargin));
    m_displayPanel->move(std::max(0, width() - m_displayPanel->width() - ViewportMargin),
                         std::max(0, height() - m_displayPanel->height() - ViewportMargin));
    if (m_openGlFallback != nullptr) {
        m_openGlFallback->raise();
    } else {
        m_sceneWidget->raise();
    }
    m_uploadPanel->raise();
    m_displayPanel->raise();
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
