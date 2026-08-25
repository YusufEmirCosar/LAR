#include "viewer/mainwindow.h"

#include "application/direct_application_runtime.h"
#include "application/session_limits.h"
#include "infrastructure/mapping/json_mapping_repository.h"
#include "infrastructure/mapping/mapped_packet_decoder.h"
#include "infrastructure/network/qt_udp_datagram_source.h"
#include "infrastructure/session/lar_session_reader.h"
#include "infrastructure/session/lar_session_writer.h"
#include "infrastructure/session/qt_session_persistence.h"
#include "infrastructure/timing/qt_playback_clock.h"
#include "infrastructure/timing/qt_recording_clock.h"
#include "viewer/dialogs/recording_file_dialog.h"
#include "viewer/lar_geodesic_geometry.h"
#include "viewer/playback_timeline_mapper.h"
#include "viewer/playbacktimeformatter.h"
#include "viewer/viewport/lar_viewport.h"
#include "viewer/workflows/recording_workflow_controller.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFrame>
#include <QGroupBox>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QWheelEvent>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <limits>

struct ViewerTestContext {
    QtUdpDatagramSource udpSource;
    MappedPacketDecoder decoder;
    MetricsService metrics;
    JsonMappingRepository mappingRepository;
    LarSessionWriter writer;
    QtSessionPersistence persistence;
    QtRecordingClock recordingClock;
    LarSessionReader reader;
    QtPlaybackClock playbackClock;

    ModeCoordinator modes;
    OnlineCaptureService capture{udpSource, decoder, metrics};
    RecordingService recording{writer, recordingClock};
    PlaybackService playback{reader, playbackClock};
    DirectApplicationRuntime runtime{capture, recording,         playback,
                                     metrics, mappingRepository, persistence};
    ApplicationViewModel viewModel;
    ApplicationFacade facade{modes, runtime, viewModel};
};

class FakeRecordingFileDialog final : public IRecordingFileDialog {
  public:
    QString chooseSavePath(RecordingSaveKind kind) override {
        if (kind == RecordingSaveKind::Snapshot) {
            ++snapshotChoices;
            return snapshotPath;
        }
        ++finalSaveChoices;
        return finalSavePath;
    }

    bool confirmReset() override {
        ++resetConfirmations;
        return resetAccepted;
    }

    bool confirmDiscardOnStop() override {
        ++discardOnStopConfirmations;
        return discardOnStopAccepted;
    }

    bool confirmDiscardOnClose() override {
        ++discardConfirmations;
        return discardAccepted;
    }

    QString snapshotPath;
    QString finalSavePath;
    int snapshotChoices = 0;
    int finalSaveChoices = 0;
    int resetConfirmations = 0;
    int discardOnStopConfirmations = 0;
    int discardConfirmations = 0;
    bool resetAccepted = false;
    bool discardOnStopAccepted = false;
    bool discardAccepted = false;
};

class ViewerTests : public QObject {
    Q_OBJECT

  private slots:
    void iconsAreBundled();
    void centralLarViewportIsVisible();
    void viewportControlsStayAbovePages();
    void originalUiContractIsPreserved();
    void hudReplacesCurrentValuesColumn();
    void cameraControlsExposeRequestedModes();
    void gridCameraIsNorthUpUnlessTurningWithPlane();
    void geodesicCirclePreservesSurfaceRadius();
    void modeButtonsSwitchPages();
    void onlinePolicyButtonsDefaultToAllowAll();
    void sessionSavingControlsStartPaused();
    void recordingDialogCancellationIsNoOp();
    void recordingWorkflowConfirmsBeforeStoppingOnline();
    void playbackTimelineMathIsBounded();
    void offlineReplayRepeatAndBurstPlaceholderAreUsable();
    void currentValuesAreGroupedAndAligned();
    void currentValuesUseHumanReadableUnits();
    void viewportGridFollowsGround();
    void viewportGridRotatesWithStationaryGround();
    void viewportGridRemainsGroundLockedWhileZooming();
    void viewportGridLinesSpanTranslatedZoomedView();
    void viewportGridCellsAreSquare();
    void navigatorTracksNorth();
    void navigatorPositionIsStableAcrossZoom();
    void telemetryIndicatorsReportPaints();
    void targetMarkerIsFilledTriangle();
};

void ViewerTests::iconsAreBundled() {
    QVERIFY(!QIcon(QStringLiteral(":/icons/folder.png")).isNull());
    QVERIFY(!QIcon(QStringLiteral(":/icons/navigator.png")).isNull());
    QVERIFY(!QIcon(QStringLiteral(":/icons/reset.png")).isNull());
    QVERIFY(!QIcon(QStringLiteral(":/icons/save.png")).isNull());
    QVERIFY(!QIcon(QStringLiteral(":/icons/play.png")).isNull());
    QVERIFY(!QIcon(QStringLiteral(":/icons/play-button-empty.png")).isNull());
    QVERIFY(!QIcon(QStringLiteral(":/icons/pause.png")).isNull());
    QVERIFY(!QIcon(QStringLiteral(":/icons/trash.png")).isNull());
    QVERIFY(!QIcon(QStringLiteral(":/icons/lightning.png")).isNull());
}

void ViewerTests::centralLarViewportIsVisible() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    window.show();
    auto *viewport = window.findChild<QWidget *>(QStringLiteral("larViewport"));
    QVERIFY(viewport);
    QVERIFY(viewport->isVisible());
}

