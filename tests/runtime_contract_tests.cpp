#include "application/ports/playback_clock.h"
#include "infrastructure/runtime/network_runtime_worker.h"
#include "infrastructure/runtime/persistence_runtime_worker.h"
#include "infrastructure/runtime/playback_runtime_worker.h"
#include "infrastructure/runtime/recording_runtime_worker.h"
#include "infrastructure/runtime/threaded_application_runtime.h"
#include "tests/support/direct_runtime_harness.h"

#include <QFile>
#include <QPointer>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

namespace {

class ManualRuntimePlaybackClock final : public IPlaybackClock {
  public:
    explicit ManualRuntimePlaybackClock(QObject *parent = nullptr) : IPlaybackClock(parent) {}

    void start(int framesPerSecond) override {
        m_framesPerSecond = framesPerSecond;
        m_active = true;
    }
    void stop() override {
        m_active = false;
    }
    bool isActive() const override {
        return m_active;
    }
    void advance(int count) {
        for (int frame = 0; frame < count; ++frame)
            emit tick();
    }

    int framesPerSecond() const noexcept {
        return m_framesPerSecond;
    }

  private:
    int m_framesPerSecond = 0;
    bool m_active = false;
};

} // namespace

class RuntimeContractTests final : public QObject {
    Q_OBJECT

  private slots:
    void directRuntimeContract();
    void threadedRuntimeContract();
    void threadedOverlappingSaveRequestsKeepTheirIds();
    void threadedShutdownDestroysWorkersSynchronously();
    void playbackWorkerPublishesEveryPresentationTick();
    void directMappingReplacementCompletesWhileListening();
    void directFailedFinalSaveLeavesRecordingPaused();

  private:
    void exerciseContract(IApplicationRuntime &runtime);
};

