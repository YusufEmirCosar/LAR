
#include "viewer/mainwindow.h"

#include "domain/statefield.h"
#include "viewer/dialogs/qt_recording_file_dialog.h"
#include "viewer/dialogs/qt_viewer_file_dialog.h"
#include "viewer/help/help_context.h"
#include "viewer/help/help_window.h"
#include "viewer/hud/dlz_control_panel.h"
#include "viewer/hud/dlz_hud_workspace.h"
#include "viewer/map/packaged_map_asset_source.h"
#include "viewer/panels/metrics_panel.h"
#include "viewer/panels/offline_panel.h"
#include "viewer/panels/online_panel.h"
#include "viewer/panels/values_panel.h"
#include "viewer/plane/plane_view_workspace.h"
#include "viewer/viewport/earth_lar_view.h"
#include "viewer/viewport/grid_lar_view.h"
#include "viewer/viewport/lar_viewport.h"
#include "viewer/viewport/viewport_controls.h"
#include "viewer/workflows/online_workflow_controller.h"
#include "viewer/workflows/recording_workflow_controller.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

#include <memory>

namespace {

constexpr int OnlineMode = 0;
constexpr int OfflineMode = 1;

} // namespace

MainWindow::MainWindow(ApplicationFacade &app, QWidget *parent) : QMainWindow(parent), m_app(app) {
    m_ownedRecordingDialog = std::make_unique<QtRecordingFileDialog>(this);
    initialize(*m_ownedRecordingDialog);
}

MainWindow::MainWindow(ApplicationFacade &app, IRecordingFileDialog &recordingDialog,
                       QWidget *parent)
    : QMainWindow(parent), m_app(app) {
    initialize(recordingDialog);
}

void MainWindow::initialize(IRecordingFileDialog &recordingDialog) {
    m_recordingWorkflow = new RecordingWorkflowController(m_app, recordingDialog, this);
    m_ownedViewerFileDialog = std::make_unique<QtViewerFileDialog>(this);
    m_onlineWorkflow = new OnlineWorkflowController(m_app, *m_ownedViewerFileDialog, this);
    setWindowTitle(QStringLiteral("LAR Packet Monitor"));
    resize(1160, 720);
    setMinimumSize(820, 560);

    auto *central = new QWidget;
    auto *centralLayout = new QHBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    auto *controlsScroll = new QScrollArea;
    controlsScroll->setObjectName(QStringLiteral("controlsColumn"));
    controlsScroll->setWidget(buildSidebar());
    controlsScroll->setWidgetResizable(true);
    controlsScroll->setFrameShape(QFrame::NoFrame);
    controlsScroll->setMinimumWidth(300);
    controlsScroll->setMaximumWidth(360);
    centralLayout->addWidget(controlsScroll);

    m_valuesPanel = new ValuesPanel;

    auto mapSource =
        std::make_shared<lar::map::PackagedMapAssetSource>(QCoreApplication::applicationDirPath());
    auto hudWorkspace =
        std::make_unique<dlz::presentation::HudWorkspace>(m_valuesPanel->hudControlPanel());
    m_viewport = new LarViewport(
        std::make_unique<GridLarView>(), std::make_unique<EarthLarView>(mapSource),
        std::make_unique<PlaneViewWorkspace>(QCoreApplication::applicationDirPath(), mapSource),
        std::move(hudWorkspace));
    m_viewport->setObjectName(QStringLiteral("larViewport"));
    centralLayout->addWidget(m_viewport, 1);
    m_viewportControls = new ViewportControls(m_viewport);
    m_viewport->setOverlayWidget(m_viewportControls);
    m_viewportControls->show();
    m_viewportControls->raise();

    connect(m_viewportControls, &ViewportControls::viewModeRequested, m_viewport,
            &LarViewport::setViewMode);
    connect(m_viewportControls, &ViewportControls::cameraTrackingModeRequested, this,
            [this](CameraTrackingMode mode) {
                m_viewport->setCameraTrackingMode(mode);
                updateCameraControls();
            });
    connect(m_viewportControls, &ViewportControls::turnWithPlaneRequested, m_viewport,
            &LarViewport::setTurnWithPlane);
    connect(m_viewport, &LarViewport::viewModeChanged, this,
            [this](LarViewMode mode) { m_viewportControls->setViewMode(mode); });
    connect(m_viewport, &LarViewport::cameraTrackingModeChanged, this,
            [this](CameraTrackingMode mode) {
                m_viewportControls->setCameraTrackingMode(mode);
                m_viewportControls->setTurnWithPlane(m_viewport->turnWithPlane());
                updateCameraControls();
            });
    connect(m_viewport, &LarViewport::turnWithPlaneChanged, m_viewportControls,
            &ViewportControls::setTurnWithPlane);
    connect(m_viewport, &LarViewport::diagnosticRaised, this, &MainWindow::showDiagnostic);

    connect(m_viewport, &LarViewport::contentModeChanged, this, [this](ViewportContentMode mode) {
        m_valuesPanel->setContentMode(mode);
        switch (mode) {
        case ViewportContentMode::Lar:
            m_helpContextTopic = QStringLiteral("lar-views");
            break;
        case ViewportContentMode::Plane:
            m_helpContextTopic = QStringLiteral("plane-view");
            break;
        case ViewportContentMode::Hud:
            m_helpContextTopic = QStringLiteral("dlz-view");
            break;
        }
    });
    centralLayout->addWidget(m_valuesPanel);

    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Online mode: load a mapping to begin"));
    connectFacade();
    switchMode(OnlineMode);

    QFile styleFile(QStringLiteral(":/styles/mainwindow.qss"));
    if (styleFile.open(QIODevice::ReadOnly)) {
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }
}