void ViewerTests::viewportControlsStayAbovePages() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    window.show();
    window.resize(1160, 720);
    QCoreApplication::processEvents();

    auto *rail = window.findChild<QGroupBox *>(QStringLiteral("viewportControlRail"));
    auto *projectionPanel = window.findChild<QFrame *>(QStringLiteral("viewportProjectionPanel"));
    auto *cameraPanel = window.findChild<QFrame *>(QStringLiteral("viewportCameraPanel"));
    auto *freeButton = window.findChild<QPushButton *>(QStringLiteral("viewportFreeButton"));
    auto *targetButton = window.findChild<QPushButton *>(QStringLiteral("viewportTargetButton"));
    auto *viewport = window.findChild<LarViewport *>(QStringLiteral("larViewport"));
    QVERIFY(rail);
    QVERIFY(projectionPanel);
    QVERIFY(cameraPanel);
    QVERIFY(freeButton);
    QVERIFY(targetButton);
    QVERIFY(viewport);
    QVERIFY(rail->isVisible());
    QVERIFY(projectionPanel->isVisible());
    QVERIFY(cameraPanel->isVisible());
    QVERIFY(projectionPanel->geometry().left() < viewport->width() / 3);
    QVERIFY(cameraPanel->geometry().right() > viewport->width() * 2 / 3);
    QVERIFY(projectionPanel->geometry().bottom() > rail->height() - 64);
    QVERIFY(cameraPanel->geometry().bottom() > rail->height() - 64);

    QTest::mouseClick(freeButton, Qt::LeftButton);
    QCOMPARE(viewport->cameraTrackingMode(), CameraTrackingMode::Free);
    QTest::mouseClick(targetButton, Qt::LeftButton);
    QCOMPARE(viewport->cameraTrackingMode(), CameraTrackingMode::FollowTarget);
}

void ViewerTests::originalUiContractIsPreserved() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    window.show();
    auto *controls = window.findChild<QWidget *>(QStringLiteral("controlsColumn"));
    auto *values = window.findChild<QWidget *>(QStringLiteral("currentValuesColumn"));
    QVERIFY(controls);
    QVERIFY(values);
    QVERIFY(controls->isVisible());
    QVERIFY(values->isVisible());
    QCOMPARE(window.minimumSize(), QSize(820, 560));
    QCOMPARE(controls->minimumWidth(), 300);
    QCOMPARE(controls->maximumWidth(), 360);
    QCOMPARE(values->minimumWidth(), 330);
    QCOMPARE(values->maximumWidth(), 410);
    QVERIFY(window.findChildren<QDockWidget *>().isEmpty());
    QVERIFY(window.findChildren<QToolButton *>().isEmpty());

    auto *brand = window.findChild<QLabel *>(QStringLiteral("brand"));
    QVERIFY(brand);
    QCOMPARE(brand->text(), QStringLiteral("LAR PACKET MONITOR"));
    QCOMPARE(brand->alignment(), Qt::AlignCenter);

    QSet<QString> groupTitles;
    for (const QGroupBox *group : window.findChildren<QGroupBox *>()) {
        groupTitles.insert(group->title());
    }
    QCOMPARE(groupTitles,
             QSet<QString>({QStringLiteral("Packet Mapping"), QStringLiteral("UDP Input"),
                            QStringLiteral("Save Session"), QStringLiteral("Offline Session"),
                            QStringLiteral("Playback"), QStringLiteral("Burst"),
                            QStringLiteral("Viewport")}));
}

void ViewerTests::hudReplacesCurrentValuesColumn() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    window.show();
    QCoreApplication::processEvents();

    auto *viewport = window.findChild<LarViewport *>(QStringLiteral("larViewport"));
    auto *valuesStack = window.findChild<QStackedWidget *>(QStringLiteral("currentValuesStack"));
    auto *valuesTitle = window.findChild<QLabel *>(QStringLiteral("columnTitle"));
    auto *planeButton =
        window.findChild<QPushButton *>(QStringLiteral("viewportPlaneContentButton"));
    auto *hudButton = window.findChild<QPushButton *>(QStringLiteral("viewportHudContentButton"));
    auto *hudPanel = window.findChild<QWidget *>(QStringLiteral("dlzControlPanel"));
    auto *udpReplayButton = window.findChild<QPushButton *>(QStringLiteral("dlzUdpReplayButton"));
    auto *sliderTestButton = window.findChild<QPushButton *>(QStringLiteral("dlzSliderTestButton"));
    auto *rangeSlider = window.findChild<QSlider *>(QStringLiteral("dlzRangeSlider"));
    QVERIFY(viewport);
    QVERIFY(valuesStack);
    QVERIFY(valuesTitle);
    QVERIFY(planeButton);
    QVERIFY(hudButton);
    QVERIFY(hudPanel);
    QVERIFY(udpReplayButton);
    QVERIFY(sliderTestButton);
    QVERIFY(rangeSlider);
    QCOMPARE(udpReplayButton->text(), QStringLiteral("UDP / Offline Replay"));
    QCOMPARE(sliderTestButton->text(), QStringLiteral("Calculation Test"));
    QCOMPARE(valuesStack->currentIndex(), 0);
    QCOMPARE(valuesTitle->text(), QStringLiteral("Current Values"));
    QVERIFY(udpReplayButton->isChecked());
    QVERIFY(!sliderTestButton->isChecked());
    QVERIFY(!hudPanel->isVisible());

    QTest::mouseClick(planeButton, Qt::LeftButton);
    QCOMPARE(viewport->contentMode(), ViewportContentMode::Plane);
    QCOMPARE(valuesStack->currentIndex(), 0);
    QCOMPARE(valuesTitle->text(), QStringLiteral("Plane Telemetry"));
    auto *planeWorkspace = window.findChild<QWidget *>(QStringLiteral("planeViewport"));
    auto *larControlRail = window.findChild<QWidget *>(QStringLiteral("viewportControlRail"));
    QVERIFY(planeWorkspace);
    QVERIFY(larControlRail);
    QVERIFY(planeWorkspace->isVisible());
    QVERIFY(!larControlRail->isVisible());

    QTest::mouseClick(hudButton, Qt::LeftButton);
    QCOMPARE(viewport->contentMode(), ViewportContentMode::Hud);
    QCOMPARE(valuesStack->currentIndex(), 1);
    QCOMPARE(valuesTitle->text(), QStringLiteral("DLZ Values"));
    QVERIFY(hudPanel->isVisible());
    const auto hudEntityHeaders = hudPanel->findChildren<QLabel *>(QStringLiteral("entityHeader"));
    QVERIFY(hudPanel->findChild<QLabel *>(QStringLiteral("dlzExternalSource")) == nullptr);
    QVERIFY(
        std::none_of(hudEntityHeaders.cbegin(), hudEntityHeaders.cend(), [](const QLabel *header) {
            return header->text() == QStringLiteral("DLZ Input Source");
        }));
    for (const auto *header : hudEntityHeaders) {
        QVERIFY(header->text() != QStringLiteral("DLZ Scenario"));
    }
    const auto hudValueNames = hudPanel->findChildren<QLabel *>(QStringLiteral("valueName"));
    QVERIFY(std::none_of(hudValueNames.cbegin(), hudValueNames.cend(), [](const QLabel *label) {
        return label->text() == QStringLiteral("Scenarios") ||
               label->text() == QStringLiteral("Preset");
    }));
    QVERIFY(!rangeSlider->isVisible());
    QTest::mouseClick(sliderTestButton, Qt::LeftButton);
    QVERIFY(sliderTestButton->isChecked());
    QVERIFY(rangeSlider->isVisible());
    QTest::mouseClick(udpReplayButton, Qt::LeftButton);
    QVERIFY(!rangeSlider->isVisible());

    auto *larButton = window.findChild<QPushButton *>(QStringLiteral("viewportLarContentButton"));
    QVERIFY(larButton);
    QTest::mouseClick(larButton, Qt::LeftButton);
    QCOMPARE(viewport->contentMode(), ViewportContentMode::Lar);
    QCOMPARE(valuesStack->currentIndex(), 0);
    QCOMPARE(valuesTitle->text(), QStringLiteral("Current Values"));
}