void RuntimeContractTests::exerciseContract(IApplicationRuntime &runtime) {
    QSignalSpy mappingResults(&runtime, &IApplicationRuntime::mappingLoadFinished);
    QSignalSpy policyResults(&runtime, &IApplicationRuntime::ipAccessPolicyChangeFinished);
    QSignalSpy startResults(&runtime, &IApplicationRuntime::onlineStartFinished);
    QSignalSpy loadResults(&runtime, &IApplicationRuntime::sessionLoadFinished);
    QSignalSpy commandResults(&runtime, &IApplicationRuntime::commandFinished);

    const CommandDispatch invalidMapping = runtime.loadMapping({});
    QVERIFY(!invalidMapping.accepted);
    QVERIFY(!invalidMapping.request.isValid());
    QCOMPARE(mappingResults.size(), 0);

    const CommandDispatch mapping =
        runtime.loadMapping(QStringLiteral("/definitely/missing/lar-contract-mapping.json"));
    QVERIFY(mapping.accepted);
    QVERIFY(mapping.request.isValid());
    if (mappingResults.isEmpty())
        QVERIFY(mappingResults.wait(3000));
    QCOMPARE(mappingResults.size(), 1);
    const MappingLoadResult mappingResult =
        qvariant_cast<MappingLoadResult>(mappingResults.at(0).at(0));
    QCOMPARE(mappingResult.request, mapping.request);
    QVERIFY(!mappingResult.loaded);

    const CommandDispatch policy = runtime.setIpAccessPolicy(IpAccessPolicy{});
    QVERIFY(policy.accepted);
    if (policyResults.isEmpty())
        QVERIFY(policyResults.wait(3000));
    QCOMPARE(policyResults.size(), 1);
    const IpPolicyChangeResult policyResult =
        qvariant_cast<IpPolicyChangeResult>(policyResults.at(0).at(0));
    QCOMPARE(policyResult.request, policy.request);
    QVERIFY(!policyResult.applied);

    const CommandDispatch invalidStart = runtime.startOnline(0);
    QVERIFY(!invalidStart.accepted);
    QCOMPARE(startResults.size(), 0);

    const CommandDispatch start = runtime.startOnline(41003);
    QVERIFY(start.accepted);
    if (startResults.isEmpty())
        QVERIFY(startResults.wait(3000));
    QCOMPARE(startResults.size(), 1);
    const OnlineStartResult startResult =
        qvariant_cast<OnlineStartResult>(startResults.at(0).at(0));
    QCOMPARE(startResult.request, start.request);
    QVERIFY(!startResult.started);
    QVERIFY(startResult.epoch.isValid());

    const CommandDispatch firstLoad =
        runtime.loadSession(QStringLiteral("/definitely/missing/first-session.lar"));
    const CommandDispatch secondLoad =
        runtime.loadSession(QStringLiteral("/definitely/missing/second-session.lar"));
    QVERIFY(firstLoad.accepted);
    QVERIFY(secondLoad.accepted);
    QVERIFY(firstLoad.request != secondLoad.request);
    while (loadResults.size() < 2) {
        QVERIFY(loadResults.wait(3000));
    }
    QCOMPARE(loadResults.size(), 2);

    QSet<quint64> completionIds;
    QSet<quint64> generations;
    for (const QList<QVariant> &arguments : loadResults) {
        const SessionLoadResult result = qvariant_cast<SessionLoadResult>(arguments.at(0));
        QVERIFY(!result.loaded);
        completionIds.insert(result.request.value);
        generations.insert(result.epoch.generation);
    }
    const QSet<quint64> expectedCompletionIds{firstLoad.request.value, secondLoad.request.value};
    QCOMPARE(completionIds, expectedCompletionIds);
    QCOMPARE(generations.size(), 2);

    const QByteArray recordingMapping =
        QByteArrayLiteral(R"([{"name":"time","index":0,"offset":0,"size":8}])");
    const CommandDispatch recordingStart = runtime.startRecording(recordingMapping);
    QVERIFY(recordingStart.accepted);
    if (commandResults.isEmpty())
        QVERIFY(commandResults.wait(3000));
    QCOMPARE(commandResults.size(), 1);
    RuntimeCommandResult recordingResult =
        qvariant_cast<RuntimeCommandResult>(commandResults.constLast().at(0));
    QCOMPARE(recordingResult.request, recordingStart.request);
    QCOMPARE(recordingResult.command, RuntimeCommandKind::RecordingStart);
    QVERIFY(recordingResult.succeeded);
    commandResults.clear();

    const CommandDispatch recordingPause = runtime.pauseRecording();
    QVERIFY(recordingPause.accepted);
    if (commandResults.isEmpty())
        QVERIFY(commandResults.wait(3000));
    QCOMPARE(commandResults.size(), 1);
    recordingResult = qvariant_cast<RuntimeCommandResult>(commandResults.constLast().at(0));
    QCOMPARE(recordingResult.request, recordingPause.request);
    QCOMPARE(recordingResult.command, RuntimeCommandKind::RecordingPause);
    QVERIFY(recordingResult.succeeded);
    commandResults.clear();

    const CommandDispatch recordingResume = runtime.resumeRecording();
    QVERIFY(recordingResume.accepted);
    if (commandResults.isEmpty())
        QVERIFY(commandResults.wait(3000));
    QCOMPARE(commandResults.size(), 1);
    recordingResult = qvariant_cast<RuntimeCommandResult>(commandResults.constLast().at(0));
    QCOMPARE(recordingResult.request, recordingResume.request);
    QCOMPARE(recordingResult.command, RuntimeCommandKind::RecordingResume);
    QVERIFY(recordingResult.succeeded);
    commandResults.clear();

    const CommandDispatch recordingDiscard = runtime.discardRecording();
    QVERIFY(recordingDiscard.accepted);
    if (commandResults.isEmpty())
        QVERIFY(commandResults.wait(3000));
    QCOMPARE(commandResults.size(), 1);
    recordingResult = qvariant_cast<RuntimeCommandResult>(commandResults.constLast().at(0));
    QCOMPARE(recordingResult.request, recordingDiscard.request);
    QCOMPARE(recordingResult.command, RuntimeCommandKind::RecordingDiscard);
    QVERIFY(recordingResult.succeeded);
    commandResults.clear();

    QHash<quint64, QPair<RuntimeCommandKind, bool>> expectedCommands;
    const auto expectCommand = [&expectedCommands](const CommandDispatch &dispatch,
                                                   RuntimeCommandKind command, bool succeeded) {
        QVERIFY(dispatch.accepted);
        QVERIFY(dispatch.request.isValid());
        expectedCommands.insert(dispatch.request.value, {command, succeeded});
    };
    expectCommand(runtime.play(), RuntimeCommandKind::PlaybackPlay, false);
    expectCommand(runtime.pause(), RuntimeCommandKind::PlaybackPause, false);
    expectCommand(runtime.stop(), RuntimeCommandKind::PlaybackStop, false);
    expectCommand(runtime.seek(SessionTimestamp{}), RuntimeCommandKind::PlaybackSeek, false);
    expectCommand(runtime.setPlaybackRate(2.0), RuntimeCommandKind::PlaybackRateChange, true);
    expectCommand(runtime.setPlaybackRepeat(true), RuntimeCommandKind::PlaybackRepeatChange, true);
    expectCommand(runtime.resetMetrics(), RuntimeCommandKind::MetricsReset, true);

    while (commandResults.size() < expectedCommands.size()) {
        QVERIFY(commandResults.wait(3000));
    }
    QCOMPARE(commandResults.size(), expectedCommands.size());
    QSet<quint64> completedCommandIds;
    for (const QList<QVariant> &arguments : commandResults) {
        const RuntimeCommandResult result = qvariant_cast<RuntimeCommandResult>(arguments.at(0));
        QVERIFY(expectedCommands.contains(result.request.value));
        QVERIFY(!completedCommandIds.contains(result.request.value));
        completedCommandIds.insert(result.request.value);
        const auto expected = expectedCommands.value(result.request.value);
        QCOMPARE(result.command, expected.first);
        QCOMPARE(result.succeeded, expected.second);
        QCOMPARE(result.error.isEmpty(), result.succeeded);
    }
    QCOMPARE(completedCommandIds.size(), expectedCommands.size());

    runtime.shutdown();
    const qsizetype completedMappings = mappingResults.size();
    const CommandDispatch afterShutdown = runtime.loadMapping(QStringLiteral("ignored.json"));
    QVERIFY(!afterShutdown.accepted);
    QVERIFY(!afterShutdown.request.isValid());
    QCoreApplication::processEvents();
    QCOMPARE(mappingResults.size(), completedMappings);
}

