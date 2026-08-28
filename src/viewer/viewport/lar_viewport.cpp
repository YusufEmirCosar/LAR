
#include "viewer/viewport/lar_viewport.h"

#include "viewer/hud/dlz_hud_workspace.h"
#include "viewer/map/packaged_map_asset_source.h"
#include "viewer/plane/plane_view_workspace.h"
#include "viewer/viewport/earth_lar_view.h"
#include "viewer/viewport/grid_lar_view.h"
#include "viewer/viewport/viewport_content_switch.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QWheelEvent>

#include <utility>

LarViewport::LarViewport(QWidget *parent)
    : LarViewport({}, {}, std::make_unique<dlz::presentation::HudWorkspace>(), parent) {}

LarViewport::LarViewport(std::unique_ptr<ILarViewportPage> gridPage,
                         std::unique_ptr<IEarthLarViewportPage> earthPage,
                         std::unique_ptr<QWidget> hudPage, QWidget *parent)
    : LarViewport(std::move(gridPage), std::move(earthPage), {}, std::move(hudPage), parent) {}

LarViewport::LarViewport(std::unique_ptr<ILarViewportPage> gridPage,
                         std::unique_ptr<IEarthLarViewportPage> earthPage,
                         std::unique_ptr<ILarViewportPage> planePage,
                         std::unique_ptr<QWidget> hudPage, QWidget *parent)
    : QWidget(parent) {
    if (gridPage == nullptr) {
        gridPage = std::make_unique<GridLarView>();
    }
    std::shared_ptr<const lar::map::IMapAssetSource> defaultMapSource;
    if (earthPage == nullptr || planePage == nullptr) {
        defaultMapSource = std::make_shared<lar::map::PackagedMapAssetSource>(
            QCoreApplication::applicationDirPath());
    }
    if (earthPage == nullptr) {
        earthPage = std::make_unique<EarthLarView>(defaultMapSource);
    }
    if (planePage == nullptr) {
        planePage = std::make_unique<PlaneViewWorkspace>(QCoreApplication::applicationDirPath(),
                                                         defaultMapSource);
    }
    if (hudPage == nullptr) {
        hudPage = std::make_unique<dlz::presentation::HudWorkspace>();
    }

    setMinimumSize(420, 360);
    setFocusPolicy(Qt::StrongFocus);

    connect(&m_frameCounter, &ViewportFrameCounter::framesPerSecondChanged, this,
            &LarViewport::framesPerSecondChanged);
    connect(&m_frameCounter, &ViewportFrameCounter::totalFrameCountChanged, this,
            &LarViewport::totalFrameCountChanged);

    m_gridPage = gridPage.release();
    m_earthPage = earthPage.release();
    m_planePage = planePage.release();
    m_hudPage = hudPage.release();
    m_gridPage->widget().setParent(this);
    m_earthPage->widget().setParent(this);
    m_planePage->widget().setParent(this);
    m_hudPage->setParent(this);

    m_contentSwitch = new ViewportContentSwitch(this);
    m_contentSwitch->setObjectName(QStringLiteral("viewportContentSwitch"));
    connect(m_contentSwitch, &ViewportContentSwitch::contentModeRequested, this,
            &LarViewport::setContentMode);

    m_gridPage->widget().setObjectName(QStringLiteral("gridLarViewport"));
    m_gridPage->widget().setGeometry(rect());
    connect(&m_gridPage->events(), &LarViewportPageEvents::frameRendered, this, [this] {
        if (m_contentMode == ViewportContentMode::Lar && m_viewMode == LarViewMode::Grid) {
            m_frameCounter.recordFrame();
        }
    });
    connect(&m_gridPage->events(), &LarViewportPageEvents::freeMovementRequested, this,
            [this] { setCameraTrackingMode(CameraTrackingMode::Free); });

    m_earthPage->widget().setObjectName(QStringLiteral("earthLarViewport"));
    m_earthPage->widget().setGeometry(rect());
    m_earthPage->widget().hide();
    connect(&m_earthPage->events(), &LarViewportPageEvents::frameRendered, this, [this] {
        if (m_contentMode == ViewportContentMode::Lar && m_viewMode != LarViewMode::Grid) {
            m_frameCounter.recordFrame();
        }
    });

    connect(&m_earthPage->events(), &LarViewportPageEvents::freeMovementRequested, this,
            [this] { setCameraTrackingMode(CameraTrackingMode::Free); });

    connect(&m_earthPage->events(), &LarViewportPageEvents::diagnosticRaised, this,
            &LarViewport::diagnosticRaised);
    connect(&m_earthPage->events(), &LarViewportPageEvents::pageUnavailable, this, [this] {
        if (m_contentMode != ViewportContentMode::Lar || m_viewMode == LarViewMode::Grid) {
            return;
        }
        m_viewMode = LarViewMode::Grid;
        showActivePage();
        emit viewModeChanged(m_viewMode);
    });

    m_planePage->widget().setObjectName(QStringLiteral("planeViewport"));
    m_planePage->widget().setGeometry(rect());
    m_planePage->widget().hide();
    connect(&m_planePage->events(), &LarViewportPageEvents::frameRendered, this, [this] {
        if (m_contentMode == ViewportContentMode::Plane) {
            m_frameCounter.recordFrame();
        }
    });
    connect(&m_planePage->events(), &LarViewportPageEvents::diagnosticRaised, this,
            &LarViewport::diagnosticRaised);

    if (auto *hud = qobject_cast<dlz::presentation::HudWorkspace *>(m_hudPage)) {
        connect(hud, &dlz::presentation::HudWorkspace::frameRendered, this, [this] {
            if (m_contentMode == ViewportContentMode::Hud) {
                m_frameCounter.recordFrame();
            }
        });
        connect(hud, &dlz::presentation::HudWorkspace::diagnosticRaised, this,
                &LarViewport::diagnosticRaised);
    }

    showActivePage();
}