void ViewerTests::cameraControlsExposeRequestedModes() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    auto *projection = window.findChild<QComboBox *>(QStringLiteral("viewportModeSelector"));
    auto *camera = window.findChild<QComboBox *>(QStringLiteral("cameraTrackingSelector"));
    auto *turn = window.findChild<QCheckBox *>(QStringLiteral("turnWithPlaneCheckBox"));
    auto *viewport = window.findChild<LarViewport *>(QStringLiteral("larViewport"));
    QVERIFY(projection);
    QVERIFY(camera);
    QVERIFY(turn);
    QVERIFY(viewport);
    QCOMPARE(projection->count(), 3);
    QCOMPARE(projection->itemText(0), QStringLiteral("Grid map"));
    QCOMPARE(projection->itemText(1), QStringLiteral("Mercator"));
    QCOMPARE(projection->itemText(2), QStringLiteral("Sphere"));
    QCOMPARE(camera->count(), 3);
    QCOMPARE(camera->itemText(0), QStringLiteral("Follow plane"));
    QCOMPARE(camera->itemText(1), QStringLiteral("Follow target"));
    QCOMPARE(camera->itemText(2), QStringLiteral("Free movement"));
    QVERIFY(turn->isChecked());
    QVERIFY(turn->isEnabled());

    camera->setCurrentIndex(1);
    QCOMPARE(viewport->cameraTrackingMode(), CameraTrackingMode::FollowTarget);
    QVERIFY(!turn->isEnabled());
    camera->setCurrentIndex(2);
    QCOMPARE(viewport->cameraTrackingMode(), CameraTrackingMode::Free);
    QVERIFY(!turn->isEnabled());
    camera->setCurrentIndex(0);
    QVERIFY(turn->isEnabled());
}

void ViewerTests::gridCameraIsNorthUpUnlessTurningWithPlane() {
    LarViewport viewport;
    viewport.resize(500, 400);
    Plane plane{};
    plane.euler[0] = LarGeodesicGeometry::Pi * 0.5;
    Target target{};
    QBitArray available(StateField::Count);
    available.setBit(StateField::Location0);
    available.setBit(StateField::Location1);
    available.setBit(StateField::Euler0);
    viewport.setState(plane, target, available);

    const auto render = [&viewport] {
        QImage image(viewport.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        viewport.render(&image);
        return image;
    };
    const QImage turning = render();
    viewport.setTurnWithPlane(false);
    const QImage northUp = render();
    QVERIFY(turning != northUp);

    viewport.setCameraTrackingMode(CameraTrackingMode::FollowTarget);
    const QImage targetFollow = render();
    QCOMPARE(northUp, targetFollow);
}

void ViewerTests::geodesicCirclePreservesSurfaceRadius() {
    const GeoCoordinateRadians center{1.2, 3.13};
    constexpr double radiusMeters = 250000.0;
    const auto points =
        LarGeodesicGeometry::arc(center, radiusMeters, 0.0, LarGeodesicGeometry::TwoPi, 128);
    QCOMPARE(points.size(), size_t(129));
    for (const GeoCoordinateRadians &point : points) {
        const double deltaLongitude = point.longitude - center.longitude;
        const double cosine = std::clamp(std::sin(center.latitude) * std::sin(point.latitude) +
                                             std::cos(center.latitude) * std::cos(point.latitude) *
                                                 std::cos(deltaLongitude),
                                         -1.0, 1.0);
        const double distance = std::acos(cosine) * LarGeodesicGeometry::EarthRadiusMeters;
        QVERIFY(std::abs(distance - radiusMeters) < 0.001);
    }
}

void ViewerTests::modeButtonsSwitchPages() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("modeStack"));
    auto *online = window.findChild<QPushButton *>(QStringLiteral("onlineMode"));
    auto *offline = window.findChild<QPushButton *>(QStringLiteral("offlineMode"));
    QVERIFY(stack);
    QVERIFY(online);
    QVERIFY(offline);
    QCOMPARE(stack->currentIndex(), 0);
    offline->click();
    QCOMPARE(stack->currentIndex(), 1);
    online->click();
    QCOMPARE(stack->currentIndex(), 0);
}