void RuntimeContractTests::directRuntimeContract() {
    DirectRuntimeHarness harness;
    exerciseContract(harness.runtime);
}

void RuntimeContractTests::threadedRuntimeContract() {
    RuntimeWorkerFactories factories;
    factories.createNetworkWorker = [] {
        return new NetworkRuntimeWorker({}, {}, [](QObject *parent) -> IDatagramSource * {
            return new FakeDatagramSource(parent);
        });
    };
    ThreadedApplicationRuntime runtime(std::move(factories));
    exerciseContract(runtime);
}

void RuntimeContractTests::threadedOverlappingSaveRequestsKeepTheirIds() {
    RuntimeWorkerFactories factories;
    factories.createNetworkWorker = [] {
        return new NetworkRuntimeWorker({}, {}, [](QObject *parent) -> IDatagramSource * {
            return new FakeDatagramSource(parent);
        });
    };
    ThreadedApplicationRuntime runtime(std::move(factories));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSignalSpy commandResults(&runtime, &IApplicationRuntime::commandFinished);
    QSignalSpy saveResults(&runtime, &IApplicationRuntime::recordingSaveFinished);

    const CommandDispatch start = runtime.startRecording(
        QByteArrayLiteral(R"([{"name":"time","index":0,"offset":0,"size":8}])"));
    QVERIFY(start.accepted);
    QVERIFY(commandResults.wait(3000));
    const RuntimeCommandResult started =
        qvariant_cast<RuntimeCommandResult>(commandResults.constLast().at(0));
    QCOMPARE(started.request, start.request);
    QVERIFY(started.succeeded);

    const QString firstPath = directory.filePath(QStringLiteral("first.lar"));
    const QString secondPath = directory.filePath(QStringLiteral("second.lar"));
    const CommandDispatch first = runtime.snapshotRecording(firstPath);
    const CommandDispatch second = runtime.snapshotRecording(secondPath);
    QVERIFY(first.accepted);
    QVERIFY(second.accepted);
    while (saveResults.size() < 2)
        QVERIFY(saveResults.wait(3000));
    QCOMPARE(saveResults.size(), 2);

    QHash<quint64, RecordingSaveResult> resultsByRequest;
    for (const QList<QVariant> &arguments : saveResults) {
        const RecordingSaveResult result = qvariant_cast<RecordingSaveResult>(arguments.at(0));
        resultsByRequest.insert(result.request.value, result);
    }
    QVERIFY(resultsByRequest.contains(first.request.value));
    QVERIFY(resultsByRequest.contains(second.request.value));
    const RecordingSaveResult firstResult = resultsByRequest.value(first.request.value);
    const RecordingSaveResult secondResult = resultsByRequest.value(second.request.value);
    QVERIFY(firstResult.saved);
    QCOMPARE(firstResult.path, firstPath);
    QVERIFY(!secondResult.saved);
    QCOMPARE(secondResult.path, secondPath);
    runtime.shutdown();
}