ILarViewportPage &LarViewport::activePage() noexcept {
    if (m_viewMode == LarViewMode::Grid) {
        return *m_gridPage;
    }
    return *m_earthPage;
}

ILarViewportPage *LarViewport::activeScenePage() noexcept {
    if (m_contentMode == ViewportContentMode::Plane)
        return m_planePage;
    if (m_contentMode == ViewportContentMode::Lar)
        return &activePage();
    return nullptr;
}

QWidget &LarViewport::activePageWidget() noexcept {
    if (m_contentMode == ViewportContentMode::Plane) {
        return m_planePage->widget();
    }
    if (m_contentMode == ViewportContentMode::Hud) {
        return *m_hudPage;
    }
    return activePage().widget();
}

void LarViewport::showActivePage() {
    const bool hudActive = m_contentMode == ViewportContentMode::Hud;
    const bool planeActive = m_contentMode == ViewportContentMode::Plane;
    const bool larActive = m_contentMode == ViewportContentMode::Lar;
    const bool gridActive = larActive && m_viewMode == LarViewMode::Grid;
    // Synchronize while the destination is still hidden so switching pages can
    // never reveal stale telemetry from an earlier activation.
    synchronizeActivePage();
    m_gridPage->widget().setVisible(gridActive);
    m_earthPage->widget().setVisible(larActive && !gridActive);
    m_planePage->widget().setVisible(planeActive);
    m_hudPage->setVisible(hudActive);
    QWidget &page = activePageWidget();
    page.setGeometry(rect());
    page.raise();
    if (m_overlayWidget != nullptr) {
        m_overlayWidget->setVisible(larActive);
    }
    raiseOverlayWidget();
    setFocusProxy(&page);
    page.update();
}

void LarViewport::applyCameraStateToActivePage() {
    if (ILarViewportPage *page = activeScenePage())
        page->setCameraState(m_cameraController.state());
}

void LarViewport::applySceneStateToActivePage() {
    ILarViewportPage *page = activeScenePage();
    if (page == nullptr || !m_hasReceivedSceneState)
        return;
    if (m_scene.hasScene)
        page->setSceneState(m_scene);
    else
        page->clearScene();
}

void LarViewport::applyDlzInputsToActivePage() {
    if (m_contentMode != ViewportContentMode::Hud || !m_hasReceivedDlzInputs)
        return;
    if (auto *hud = qobject_cast<dlz::presentation::HudWorkspace *>(m_hudPage)) {
        if (m_dlzInputsAvailable)
            hud->setExternalInputs(m_dlzInputs, true, m_dlzSource);
        else
            hud->clearExternalInputs();
    }
}

void LarViewport::synchronizeActivePage() {
    applyCameraStateToActivePage();
    applySceneStateToActivePage();
    applyDlzInputsToActivePage();
}

void LarViewport::setState(const Plane &plane, const Target &target,
                           const QBitArray &availableFields) {
    m_scene = {plane, target, availableFields, true};
    m_hasReceivedSceneState = true;
    m_cameraController.setScene(plane, target, availableFields);
    applyCameraStateToActivePage();
    applySceneStateToActivePage();
    if (!m_hasInitialFit && m_contentMode == ViewportContentMode::Lar) {
        fitToData();
    }
}

void LarViewport::setDlzInputs(const dlz::TelemetryInputs &inputs, bool available,
                               const QString &source) {
    m_dlzInputs = inputs;
    m_dlzSource = source;
    m_dlzInputsAvailable = available;
    m_hasReceivedDlzInputs = true;
    applyDlzInputsToActivePage();
}

void LarViewport::clearState() {
    m_scene = {};
    m_hasReceivedSceneState = true;
    m_dlzInputs = {};
    m_dlzSource.clear();
    m_dlzInputsAvailable = false;
    m_hasReceivedDlzInputs = true;
    m_hasInitialFit = false;
    m_cameraController.clearScene();
    synchronizeActivePage();
}