void ViewerTests::onlinePolicyButtonsDefaultToAllowAll() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    auto *allowAll = window.findChild<QPushButton *>(QStringLiteral("ipAllowAllButton"));
    auto *whitelist = window.findChild<QPushButton *>(QStringLiteral("ipWhitelistButton"));
    QVERIFY(allowAll);
    QVERIFY(whitelist);
    QCOMPARE(allowAll->text(), QStringLiteral("Allow all"));
    QCOMPARE(whitelist->text(), QStringLiteral("Whitelist"));
    QVERIFY(allowAll->isCheckable());
    QVERIFY(whitelist->isCheckable());
    QVERIFY(allowAll->isChecked());
    QVERIFY(!whitelist->isChecked());
    QVERIFY(!window.findChild<QComboBox *>(QStringLiteral("ipAccessMode")));
    QVERIFY(!window.findChild<QLabel *>(QStringLiteral("ipWhitelistLabel")));
}

void ViewerTests::sessionSavingControlsStartPaused() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    auto *pause = window.findChild<QPushButton *>(QStringLiteral("recordPauseButton"));
    auto *save = window.findChild<QPushButton *>(QStringLiteral("recordSaveButton"));
    auto *reset = window.findChild<QPushButton *>(QStringLiteral("recordResetButton"));
    auto *duration = window.findChild<QLabel *>(QStringLiteral("recordDurationLabel"));
    auto *count = window.findChild<QLabel *>(QStringLiteral("recordCountLabel"));
    QVERIFY(pause);
    QVERIFY(save);
    QVERIFY(reset);
    QVERIFY(duration);
    QVERIFY(count);
    QCOMPARE(pause->text(), QString());
    QCOMPARE(save->text(), QString());
    QCOMPARE(reset->text(), QString());
    QCOMPARE(duration->text(), QStringLiteral("Duration: 00:00:000"));
    QCOMPARE(count->text(), QStringLiteral("Saved packages: 0"));
    QVERIFY(!pause->isEnabled());
    QVERIFY(!save->isEnabled());
    QVERIFY(!reset->isEnabled());
    QVERIFY(!pause->icon().isNull());
    QVERIFY(!save->icon().isNull());
    QVERIFY(!reset->icon().isNull());
}

void ViewerTests::recordingDialogCancellationIsNoOp() {
    ViewerTestContext context;
    FakeRecordingFileDialog dialog;
    RecordingWorkflowController workflow(context.facade, dialog);
    QSignalSpy errorSpy(&context.facade, &ApplicationFacade::errorRaised);
    QSignalSpy operationSpy(&context.facade, &ApplicationFacade::recordingOperationStateChanged);

    workflow.requestSnapshot();
    workflow.requestFinalSave();
    workflow.requestReset();

    QCOMPARE(dialog.snapshotChoices, 1);
    QCOMPARE(dialog.finalSaveChoices, 1);
    QCOMPARE(dialog.resetConfirmations, 1);
    QCOMPARE(errorSpy.size(), 0);
    QCOMPARE(operationSpy.size(), 0);
    QCOMPARE(context.facade.recordingOperationState(), RecordingOperationState::Idle);

    emit context.runtime.recordingStateChanged({true, false, 0, {}});
    QVERIFY(context.facade.hasRecordingSession());
    errorSpy.clear(); // Ignore the intentionally inconsistent fixture transition.
    QVERIFY(!workflow.prepareToClose());
    QCOMPARE(dialog.discardConfirmations, 1);
    QVERIFY(context.facade.hasRecordingSession());
    QCOMPARE(errorSpy.size(), 0);
}

void ViewerTests::recordingWorkflowConfirmsBeforeStoppingOnline() {
    ViewerTestContext context;
    FakeRecordingFileDialog dialog;
    RecordingWorkflowController workflow(context.facade, dialog);

    emit context.runtime.recordingStateChanged({true, false, 1, {}});
    QVERIFY(!workflow.prepareToStopOnline());
    QCOMPARE(dialog.discardOnStopConfirmations, 1);
    QVERIFY(context.facade.hasRecordingSession());

    dialog.discardOnStopAccepted = true;
    QVERIFY(workflow.prepareToStopOnline());
    QCOMPARE(dialog.discardOnStopConfirmations, 2);
    QVERIFY(!context.facade.hasRecordingSession());
}

void ViewerTests::playbackTimelineMathIsBounded() {
    const auto maximum = SessionTimestamp::fromSeconds(lar::session::MaximumDurationSeconds);
    QVERIFY(maximum.has_value());
    QCOMPARE(maximum->milliseconds(),
             static_cast<qint64>(lar::session::MaximumDurationMilliseconds));
    QVERIFY(!SessionTimestamp::fromSeconds(lar::session::MaximumDurationSeconds + 0.001));
    QVERIFY(!SessionTimestamp::fromSeconds(std::numeric_limits<double>::infinity()));

    constexpr int SliderMaximum = 10000;
    QCOMPARE(
        PlaybackTimelineMapper::fromSlider(*maximum, SliderMaximum, SliderMaximum).milliseconds(),
        maximum->milliseconds());
    QCOMPARE(PlaybackTimelineMapper::fromSlider(*maximum, 0, SliderMaximum).milliseconds(),
             qint64(0));
    QCOMPARE(PlaybackTimelineMapper::toSlider(*maximum, *maximum, SliderMaximum), SliderMaximum);
    QCOMPARE(PlaybackTimelineMapper::toSlider(SessionTimestamp{}, *maximum, SliderMaximum), 0);
    QCOMPARE(PlaybackTimeFormatter::format({}), QStringLiteral("00:00.000"));
    QCOMPARE(PlaybackTimeFormatter::format(*maximum), QStringLiteral("365d 00:00:00.000"));
}