void RuntimeContractTests::threadedShutdownDestroysWorkersSynchronously() {
    for (int iteration = 0; iteration < 8; ++iteration) {
        QPointer<NetworkRuntimeWorker> network;
        QPointer<RecordingRuntimeWorker> recording;
        QPointer<PlaybackRuntimeWorker> playback;
        QPointer<PersistenceRuntimeWorker> persistence;
        RuntimeWorkerFactories factories;
        factories.createNetworkWorker = [&network] {
            network = new NetworkRuntimeWorker;
            return network.data();
        };
        factories.createRecordingWorker = [&recording] {
            recording = new RecordingRuntimeWorker;
            return recording.data();
        };
        factories.createPlaybackWorker = [&playback] {
            playback = new PlaybackRuntimeWorker;
            return playback.data();
        };
        factories.createPersistenceWorker = [&persistence] {
            persistence = new PersistenceRuntimeWorker;
            return persistence.data();
        };

        ThreadedApplicationRuntime runtime(std::move(factories));
        QCOMPARE(runtime.runningWorkerThreadCount(), 3);
        QVERIFY(network && recording && playback && persistence);
        runtime.shutdown();
        QCOMPARE(runtime.runningWorkerThreadCount(), 0);
        QVERIFY(network.isNull());
        QVERIFY(recording.isNull());
        QVERIFY(playback.isNull());
        QVERIFY(persistence.isNull());
    }
}