MainWindow::~MainWindow() = default;

QWidget *MainWindow::buildSidebar() {
    auto *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setMinimumWidth(300);
    sidebar->setMaximumWidth(360);
    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(16, 16, 16, 14);
    layout->setSpacing(12);

    auto *brandRow = new QHBoxLayout;
    brandRow->setContentsMargins(0, 0, 0, 0);
    brandRow->setSpacing(8);
    constexpr int HelpButtonWidth = 28;
    brandRow->addSpacing(HelpButtonWidth);
    auto *brand = new QLabel(QStringLiteral("LAR PACKET MONITOR"));
    brand->setObjectName(QStringLiteral("brand"));
    brand->setAlignment(Qt::AlignCenter);
    brandRow->addWidget(brand, 1);
    m_helpButton = new QPushButton(QStringLiteral("?"));
    m_helpButton->setObjectName(QStringLiteral("applicationHelpButton"));
    m_helpButton->setAccessibleName(QStringLiteral("Open application help"));
    m_helpButton->setToolTip(QStringLiteral("Help (F1)"));
    m_helpButton->setFixedSize(HelpButtonWidth, HelpButtonWidth);
    connect(m_helpButton, &QPushButton::clicked, this, &MainWindow::openHelp);
    brandRow->addWidget(m_helpButton);
    layout->addLayout(brandRow);

    auto *helpShortcut = new QShortcut(QKeySequence::HelpContents, this);
    helpShortcut->setObjectName(QStringLiteral("applicationHelpShortcut"));
    helpShortcut->setContext(Qt::WindowShortcut);
    connect(helpShortcut, &QShortcut::activated, this, &MainWindow::openHelp);

    auto *modeRow = new QHBoxLayout;
    modeRow->setSpacing(0);
    m_onlineButton = new QPushButton(QStringLiteral("Online"));
    m_offlineButton = new QPushButton(QStringLiteral("Offline"));
    m_onlineButton->setObjectName(QStringLiteral("onlineMode"));
    m_offlineButton->setObjectName(QStringLiteral("offlineMode"));
    m_onlineButton->setCheckable(true);
    m_offlineButton->setCheckable(true);
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_onlineButton, OnlineMode);
    modeGroup->addButton(m_offlineButton, OfflineMode);
    connect(modeGroup, &QButtonGroup::idClicked, this, &MainWindow::switchMode);
    modeRow->addWidget(m_onlineButton);
    modeRow->addWidget(m_offlineButton);
    layout->addLayout(modeRow);

    m_modeStack = new QStackedWidget;
    m_modeStack->setObjectName(QStringLiteral("modeStack"));
    m_onlinePanel = new OnlinePanel(m_app, *m_onlineWorkflow, *m_recordingWorkflow);
    m_modeStack->addWidget(m_onlinePanel);
    m_offlinePanel = new OfflinePanel(m_app);
    connect(m_offlinePanel, &OfflinePanel::sessionSelectionRequested, this,
            &MainWindow::chooseLarPath);
    m_modeStack->addWidget(m_offlinePanel);
    layout->addWidget(m_modeStack);
    layout->addStretch();

    m_metricsPanel = new MetricsPanel;
    layout->addWidget(m_metricsPanel);
    return sidebar;
}

