#include "tests/support/architecture_test_support.h"

class ArchitectureTests final : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void modeCoordinatorRejectsIllegalTransitions();
    void capturePublishesOnlyAcceptedRawPackets();
    void failedPolicyInstallRetainsLastAppliedWhitelist();
    void facadePreservesOnlineRecordingAndOfflineWorkflows();
    void facadeStopsOnlineAfterDiscardingRecording();
    void facadeRejectsInactiveAndStaleStatePublications();
    void threadedRuntimeLoadsAndReplaysWithThreeWorkers();
};

void ArchitectureTests::initTestCase() {
    qRegisterMetaType<SessionSnapshot>("SessionSnapshot");
    qRegisterMetaType<QVector<CapturedPacket>>("QVector<CapturedPacket>");
    qRegisterMetaType<RuntimeStateSource>("RuntimeStateSource");
    qRegisterMetaType<IpAccessPolicy>("IpAccessPolicy");
}

void ArchitectureTests::modeCoordinatorRejectsIllegalTransitions() {
    ModeCoordinator modes;
    QString error;

    QVERIFY(!modes.transitionTo(ApplicationMode::Playing, &error));
    QVERIFY(error.contains(QStringLiteral("Illegal")));
    QCOMPARE(modes.mode(), ApplicationMode::Idle);

    QVERIFY(modes.transitionTo(ApplicationMode::Online, &error));
    QVERIFY(modes.transitionTo(ApplicationMode::Recording, &error));
    QVERIFY(modes.transitionTo(ApplicationMode::RecordingPaused, &error));
    QVERIFY(!modes.transitionTo(ApplicationMode::Playing, &error));
    QCOMPARE(modes.mode(), ApplicationMode::RecordingPaused);
}