void RuntimeContractTests::playbackWorkerPublishesEveryPresentationTick() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray mappingJson =
        QByteArrayLiteral(R"([{"name":"time","index":0,"offset":0,"size":8}])");
    PacketMapping mapping;
    QString error;
    QVERIFY2(JsonMappingRepository().loadJson(mappingJson, &mapping, &error), qPrintable(error));

    LarSessionWriter writer;
    QVERIFY2(writer.begin(mappingJson, &error), qPrintable(error));
    Plane plane{};
    Target target{};
    for (int index = 0; index < 12; ++index) {
        target.time = double(index);
        QVERIFY2(writer.append(SessionTimestamp::clampedMilliseconds(qint64(index) * 10),
                               mapping.encode(plane, target), &error),
                 qPrintable(error));
    }
    SessionSnapshot snapshot;
    QVERIFY2(writer.createSnapshot(&snapshot, &error), qPrintable(error));
    const QString path = directory.filePath(QStringLiteral("presentation.lar"));
    QVERIFY2(QtSessionPersistence().save(snapshot, path, &error), qPrintable(error));

    ManualRuntimePlaybackClock *clock = nullptr;
    PlaybackRuntimeWorker worker({}, [&clock](QObject *parent) -> IPlaybackClock * {
        clock = new ManualRuntimePlaybackClock(parent);
        return clock;
    });
    QSignalSpy states(&worker, &PlaybackRuntimeWorker::stateReady);
    worker.initialize();
    QVERIFY(clock);
    worker.loadSession(path, 9, RuntimeRequestId{1});
    QCOMPARE(states.size(), 1);

    worker.play(RuntimeRequestId{2});
    QCOMPARE(clock->framesPerSecond(), PlaybackService::ReplayFramesPerSecond);
    clock->advance(3);
    QCOMPARE(states.size(), 4);
    worker.shutdown();
}

void RuntimeContractTests::directMappingReplacementCompletesWhileListening() {
    DirectRuntimeHarness harness;
    harness.datagramSource.startSucceeds = true;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString mappingPath = directory.filePath(QStringLiteral("mapping.json"));
    QFile mappingFile(mappingPath);
    QVERIFY(mappingFile.open(QIODevice::WriteOnly));
    const QByteArray mappingJson =
        QByteArrayLiteral(R"([{"name":"time","index":0,"offset":0,"size":8}])");
    QCOMPARE(mappingFile.write(mappingJson), qint64(mappingJson.size()));
    mappingFile.close();

    QSignalSpy mappingResults(&harness.runtime, &IApplicationRuntime::mappingLoadFinished);
    const CommandDispatch initialLoad = harness.runtime.loadMapping(mappingPath);
    QVERIFY(initialLoad.accepted);
    QCOMPARE(mappingResults.size(), 1);
    QVERIFY(qvariant_cast<MappingLoadResult>(mappingResults.constLast().at(0)).loaded);
    const CommandDispatch start = harness.runtime.startOnline(41004);
    QVERIFY(start.accepted);

    const CommandDispatch replacement = harness.runtime.loadMapping(mappingPath);
    QVERIFY(replacement.accepted);
    QCOMPARE(mappingResults.size(), 2);
    const MappingLoadResult result =
        qvariant_cast<MappingLoadResult>(mappingResults.constLast().at(0));
    QCOMPARE(result.request, replacement.request);
    QVERIFY(!result.loaded);
    QVERIFY(!result.error.isEmpty());
    harness.runtime.shutdown();
}

void RuntimeContractTests::directFailedFinalSaveLeavesRecordingPaused() {
    DirectRuntimeHarness harness;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSignalSpy saveResults(&harness.runtime, &IApplicationRuntime::recordingSaveFinished);

    const CommandDispatch start = harness.runtime.startRecording(
        QByteArrayLiteral(R"([{"name":"time","index":0,"offset":0,"size":8}])"));
    QVERIFY(start.accepted);
    QVERIFY(harness.recording.isRecording());
    const CommandDispatch save =
        harness.runtime.stopRecording(directory.filePath(QStringLiteral("missing/final.lar")));
    QVERIFY(save.accepted);
    QCOMPARE(saveResults.size(), 1);
    const RecordingSaveResult result =
        qvariant_cast<RecordingSaveResult>(saveResults.constLast().at(0));
    QCOMPARE(result.request, save.request);
    QVERIFY(!result.saved);
    QVERIFY(harness.recording.isPaused());
    harness.runtime.shutdown();
}

QTEST_GUILESS_MAIN(RuntimeContractTests)
#include "runtime_contract_tests.moc"