void ViewerTests::offlineReplayRepeatAndBurstPlaceholderAreUsable() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    auto *playbackPanel = window.findChild<QGroupBox *>(QStringLiteral("playbackPanel"));
    auto *burstPanel = window.findChild<QGroupBox *>(QStringLiteral("burstPanel"));
    auto *burst = window.findChild<QPushButton *>(QStringLiteral("burstPlaybackButton"));
    auto *repeat = window.findChild<QPushButton *>(QStringLiteral("repeatPlaybackButton"));
    auto *rate = window.findChild<QLineEdit *>(QStringLiteral("playbackRateInput"));
    auto *unit = window.findChild<QLabel *>(QStringLiteral("playbackRateUnit"));
    QVERIFY(playbackPanel);
    QVERIFY(burstPanel);
    QVERIFY(burst);
    QVERIFY(repeat);
    QVERIFY(rate);
    QVERIFY(unit);
    QCOMPARE(burstPanel->title(), QStringLiteral("Burst"));
    QCOMPARE(burstPanel->parentWidget()->layout()->indexOf(burstPanel),
             burstPanel->parentWidget()->layout()->indexOf(playbackPanel) + 1);
    QVERIFY(!burst->isEnabled());
    QVERIFY(!burst->icon().isNull());
    QCOMPARE(burst->text(), QStringLiteral("Burst"));
    QCOMPARE(burst->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    QVERIFY(!repeat->isEnabled());
    QVERIFY(repeat->isCheckable());
    QVERIFY(!repeat->isChecked());
    QVERIFY(!repeat->icon().isNull());
    QCOMPARE(rate->text(), QStringLiteral("1"));
    QCOMPARE(unit->text(), QStringLiteral("x"));
    QCOMPARE(rate->alignment(), Qt::AlignRight | Qt::AlignVCenter);

    rate->setText(QStringLiteral("1.75"));
    QVERIFY(QMetaObject::invokeMethod(rate, "editingFinished", Qt::DirectConnection));
    QCOMPARE(rate->text(), QStringLiteral("1.75"));
    rate->setText(QStringLiteral("invalid"));
    QVERIFY(QMetaObject::invokeMethod(rate, "editingFinished", Qt::DirectConnection));
    QCOMPARE(rate->text(), QStringLiteral("1"));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("offline-ui.lar"));
    const PacketMapping mapping(
        {{StateField::Time, 0, 8}},
        QByteArrayLiteral(R"([{"name":"time","index":0,"offset":0,"size":8}])"));
    Plane plane{};
    Target target{};
    target.time = 1.0;
    QString error;
    LarSessionWriter sessionWriter;
    QVERIFY2(sessionWriter.begin(mapping.json(), &error), qPrintable(error));
    QVERIFY2(sessionWriter.append(SessionTimestamp{}, mapping.encode(plane, target), &error),
             qPrintable(error));
    target.time = 2.0;
    QVERIFY2(sessionWriter.append(SessionTimestamp::fromMilliseconds(100).value(),
                                  mapping.encode(plane, target), &error),
             qPrintable(error));
    SessionSnapshot snapshot;
    QVERIFY2(sessionWriter.createSnapshot(&snapshot, &error), qPrintable(error));
    QtSessionPersistence persistence;
    QVERIFY2(persistence.save(snapshot, path, &error), qPrintable(error));
    sessionWriter.cancel();
    QVERIFY2(context.facade.loadSession(path), qPrintable(context.viewModel.lastError()));
    QVERIFY(repeat->isEnabled());
    QVERIFY(!burst->isEnabled());

    repeat->click();
    QVERIFY(repeat->isChecked());
    QVERIFY(context.playback.repeatEnabled());
    repeat->click();
    QVERIFY(!repeat->isChecked());
    QVERIFY(!context.playback.repeatEnabled());

    QSignalSpy frameSpy(&context.playback, &PlaybackService::frameReady);
    burst->click();
    QCOMPARE(frameSpy.size(), 0);
    QVERIFY(!context.playback.isPlaying());
}

void ViewerTests::currentValuesAreGroupedAndAligned() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    window.show();
    QTest::qWait(10);

    auto *content = window.findChild<QWidget *>(QStringLiteral("currentValuesContent"));
    auto *scroll = window.findChild<QScrollArea *>(QStringLiteral("currentValuesScroll"));
    QVERIFY(content);
    QVERIFY(scroll);
    QVERIFY(scroll->verticalScrollBar()->maximum() > 0);

    const auto entityHeaders = content->findChildren<QLabel *>(QStringLiteral("entityHeader"));
    QCOMPARE(entityHeaders.size(), 3);
    QCOMPARE(entityHeaders.at(0)->text(), QStringLiteral("Plane Data"));
    QCOMPARE(entityHeaders.at(1)->text(), QStringLiteral("Target Data"));
    QCOMPARE(entityHeaders.at(2)->text(), QStringLiteral("DLZ Data"));
    for (const QLabel *header : entityHeaders)
        QCOMPARE(header->height(), 36);

    const auto divider = content->findChild<QFrame *>(QStringLiteral("entityDivider"));
    QVERIFY(divider);
    QCOMPARE(divider->frameShape(), QFrame::HLine);

    const auto groupHeaders = content->findChildren<QLabel *>(QStringLiteral("valueGroupHeader"));
    QStringList groupNames;
    for (const QLabel *header : groupHeaders)
        groupNames.append(header->text());
    QVERIFY(groupNames.contains(QStringLiteral("Location")));
    QVERIFY(groupNames.contains(QStringLiteral("Euler (Rotation)")));
    QVERIFY(groupNames.contains(QStringLiteral("Velocity")));
    QVERIFY(groupNames.contains(QStringLiteral("In-Zone Center")));
    QVERIFY(groupNames.contains(QStringLiteral("In-Range Center")));
    QVERIFY(groupNames.contains(QStringLiteral("Telemetry")));
    for (const QLabel *header : groupHeaders)
        QCOMPARE(header->height(), 30);

    const auto nameLabels = content->findChildren<QLabel *>(QStringLiteral("valueName"));
    const auto valueLabels = content->findChildren<QLabel *>(QStringLiteral("currentValue"));
    QCOMPARE(nameLabels.size(), 24);
    QCOMPARE(valueLabels.size(), 24);

    QSet<int> fieldIds;
    const int valueColumnX = valueLabels.constFirst()->geometry().x();
    for (const QLabel *label : nameLabels) {
        QCOMPARE(label->height(), 28);
        QVERIFY(!label->text().contains(QLatin1Char('[')));
        QVERIFY(!label->text().startsWith(QStringLiteral("Plane.")));
        QVERIFY(!label->text().startsWith(QStringLiteral("Target.")));
    }
    for (const QLabel *label : valueLabels) {
        QCOMPARE(label->height(), 28);
        QCOMPARE(label->text(), QStringLiteral("N/A"));
        QCOMPARE(label->geometry().x(), valueColumnX);
        QVERIFY(label->font().fixedPitch());
        fieldIds.insert(label->property("fieldId").toInt());
    }
    QCOMPARE(fieldIds.size(), 24);
}