void ArchitectureTests::capturePublishesOnlyAcceptedRawPackets() {
    FakeDatagramSource source;
    FakePacketDecoder decoder;
    MetricsService metrics;
    OnlineCaptureService capture(source, decoder, metrics);
    capture.setMapping(timeOnlyMapping());

    QSignalSpy rawSpy(&capture, &OnlineCaptureService::rawPacketReceived);
    QSignalSpy stateSpy(&capture, &OnlineCaptureService::stateReceived);
    QSignalSpy errorSpy(&capture, &OnlineCaptureService::captureError);

    QString error;
    QVERIFY(capture.start(9000, &error));
    source.deliver(QByteArrayLiteral("rejected"));
    source.deliver(QByteArrayLiteral("accepted"));

    QCOMPARE(errorSpy.size(), 1);
    QCOMPARE(rawSpy.size(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(stateSpy.size(), 1, 100);
    QCOMPARE(metrics.totalProcessedPackets(), quint64(1));
}

void ArchitectureTests::failedPolicyInstallRetainsLastAppliedWhitelist() {
    FacadeTestContext context;
    IpAccessPolicy whitelist;
    QString error;
    QVERIFY(QtIpAccessPolicyRepository::parseText(QByteArrayLiteral("127.0.0.1\n"), &whitelist,
                                                  &error));
    QSignalSpy completionSpy(&context.facade, &ApplicationFacade::ipAccessPolicyChangeFinished);

    QVERIFY(context.facade.setIpAccessPolicy(whitelist));
    QCOMPARE(completionSpy.size(), 1);
    QVERIFY(completionSpy.constLast().at(0).toBool());
    QCOMPARE(qvariant_cast<IpAccessPolicy>(completionSpy.constLast().at(1)).mode(),
             IpAccessPolicy::Mode::WhitelistOnly);

    context.datagramSource.acceptPolicyChanges = false;
    QVERIFY(context.facade.setIpAccessPolicy(IpAccessPolicy{}));
    QCOMPARE(completionSpy.size(), 2);
    QVERIFY(!completionSpy.constLast().at(0).toBool());
    QCOMPARE(qvariant_cast<IpAccessPolicy>(completionSpy.constLast().at(1)).mode(),
             IpAccessPolicy::Mode::WhitelistOnly);
    context.datagramSource.acceptPolicyChanges = true;

    QVERIFY(context.facade.loadMapping(QStringLiteral("mapping.json")));
    QVERIFY(context.facade.startOnline(45454));
    QVERIFY(context.runtime.setIpAccessPolicy(IpAccessPolicy{}));
    // The facade did not dispatch this request, so its completion is stale
    // from the facade's perspective and must not mutate or publish policy.
    QCOMPARE(completionSpy.size(), 2);
    QCOMPARE(qvariant_cast<IpAccessPolicy>(completionSpy.constLast().at(1)).mode(),
             IpAccessPolicy::Mode::WhitelistOnly);
}

void ArchitectureTests::facadePreservesOnlineRecordingAndOfflineWorkflows() {
    FacadeTestContext context;

    QVERIFY(context.facade.loadMapping(QStringLiteral("mapping.json")));
    QCOMPARE(context.facade.mappedFieldCount(), 1);
    QCOMPARE(context.facade.minimumPacketSize(), 8);

    QVERIFY(context.facade.startOnline(45454));
    QVERIFY(context.facade.isListening());
    QCOMPARE(context.viewModel.mode(), ApplicationMode::Online);

    QVERIFY(context.facade.startRecording());
    QVERIFY(context.facade.isRecording());
    QCOMPARE(context.viewModel.mode(), ApplicationMode::Recording);

    context.datagramSource.deliver(QByteArrayLiteral("accepted"));
    QCOMPARE(context.transaction.recordCount(), quint64(1));
    QCOMPARE(context.viewModel.recordedPacketCount(), quint64(1));

    context.facade.pauseRecording();
    QVERIFY(context.facade.isRecordingPaused());
    QCOMPARE(context.viewModel.mode(), ApplicationMode::RecordingPaused);

    context.facade.resumeRecording();
    QVERIFY(context.facade.isRecording());
    QCOMPARE(context.viewModel.mode(), ApplicationMode::Recording);

    QVERIFY(context.facade.stopRecording(QStringLiteral("saved.lar")));
    QVERIFY(!context.facade.hasRecordingSession());
    QVERIFY(!context.facade.isListening());
    QCOMPARE(context.viewModel.mode(), ApplicationMode::Idle);

    QVERIFY(context.facade.stopOnline());
    QVERIFY(!context.facade.isListening());
    QCOMPARE(context.viewModel.mode(), ApplicationMode::Idle);

    context.reader.items = {stateItem(0.0, 10.0), stateItem(1.0, 20.0)};
    QVERIFY(context.facade.loadSession(QStringLiteral("session.lar")));
    QVERIFY(context.facade.isSessionLoaded());
    QCOMPARE(context.facade.loadedRecordCount(), 2);
    QCOMPARE(context.viewModel.mode(), ApplicationMode::SessionLoaded);

    context.facade.closeSession();
    QVERIFY(!context.facade.isSessionLoaded());
    QCOMPARE(context.viewModel.mode(), ApplicationMode::Idle);
}

void ArchitectureTests::facadeStopsOnlineAfterDiscardingRecording() {
    FacadeTestContext context;
    QVERIFY(context.facade.loadMapping(QStringLiteral("mapping.json")));

    QVERIFY(context.facade.startOnline(45454));
    QVERIFY(context.facade.startRecording());
    QCOMPARE(context.viewModel.recordedPacketCount(), quint64(0));
    QVERIFY(context.facade.stopOnlineAndDiscardRecording());
    QVERIFY(!context.facade.isListening());
    QVERIFY(!context.facade.hasRecordingSession());
    QCOMPARE(context.transaction.recordCount(), quint64(0));

    QVERIFY(context.facade.startOnline(45454));
    QVERIFY(context.facade.startRecording());
    context.datagramSource.deliver(QByteArrayLiteral("accepted"));
    QCOMPARE(context.viewModel.recordedPacketCount(), quint64(1));
    QVERIFY(context.facade.stopOnlineAndDiscardRecording());
    QVERIFY(!context.facade.isListening());
    QVERIFY(!context.facade.hasRecordingSession());
    QCOMPARE(context.transaction.recordCount(), quint64(0));
}

void ArchitectureTests::facadeRejectsInactiveAndStaleStatePublications() {
    FacadeTestContext context;
    QVERIFY(context.facade.loadMapping(QStringLiteral("mapping.json")));
    QVERIFY(context.facade.startOnline(45454));

    DecodedState accepted;
    accepted.target.time = 10.0;
    accepted.availableFields = QBitArray(StateField::Count);
    accepted.availableFields.setBit(StateField::Time);
    context.runtime.stateReady({{RuntimeStateSource::Online, 1}, accepted});
    QCOMPARE(context.viewModel.target().time, 10.0);

    DecodedState stale = accepted;
    stale.target.time = 9.0;
    context.runtime.stateReady({{RuntimeStateSource::Online, 2}, stale});
    QCOMPARE(context.viewModel.target().time, 10.0);

    DecodedState inactive = accepted;
    inactive.target.time = 11.0;
    context.runtime.stateReady({{RuntimeStateSource::Playback, 1}, inactive});
    QCOMPARE(context.viewModel.target().time, 10.0);

    context.runtime.sessionClosed({{}, {RuntimeStateSource::Playback, 1}, true, {}});
    QCOMPARE(context.viewModel.target().time, 10.0);

    context.runtime.sessionLoadFinished({{},
                                         {RuntimeStateSource::Playback, 1},
                                         true,
                                         QStringLiteral("stale.lar"),
                                         3,
                                         sessionTimestamp(2.0),
                                         {}});
    QVERIFY(!context.facade.isSessionLoaded());
    QCOMPARE(context.viewModel.mode(), ApplicationMode::Online);

    context.runtime.metricsChanged({{RuntimeStateSource::Online, 1}, 42, 7});
    QCOMPARE(context.viewModel.processedPacketCount(), quint64(42));

    context.reader.items = {stateItem(0.0, 20.0)};
    QVERIFY(context.facade.loadSession(QStringLiteral("session.lar")));
    QVERIFY(context.facade.isSessionLoaded());
    QCOMPARE(context.viewModel.mode(), ApplicationMode::SessionLoaded);
    QCOMPARE(context.viewModel.processedPacketCount(), quint64(0));

    context.runtime.onlineStateChanged({{RuntimeStateSource::Online, 1}, true, 45454});
    context.runtime.metricsChanged({{RuntimeStateSource::Online, 1}, 99, 99});
    context.runtime.playbackPositionChanged(
        {{RuntimeStateSource::Playback, 1}, sessionTimestamp(99.0)});
    context.runtime.playbackPlayingChanged({{RuntimeStateSource::Playback, 1}, true});
    context.runtime.sessionClosed({{}, {RuntimeStateSource::Playback, 1}, true, {}});
    context.runtime.sessionLoadFinished({{},
                                         {RuntimeStateSource::Playback, 3},
                                         true,
                                         QStringLiteral("superseded.lar"),
                                         4,
                                         sessionTimestamp(4.0),
                                         {}});

    QVERIFY(!context.facade.isListening());
    QVERIFY(context.facade.isSessionLoaded());
    QCOMPARE(context.facade.loadedRecordCount(), 1);
    QCOMPARE(context.viewModel.mode(), ApplicationMode::SessionLoaded);
    QCOMPARE(context.viewModel.processedPacketCount(), quint64(0));
    QCOMPARE(context.viewModel.playbackPosition().milliseconds(), qint64(0));
}

void ArchitectureTests::threadedRuntimeLoadsAndReplaysWithThreeWorkers() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("threaded-runtime.lar"));

    const PacketMapping mapping = timeOnlyMapping();
    Plane plane{};
    Target target{};
    QString error;
    LarSessionWriter writer;
    QtSessionPersistence persistence;
    QVERIFY2(writer.begin(mapping.json(), &error), qPrintable(error));
    for (int index = 0; index < 1000; ++index) {
        target.time = index;
        QVERIFY2(writer.append(sessionTimestamp(double(index) / 1000.0),
                               mapping.encode(plane, target), &error),
                 qPrintable(error));
    }
    SessionSnapshot snapshot;
    QVERIFY2(writer.createSnapshot(&snapshot, &error), qPrintable(error));
    QVERIFY2(persistence.save(snapshot, path, &error), qPrintable(error));
    writer.cancel();

    ThreadedApplicationRuntime runtime;
    QTRY_COMPARE_WITH_TIMEOUT(runtime.runningWorkerThreadCount(), 3, 1000);
    QSignalSpy loadedSpy(&runtime, &IApplicationRuntime::sessionLoadFinished);
    QSignalSpy finishedSpy(&runtime, &IApplicationRuntime::playbackFinished);
    QSignalSpy stateSpy(&runtime, &IApplicationRuntime::stateReady);
    QSignalSpy recordingSpy(&runtime, &IApplicationRuntime::recordingStateChanged);
    QSignalSpy saveSpy(&runtime, &IApplicationRuntime::recordingSaveFinished);

    QVERIFY(runtime.loadSession(path));
    QTRY_COMPARE_WITH_TIMEOUT(loadedSpy.size(), 1, 3000);
    const auto loadResult = qvariant_cast<SessionLoadResult>(loadedSpy.constFirst().at(0));
    QVERIFY(loadResult.loaded);
    QCOMPARE(loadResult.recordCount, 1000);

    stateSpy.clear();
    QVERIFY(runtime.setPlaybackRate(120.0));
    QVERIFY(runtime.play());
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 3000);
    QVERIFY(!stateSpy.isEmpty());

    const QString recordedPath = directory.filePath(QStringLiteral("threaded-persistence.lar"));
    QVERIFY(runtime.startRecording(mapping.json()));
    QTRY_VERIFY_WITH_TIMEOUT(
        !recordingSpy.isEmpty() &&
            qvariant_cast<RecordingStateEvent>(recordingSpy.constLast().at(0)).hasSession,
        1000);
    QVERIFY(runtime.stopRecording(recordedPath));
    QTRY_COMPARE_WITH_TIMEOUT(saveSpy.size(), 1, 3000);
    QVERIFY(qvariant_cast<RecordingSaveResult>(saveSpy.constFirst().at(0)).saved);
    QVERIFY(QFileInfo::exists(recordedPath));
    LarSessionReader persistedReader;
    QVERIFY2(persistedReader.loadFile(recordedPath, &error), qPrintable(error));
    QCOMPARE(persistedReader.recordCount(), 0);

    runtime.shutdown();
    QCOMPARE(runtime.runningWorkerThreadCount(), 0);
}

QTEST_GUILESS_MAIN(ArchitectureTests)
#include "architecture_tests.moc"