void LarViewport::resetTotalFrameCount() {
    m_frameCounter.resetTotalCount();
}

void LarViewport::setViewMode(LarViewMode mode) {
    if (mode != LarViewMode::Grid && !m_earthPage->ensureAvailable()) {

        emit diagnosticRaised(QStringLiteral("Map mode is unavailable; Grid map remains active."));
        if (m_viewMode != LarViewMode::Grid) {
            m_viewMode = LarViewMode::Grid;
            showActivePage();
            emit viewModeChanged(m_viewMode);
        }
        return;
    }
    if (m_viewMode == mode) {
        return;
    }

    m_viewMode = mode;
    if (mode != LarViewMode::Grid) {
        m_earthPage->setEarthViewMode(mode);
    }
    showActivePage();
    m_hasInitialFit = false;
    if (m_scene.hasScene && m_contentMode == ViewportContentMode::Lar) {
        fitToData();
    }
    emit viewModeChanged(mode);
}

void LarViewport::setContentMode(ViewportContentMode mode) {
    if (m_contentMode == mode) {
        showActivePage();
        return;
    }
    m_contentMode = mode;
    if (m_contentSwitch != nullptr) {
        m_contentSwitch->setContentMode(mode);
    }
    showActivePage();
    if (mode == ViewportContentMode::Lar && m_scene.hasScene && !m_hasInitialFit) {
        fitToData();
    }
    emit contentModeChanged(mode);
}

void LarViewport::setOverlayWidget(QWidget *overlay) noexcept {
    m_overlayWidget = overlay;
    if (m_overlayWidget != nullptr && m_overlayWidget->parentWidget() != this) {
        m_overlayWidget->setParent(this);
    }
    raiseOverlayWidget();
}

void LarViewport::raiseOverlayWidget() noexcept {
    if (m_overlayWidget != nullptr && m_overlayWidget->parentWidget() == this &&
        m_contentMode == ViewportContentMode::Lar) {
        m_overlayWidget->raise();
    }
    if (m_contentSwitch != nullptr) {
        m_contentSwitch->raise();
    }
}

void LarViewport::setCameraTrackingMode(CameraTrackingMode mode) {
    if (m_cameraController.mode() == mode) {
        return;
    }
    if (mode == CameraTrackingMode::FollowPlane && m_cameraController.turnWithPlane()) {
        m_cameraController.setTurnWithPlane(false);
        applyCameraStateToActivePage();
        emit turnWithPlaneChanged(false);
    }
    m_cameraController.setMode(mode);
    applyCameraStateToActivePage();
    emit cameraTrackingModeChanged(mode);
}

void LarViewport::setTurnWithPlane(bool enabled) {
    if (m_cameraController.turnWithPlane() == enabled) {
        return;
    }
    m_cameraController.setTurnWithPlane(enabled);
    applyCameraStateToActivePage();
    emit turnWithPlaneChanged(enabled);
}

void LarViewport::fitToData() {
    if (!m_scene.hasScene) {
        return;
    }
    if (m_contentMode == ViewportContentMode::Plane) {
        m_planePage->fitToData();
        return;
    }
    if (m_contentMode != ViewportContentMode::Lar) {
        return;
    }
    activePage().widget().setGeometry(rect());
    activePage().fitToData();
    m_hasInitialFit = true;
}

void LarViewport::forwardEvent(QEvent &event) {
    // An ignored child event is propagated back to this parent by Qt. Do not
    // forward that same event to the child again indefinitely.
    if (m_forwardingInputEvent) {
        event.accept();
        return;
    }

    QScopedValueRollback forwardingGuard(m_forwardingInputEvent, true);
    QCoreApplication::sendEvent(&activePageWidget(), &event);
    // LarViewport is the terminal input router for its page. Consuming an
    // event that a page did not handle prevents another propagation cycle.
    event.accept();
}

void LarViewport::wheelEvent(QWheelEvent *event) {
    forwardEvent(*event);
}

void LarViewport::mouseDoubleClickEvent(QMouseEvent *event) {
    forwardEvent(*event);
}

void LarViewport::mousePressEvent(QMouseEvent *event) {
    forwardEvent(*event);
}

void LarViewport::mouseMoveEvent(QMouseEvent *event) {
    forwardEvent(*event);
}

void LarViewport::mouseReleaseEvent(QMouseEvent *event) {
    forwardEvent(*event);
}

void LarViewport::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    m_gridPage->widget().setGeometry(rect());
    m_earthPage->widget().setGeometry(rect());
    m_planePage->widget().setGeometry(rect());
    m_hudPage->setGeometry(rect());
    const QSize switchSize = m_contentSwitch->sizeHint();
    m_contentSwitch->setGeometry((width() - switchSize.width()) / 2, 10, switchSize.width(),
                                 switchSize.height());
    activePageWidget().raise();
    raiseOverlayWidget();
}