void ViewerTests::currentValuesUseHumanReadableUnits() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    Plane plane{};
    plane.location[0] = 0.7;
    plane.location[2] = 3200.0;
    plane.velocity[0] = 720.0;
    Target target{};
    target.iz_theta1 = -0.45;
    target.ir_r = 26000.0;
    target.time = 1234.5;
    QBitArray available(StateField::Count, true);
    available.clearBit(StateField::Euler1);

    DecodedState decoded;
    decoded.plane = plane;
    decoded.target = target;
    decoded.dlzInputs = {20.0, 90.0, 30000.0};
    decoded.availableFields = available;

    QVERIFY(QMetaObject::invokeMethod(&window, "updateState", Qt::DirectConnection,
                                      Q_ARG(DecodedState, decoded)));

    const auto valueLabels = window.findChildren<QLabel *>(QStringLiteral("currentValue"));
    const auto valueFor = [&valueLabels](int fieldId) {
        for (QLabel *label : valueLabels) {
            if (label->property("fieldId").toInt() == fieldId)
                return label->text();
        }
        return QString();
    };

    QCOMPARE(valueFor(StateField::Location0), QStringLiteral("40.107 °"));
    QCOMPARE(valueFor(StateField::Location2), QStringLiteral("3200.000 m"));
    QCOMPARE(valueFor(StateField::Velocity0), QStringLiteral("200.000 m/s"));
    QCOMPARE(valueFor(StateField::Euler1), QStringLiteral("N/A"));
    QCOMPARE(valueFor(StateField::IzTheta1), QStringLiteral("-25.783 °"));
    QCOMPARE(valueFor(StateField::IrR), QStringLiteral("26000.000 m"));
    QCOMPARE(valueFor(StateField::Time), QStringLiteral("1234.500 s"));
    QCOMPARE(valueFor(StateField::DlzRangeNm), QStringLiteral("20.000 nm"));
    QCOMPARE(valueFor(StateField::DlzAspectDegrees), QStringLiteral("90.000 °"));
    QCOMPARE(valueFor(StateField::DlzAltitudeFeet), QStringLiteral("30000.000 ft"));
}