void MainWindow::connectFacade() {
    connect(m_viewport, &LarViewport::framesPerSecondChanged, this,
            [this](int fps) { m_metricsPanel->setFramesPerSecond(fps); });
    connect(m_viewport, &LarViewport::totalFrameCountChanged, this,
            [this](quint64 count) { m_metricsPanel->setTotalFrameCount(count); });
    connect(m_metricsPanel, &MetricsPanel::resetRequested, this, [this] {
        m_viewport->resetTotalFrameCount();
        m_app.resetProcessedPacketCount();
    });

    connect(&m_app.viewModel(), &ApplicationViewModel::stateChanged, this,
            &MainWindow::updateStateFromViewModel);
    connect(&m_app.viewModel(), &ApplicationViewModel::metricsChanged, this,
            [this] { m_metricsPanel->render(m_app.viewModel()); });
    connect(&m_app.viewModel(), &ApplicationViewModel::statusChanged, this,
            [this](const QString &message) { statusBar()->showMessage(message); });
    connect(&m_app, &ApplicationFacade::errorRaised, this, &MainWindow::showDiagnostic);

    connect(&m_app, &ApplicationFacade::sessionLoaded, this,
            [this](const QString &, qint64 recordCount) {
                statusBar()->showMessage(
                    QStringLiteral("Loaded %1 UDP package(s)").arg(recordCount));
            });
}

void MainWindow::updateState(const Plane &plane, const Target &target,
                             const QBitArray &availableFields) {
    m_app.viewModel().setState(plane, target, availableFields);
}

void MainWindow::updateState(const DecodedState &state) {
    m_app.viewModel().setState(state);
}

void MainWindow::updateStateFromViewModel() {
    const auto &vm = m_app.viewModel();
    m_valuesPanel->render(vm);
    if (!vm.hasState()) {
        m_viewport->clearState();
        m_viewport->setDlzInputs({}, false);
        m_viewportControls->setNavigationReadout({}, {}, {});
        updateCameraControls();
        return;
    }

    m_viewport->setState(vm.plane(), vm.target(), vm.availableFields());
    const auto &fields = vm.availableFields();
    const auto isAvailable = [&fields](int id) {
        return id >= 0 && id < fields.size() && fields.testBit(id);
    };
    const bool hasDlzTelemetry = isAvailable(StateField::DlzRangeNm) &&
                                 isAvailable(StateField::DlzAspectDegrees) &&
                                 isAvailable(StateField::DlzAltitudeFeet);
    m_viewport->setDlzInputs(vm.dlzInputs(), hasDlzTelemetry);
    m_viewportControls->setNavigationReadout(vm.plane(), vm.target(), vm.availableFields());
    updateCameraControls();
}

void MainWindow::switchMode(int modeIndex) {
    if (!m_modeStack)
        return;
    if (modeIndex == OfflineMode && !m_app.stopOnline()) {
        m_onlineButton->setChecked(true);
        return;
    }
    m_modeStack->setCurrentIndex(modeIndex);
    if (modeIndex == OnlineMode) {
        m_onlineButton->setChecked(true);
        m_helpContextTopic = QStringLiteral("online-capture");
        m_app.closeSession();
        statusBar()->showMessage(QStringLiteral("Online mode"));
    } else {
        m_offlineButton->setChecked(true);
        m_helpContextTopic = QStringLiteral("offline-replay");
        m_app.closeSession();
        statusBar()->showMessage(QStringLiteral("Offline mode: select a .lar file"));
    }
}

void MainWindow::showDiagnostic(const QString &message) {
    statusBar()->showMessage(message, 6000);
}

void MainWindow::chooseLarPath() {
    const QString path = m_ownedViewerFileDialog->chooseSessionPath();
    if (path.isEmpty())
        return;
    m_app.loadSession(path);
}

void MainWindow::openHelp() {
    if (QApplication::focusWidget() != m_helpButton) {
        m_helpContextTopic = lar::help::currentTopic(
            QApplication::focusWidget(), m_onlinePanel, m_offlinePanel, m_viewport,
            m_modeStack != nullptr && m_modeStack->currentIndex() == OfflineMode);
    }
    if (m_helpWindow == nullptr) {
        m_helpWindow = new HelpWindow(this);
        m_helpWindow->setCurrentTopicResolver([this] { return m_helpContextTopic; });
    }
    m_helpWindow->show();
    m_helpWindow->raise();
    m_helpWindow->activateWindow();
}

void MainWindow::updateCameraControls() {
    if (m_viewportControls == nullptr) {
        return;
    }
    const auto &viewModel = m_app.viewModel();
    const bool yawAvailable =
        !viewModel.hasState() || (StateField::Euler0 < viewModel.availableFields().size() &&
                                  viewModel.availableFields().testBit(StateField::Euler0));
    if (!yawAvailable && m_viewport->turnWithPlane()) {
        m_viewport->setTurnWithPlane(false);
    }
    m_viewportControls->setTurnWithPlaneAvailability(yawAvailable);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!m_recordingWorkflow->prepareToClose()) {
        event->ignore();
        return;
    }
    m_app.shutdown();
    QMainWindow::closeEvent(event);
}