void ViewerTests::viewportGridFollowsGround() {
    LarViewport viewport;
    viewport.resize(500, 400);
    Target target{};
    Plane plane{};
    QBitArray available(StateField::Count);
    available.setBit(StateField::Location0);
    available.setBit(StateField::Location1);
    available.setBit(StateField::Euler0);

    const auto renderViewport = [&viewport] {
        QImage image(viewport.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        viewport.render(&image);
        return image;
    };

    viewport.setState(plane, target, available);
    const QImage initialGrid = renderViewport();

    plane.location[1] = 0.00005;
    viewport.setState(plane, target, available);
    const QImage movedGrid = renderViewport();
    QVERIFY(initialGrid != movedGrid);

    plane.location[1] = 0.0;
    plane.euler[0] = 0.7853981633974483;
    viewport.setState(plane, target, available);
    const QImage rotatedGrid = renderViewport();
    QVERIFY(initialGrid != rotatedGrid);
}

void ViewerTests::viewportGridRemainsGroundLockedWhileZooming() {
    LarViewport viewport;
    viewport.resize(800, 600);
    Plane plane{};
    plane.euler[0] = 0.35;
    Target target{};
    QBitArray available(StateField::Count);
    available.setBit(StateField::Location0);
    available.setBit(StateField::Location1);
    available.setBit(StateField::Euler0);
    viewport.setState(plane, target, available);

    const auto renderViewport = [&viewport] {
        QImage image(viewport.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        viewport.render(&image);
        return image;
    };
    const auto isGridPixel = [](const QColor &color) {
        return qAbs(color.red() - 217) <= 3 && qAbs(color.green() - 221) <= 3 &&
               qAbs(color.blue() - 218) <= 3;
    };

    const QImage before = renderViewport();
    constexpr int WheelDelta = 462;
    const double zoomFactor = std::pow(1.0015, WheelDelta);
    QWheelEvent zoomEvent(QPointF(viewport.rect().center()), QPointF(viewport.rect().center()),
                          QPoint(), QPoint(0, WheelDelta), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
    QApplication::sendEvent(&viewport, &zoomEvent);
    const QImage after = renderViewport();

    const QPointF center(viewport.width() * 0.5, viewport.height() * 0.5);
    int checked = 0;
    int matched = 0;
    const QWidget *contentSwitch =
        viewport.findChild<QWidget *>(QStringLiteral("viewportContentSwitch"));
    const QRect switchRect = contentSwitch != nullptr ? contentSwitch->geometry() : QRect{};
    for (int y = 20; y < 220; ++y) {
        for (int x = 20; x < before.width() - 20; ++x) {
            if (switchRect.contains(x, y))
                continue;
            if (!isGridPixel(before.pixelColor(x, y)))
                continue;
            const QPointF mapped = center + (QPointF(x, y) - center) * zoomFactor;
            const int mappedX = qRound(mapped.x());
            const int mappedY = qRound(mapped.y());
            if (mappedX < 3 || mappedX >= after.width() - 3 || mappedY < 3 ||
                mappedY >= after.height() - 3) {
                continue;
            }
            ++checked;
            bool found = false;
            for (int dy = -3; dy <= 3 && !found; ++dy) {
                for (int dx = -3; dx <= 3; ++dx) {
                    if (isGridPixel(after.pixelColor(mappedX + dx, mappedY + dy))) {
                        found = true;
                        break;
                    }
                }
            }
            if (found)
                ++matched;
        }
    }
    QVERIFY(checked >= 20);
    QVERIFY2(double(matched) / double(checked) >= 0.85,
             qPrintable(QStringLiteral("Only %1 of %2 tracked grid points remained fixed")
                            .arg(matched)
                            .arg(checked)));
}

void ViewerTests::viewportGridRotatesWithStationaryGround() {
    LarViewport viewport;
    viewport.resize(800, 600);
    Plane plane{};
    plane.location[0] = 0.6;
    plane.location[1] = 0.7;
    Target target{};
    QBitArray available(StateField::Count);
    available.setBit(StateField::Location0);
    available.setBit(StateField::Location1);
    available.setBit(StateField::Euler0);
    viewport.setState(plane, target, available);

    const auto renderViewport = [&viewport] {
        QImage image(viewport.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        viewport.render(&image);
        return image;
    };
    const auto isGridPixel = [](const QColor &color) {
        return qAbs(color.red() - 217) <= 3 && qAbs(color.green() - 221) <= 3 &&
               qAbs(color.blue() - 218) <= 3;
    };

    const QImage before = renderViewport();
    constexpr double yawChange = 0.7;
    plane.euler[0] = yawChange;
    viewport.setState(plane, target, available);
    const QImage after = renderViewport();

    const QPointF center(viewport.width() * 0.5, viewport.height() * 0.5);
    const double cosine = std::cos(yawChange);
    const double sine = std::sin(yawChange);
    int checked = 0;
    int matched = 0;
    const QWidget *contentSwitch =
        viewport.findChild<QWidget *>(QStringLiteral("viewportContentSwitch"));
    const QRect switchRect = contentSwitch != nullptr ? contentSwitch->geometry() : QRect{};
    for (int y = 20; y < 220; ++y) {
        for (int x = 20; x < before.width() - 20; ++x) {
            if (switchRect.contains(x, y))
                continue;
            if (!isGridPixel(before.pixelColor(x, y)))
                continue;
            const QPointF delta = QPointF(x, y) - center;
            const QPointF mapped = center + QPointF(cosine * delta.x() + sine * delta.y(),
                                                    -sine * delta.x() + cosine * delta.y());
            const int mappedX = qRound(mapped.x());
            const int mappedY = qRound(mapped.y());
            if (mappedX < 3 || mappedX >= after.width() - 3 || mappedY < 3 ||
                mappedY >= after.height() - 3) {
                continue;
            }
            ++checked;
            bool found = false;
            for (int dy = -3; dy <= 3 && !found; ++dy) {
                for (int dx = -3; dx <= 3; ++dx) {
                    if (isGridPixel(after.pixelColor(mappedX + dx, mappedY + dy))) {
                        found = true;
                        break;
                    }
                }
            }
            if (found)
                ++matched;
        }
    }
    QVERIFY(checked >= 20);
    QVERIFY2(double(matched) / double(checked) >= 0.85,
             qPrintable(QStringLiteral("Only %1 of %2 rotated grid points remained aligned")
                            .arg(matched)
                            .arg(checked)));
}

void ViewerTests::viewportGridLinesSpanTranslatedZoomedView() {
    LarViewport viewport;
    viewport.resize(800, 600);
    Plane plane{};
    plane.location[0] = 0.6;
    plane.location[1] = 0.7;
    Target target{};
    QBitArray available(StateField::Count);
    available.setBit(StateField::Location0);
    available.setBit(StateField::Location1);
    available.setBit(StateField::Euler0);
    viewport.setState(plane, target, available);

    plane.location[0] += 0.0005;
    plane.location[1] += 0.0005;
    plane.euler[0] = 0.4;
    viewport.setState(plane, target, available);
    QWheelEvent zoomEvent(QPointF(viewport.rect().center()), QPointF(viewport.rect().center()),
                          QPoint(), QPoint(0, 600), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase,
                          false);
    QApplication::sendEvent(&viewport, &zoomEvent);

    QImage image(viewport.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    viewport.render(&image);
    const auto isGridPixel = [](const QColor &color) {
        return qAbs(color.red() - 217) <= 4 && qAbs(color.green() - 221) <= 4 &&
               qAbs(color.blue() - 218) <= 4;
    };
    const auto horizontalEdgeCount = [&](int y) {
        int count = 0;
        for (int x = 0; x < image.width(); ++x)
            if (isGridPixel(image.pixelColor(x, y)))
                ++count;
        return count;
    };
    const auto verticalEdgeCount = [&](int x) {
        int count = 0;
        for (int y = 0; y < image.height(); ++y)
            if (isGridPixel(image.pixelColor(x, y)))
                ++count;
        return count;
    };

    QVERIFY(horizontalEdgeCount(1) >= 2);
    QVERIFY(horizontalEdgeCount(image.height() - 2) >= 2);
    QVERIFY(verticalEdgeCount(1) >= 2);
    QVERIFY(verticalEdgeCount(image.width() - 2) >= 2);
}

void ViewerTests::viewportGridCellsAreSquare() {
    LarViewport viewport;
    viewport.resize(500, 400);
    Plane plane{};
    plane.location[0] = 0.7;
    plane.location[1] = 0.5;
    Target target{};
    QBitArray available(StateField::Count);
    available.setBit(StateField::Location0);
    available.setBit(StateField::Location1);
    available.setBit(StateField::Euler0);
    viewport.setState(plane, target, available);

    QImage image(viewport.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    viewport.render(&image);

    const auto darkRuns = [&image](bool scanHorizontally, int fixedCoordinate) {
        QList<int> centers;
        int runStart = -1;
        const int length = scanHorizontally ? image.width() : image.height();
        for (int position = 0; position < length; ++position) {
            const QColor color = scanHorizontally ? image.pixelColor(position, fixedCoordinate)
                                                  : image.pixelColor(fixedCoordinate, position);
            const bool isGrid = color.red() < 235 && color.green() < 235 && color.blue() < 235;
            if (isGrid && runStart < 0)
                runStart = position;
            if (!isGrid && runStart >= 0) {
                centers.append((runStart + position - 1) / 2);
                runStart = -1;
            }
        }
        if (runStart >= 0)
            centers.append((runStart + length - 1) / 2);
        return centers;
    };

    const QList<int> verticalLines = darkRuns(true, 80);
    // The host-owned LAR/HUD switch occupies the first few rows at x=20 in
    // the rendered viewport.  Scan below it so the grid-spacing assertion
    // continues to measure the page rather than the selector chrome.
    const QList<int> horizontalLines = darkRuns(false, 20);
    QVERIFY(verticalLines.size() >= 2);
    QVERIFY(horizontalLines.size() >= 2);
    const double horizontalSpacing =
        double(verticalLines.constLast() - verticalLines.constFirst()) /
        double(verticalLines.size() - 1);
    const double verticalSpacing =
        double(horizontalLines.constLast() - horizontalLines.constFirst()) /
        double(horizontalLines.size() - 1);
    QVERIFY(qAbs(horizontalSpacing - verticalSpacing) <= 1.5);
}

void ViewerTests::navigatorTracksNorth() {
    LarViewport viewport;
    viewport.resize(500, 400);
    Plane plane{};
    Target target{};
    QBitArray available(StateField::Count);
    available.setBit(StateField::Euler0);

    const auto renderNavigator = [&viewport] {
        QImage image(viewport.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        viewport.render(&image);
        return image.copy(QRect(viewport.width() / 2 - 110, viewport.height() - 100, 220, 100));
    };

    viewport.setState(plane, target, available);
    const QImage northAtZeroHeading = renderNavigator();
    plane.euler[0] = 1.5707963267948966;
    viewport.setState(plane, target, available);
    const QImage northAtEastHeading = renderNavigator();
    QVERIFY(northAtZeroHeading != northAtEastHeading);
}

void ViewerTests::navigatorPositionIsStableAcrossZoom() {
    LarViewport viewport;
    viewport.resize(500, 400);
    Plane plane{};
    Target target{};
    QBitArray available(StateField::Count);
    available.setBit(StateField::Euler0);
    viewport.setState(plane, target, available);

    const auto renderNavigator = [&viewport] {
        QImage image(viewport.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        viewport.render(&image);
        return image.copy(QRect(viewport.width() / 2 + 48, viewport.height() - 53, 31, 31));
    };

    const QImage initialNavigator = renderNavigator();
    viewport.resize(500, 500);
    viewport.fitToData();
    viewport.resize(500, 400);
    const QImage zoomedNavigator = renderNavigator();
    QCOMPARE(initialNavigator, zoomedNavigator);
}

void ViewerTests::telemetryIndicatorsReportPaints() {
    ViewerTestContext context;
    MainWindow window(context.facade);
    auto *viewport = window.findChild<LarViewport *>(QStringLiteral("larViewport"));
    auto *fps = window.findChild<QLabel *>(QStringLiteral("fpsIndicator"));
    auto *totalFrames = window.findChild<QLabel *>(QStringLiteral("totalFramesIndicator"));
    auto *receivedRate = window.findChild<QLabel *>(QStringLiteral("receivedPacketRateIndicator"));
    auto *processed = window.findChild<QLabel *>(QStringLiteral("processedPacketsIndicator"));
    auto *reset = window.findChild<QPushButton *>(QStringLiteral("resetMetricsButton"));
    auto *brand = window.findChild<QLabel *>(QStringLiteral("brand"));
    QVERIFY(viewport);
    QVERIFY(fps);
    QVERIFY(totalFrames);
    QVERIFY(receivedRate);
    QVERIFY(processed);
    QVERIFY(reset);
    QVERIFY(brand);
    QCOMPARE(brand->alignment(), Qt::AlignCenter);
    QVERIFY(reset->icon().isNull());
    QCOMPARE(fps->text(), QStringLiteral("FPS: 0"));
    QCOMPARE(totalFrames->text(), QStringLiteral("Total Frame Count: 0"));
    QCOMPARE(receivedRate->text(), QStringLiteral("Received Packages/s: 0"));
    QCOMPARE(processed->text(), QStringLiteral("Processed Packets: 0"));

    QSignalSpy fpsSpy(viewport, &LarViewport::framesPerSecondChanged);
    QImage image(viewport->size(), QImage::Format_ARGB32_Premultiplied);
    for (int i = 0; i < 3; ++i)
        viewport->render(&image);
    QCOMPARE(totalFrames->text(), QStringLiteral("Total Frame Count: 3"));
    QTRY_VERIFY_WITH_TIMEOUT(!fpsSpy.isEmpty(), 1500);
    const int reportedFps = fpsSpy.constLast().at(0).toInt();
    QVERIFY(reportedFps >= 3);
    QCOMPARE(fps->text(), QStringLiteral("FPS: %1").arg(reportedFps));

    reset->click();
    QCOMPARE(totalFrames->text(), QStringLiteral("Total Frame Count: 0"));
    QCOMPARE(receivedRate->text(), QStringLiteral("Received Packages/s: 0"));
    QCOMPARE(processed->text(), QStringLiteral("Processed Packets: 0"));
}

void ViewerTests::targetMarkerIsFilledTriangle() {
    LarViewport viewport;
    viewport.resize(500, 400);
    Plane plane{};
    Target target{};
    target.iz_pos[1] = 0.0001;
    QBitArray available(StateField::Count);
    available.setBit(StateField::Location0);
    available.setBit(StateField::Location1);
    available.setBit(StateField::IzPos0);
    available.setBit(StateField::IzPos1);
    viewport.setState(plane, target, available);

    QImage image(viewport.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    viewport.render(&image);
    const QColor triangleInterior = image.pixelColor(354, 203);
    QVERIFY(triangleInterior.red() > triangleInterior.green() + 40);
    QVERIFY(triangleInterior.red() > triangleInterior.blue() + 40);
}

QTEST_MAIN(ViewerTests)
#include "viewer_tests.moc"
