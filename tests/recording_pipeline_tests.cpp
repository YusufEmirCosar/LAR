#include "tests/support/architecture_test_support.h"

class RecordingPipelineTests final : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void networkDrainRequiresExactBoundedBatchAcknowledgements();
    void snapshotDrainBuffersPostBoundaryPackets();
    void recordingRetainsFailedTransactionAndExcludesPauseTime();
    void recordingUsesTransportReceiptTime();
    void recordingCoordinatorRejectsStalePersistenceCompletions();
    void overlappingSaveRequestsKeepTheirRequestIds();
    void pausedSnapshotDoesNotCapturePausedPackets();
};

void RecordingPipelineTests::initTestCase() {
    qRegisterMetaType<SessionSnapshot>("SessionSnapshot");
    qRegisterMetaType<QVector<CapturedPacket>>("QVector<CapturedPacket>");
    qRegisterMetaType<RuntimeStateSource>("RuntimeStateSource");
    qRegisterMetaType<IpAccessPolicy>("IpAccessPolicy");
}

void RecordingPipelineTests::networkDrainRequiresExactBoundedBatchAcknowledgements() {
    auto *source = new FakeDatagramSource;
    NetworkRuntimeWorker worker(std::make_unique<FakeMappingRepository>(),
                                std::make_unique<FakePacketDecoder>(),
                                [source](QObject *parent) -> IDatagramSource * {
                                    source->setParent(parent);
                                    return source;
                                });
    worker.initialize();
    worker.loadMapping(QStringLiteral("mapping.json"));
    worker.startOnline(45454);
    worker.setRecordingInputEnabled(true);

    QSignalSpy batchSpy(&worker, &NetworkRuntimeWorker::recordingBatchReady);
    QSignalSpy drainSpy(&worker, &NetworkRuntimeWorker::recordingInputDrained);
    for (int index = 0; index < 2049; ++index)
        source->deliver(QByteArrayLiteral("accepted"));

    QCOMPARE(batchSpy.size(), 1);
    QCOMPARE(qvariant_cast<QVector<CapturedPacket>>(batchSpy.at(0).at(1)).size(), 2048);
    worker.drainRecordingInput(73);
    QCOMPARE(batchSpy.size(), 2);
    QCOMPARE(qvariant_cast<QVector<CapturedPacket>>(batchSpy.at(1).at(1)).size(), 1);
    QCOMPARE(drainSpy.size(), 0);

    const quint64 firstId = batchSpy.at(0).at(0).toULongLong();
    const quint64 secondId = batchSpy.at(1).at(0).toULongLong();
    worker.acknowledgeRecordingBatch(secondId + 100);
    worker.acknowledgeRecordingBatch(firstId);
    worker.acknowledgeRecordingBatch(firstId);
    QCOMPARE(drainSpy.size(), 0);
    worker.acknowledgeRecordingBatch(secondId);
    QCOMPARE(drainSpy.size(), 1);
    QCOMPARE(drainSpy.constFirst().at(0).toULongLong(), quint64(73));
    worker.shutdown();
}

void RecordingPipelineTests::snapshotDrainBuffersPostBoundaryPackets() {
    auto *source = new FakeDatagramSource;
    NetworkRuntimeWorker worker(std::make_unique<FakeMappingRepository>(),
                                std::make_unique<FakePacketDecoder>(),
                                [source](QObject *parent) -> IDatagramSource * {
                                    source->setParent(parent);
                                    return source;
                                });
    worker.initialize();
    worker.loadMapping(QStringLiteral("mapping.json"));
    worker.startOnline(45454);
    worker.setRecordingInputEnabled(true);

    QSignalSpy batchSpy(&worker, &NetworkRuntimeWorker::recordingBatchReady);
    QSignalSpy drainSpy(&worker, &NetworkRuntimeWorker::recordingInputDrained);
    source->deliver(QByteArrayLiteral("accepted"), 1'000'000'000);
    worker.drainRecordingInput(74, true);
    QCOMPARE(batchSpy.size(), 1);

    source->deliver(QByteArrayLiteral("accepted"), 2'000'000'000);
    source->deliver(QByteArrayLiteral("accepted"), 3'000'000'000);
    const quint64 prefixBatchId = batchSpy.constFirst().at(0).toULongLong();
    worker.acknowledgeRecordingBatch(prefixBatchId);
    QCOMPARE(drainSpy.size(), 1);

    worker.setRecordingInputEnabled(true);
    QCOMPARE(batchSpy.size(), 2);
    const QVector<CapturedPacket> postBoundary =
        qvariant_cast<QVector<CapturedPacket>>(batchSpy.at(1).at(1));
    QCOMPARE(postBoundary.size(), 2);
    QCOMPARE(postBoundary.at(0).receivedAtNanoseconds, qint64(2'000'000'000));
    QCOMPARE(postBoundary.at(1).receivedAtNanoseconds, qint64(3'000'000'000));

    worker.acknowledgeRecordingBatch(batchSpy.at(1).at(0).toULongLong());
    worker.shutdown();
}

void RecordingPipelineTests::recordingRetainsFailedTransactionAndExcludesPauseTime() {
    FakeRecordingTransaction transaction;
    FakeSessionPersistence persistence;
    FakeRecordingClock clock;
    RecordingService recording(transaction, clock);

    QString error;
    QVERIFY(recording.startRecording(QByteArrayLiteral("mapping"), &error));
    clock.now = 1'000'000'000;
    recording.recordPacket(QByteArrayLiteral("first"));
    recording.pauseRecording();

    clock.now = 31'000'000'000;
    recording.resumeRecording();
    clock.now = 31'500'000'000;
    recording.recordPacket(QByteArrayLiteral("second"));

    QCOMPARE(transaction.timestamps.size(), 2);
    QCOMPARE(transaction.timestamps.at(0).milliseconds(), qint64(1000));
    QCOMPARE(transaction.timestamps.at(1).milliseconds(), qint64(1500));
    QCOMPARE(recording.recordingDuration().milliseconds(), qint64(500));

    SessionSnapshot snapshot;
    QVERIFY(recording.createSnapshot(&snapshot, &error));
    persistence.fail = true;
    QVERIFY(!persistence.save(snapshot, QStringLiteral("failed.lar"), &error));
    QVERIFY(recording.isRecording());
    QCOMPARE(transaction.recordCount(), quint64(2));

    persistence.fail = false;
    QVERIFY(persistence.save(snapshot, QStringLiteral("saved.lar"), &error));
    recording.completeRecording();
    QVERIFY(!recording.isRecording());
}

void RecordingPipelineTests::recordingUsesTransportReceiptTime() {
    FakeRecordingTransaction transaction;
    FakeRecordingClock clock;
    clock.now = 100'000'000'000;
    RecordingService recording(transaction, clock);

    QString error;
    QVERIFY(recording.startRecording(QByteArrayLiteral("mapping"), &error));
    const QVector<CapturedPacket> delayedBatch{
        {QByteArrayLiteral("first"), 101'000'000'000},
        {QByteArrayLiteral("second"), 103'500'000'000},
    };
    clock.now = 500'000'000'000;
    recording.recordPackets(delayedBatch);

    QCOMPARE(transaction.timestamps.size(), 2);
    QCOMPARE(transaction.timestamps.at(0).milliseconds(), qint64(1000));
    QCOMPARE(transaction.timestamps.at(1).milliseconds(), qint64(3500));
    QCOMPARE(recording.recordingDuration().milliseconds(), qint64(2500));
}

void RecordingPipelineTests::recordingCoordinatorRejectsStalePersistenceCompletions() {
    FakeRecordingTransaction transaction;
    FakeRecordingClock clock;
    RecordingService recording(transaction, clock);
    RecordingPipelineCoordinator coordinator(recording);
    QSignalSpy drainSpy(&coordinator, &RecordingPipelineCoordinator::recordingDrainRequested);
    QSignalSpy persistenceSpy(&coordinator, &RecordingPipelineCoordinator::persistenceRequested);
    QSignalSpy saveSpy(&coordinator, &RecordingPipelineCoordinator::recordingSaveFinished);

    coordinator.startRecording(QByteArrayLiteral("mapping"));
    coordinator.snapshotRecording(QStringLiteral("snapshot.lar"));
    QCOMPARE(drainSpy.size(), 1);
    QVERIFY(drainSpy.constLast().at(1).toBool());
    const quint64 snapshotDrain = drainSpy.constLast().at(0).toULongLong();
    coordinator.recordingInputDrained(snapshotDrain);
    QCOMPARE(persistenceSpy.size(), 1);
    const quint64 snapshotRequest = persistenceSpy.constLast().at(0).toULongLong();

    coordinator.persistenceFinished(snapshotRequest + 1, false, true, QStringLiteral("stale.lar"),
                                    {});
    QCOMPARE(saveSpy.size(), 0);
    QVERIFY(recording.isRecording());

    coordinator.persistenceFinished(snapshotRequest, false, true, QStringLiteral("snapshot.lar"),
                                    {});
    QCOMPARE(saveSpy.size(), 1);
    QVERIFY(recording.isRecording());

    coordinator.stopRecording(QStringLiteral("final.lar"));
    QCOMPARE(drainSpy.size(), 2);
    QVERIFY(!drainSpy.constLast().at(1).toBool());
    coordinator.recordingInputDrained(drainSpy.constLast().at(0).toULongLong());
    QCOMPARE(persistenceSpy.size(), 2);
    const quint64 failedFinalRequest = persistenceSpy.constLast().at(0).toULongLong();
    coordinator.persistenceFinished(failedFinalRequest, true, false, QStringLiteral("final.lar"),
                                    QStringLiteral("simulated save failure"));
    QVERIFY(recording.isRecording());
    QVERIFY(recording.isPaused());

    coordinator.stopRecording(QStringLiteral("retry.lar"));
    QCOMPARE(drainSpy.size(), 3);
    coordinator.recordingInputDrained(drainSpy.constLast().at(0).toULongLong());
    QCOMPARE(persistenceSpy.size(), 3);
    coordinator.persistenceFinished(persistenceSpy.constLast().at(0).toULongLong(), true, true,
                                    QStringLiteral("retry.lar"), {});
    QVERIFY(!recording.isRecording());
}

void RecordingPipelineTests::overlappingSaveRequestsKeepTheirRequestIds() {
    FakeRecordingTransaction transaction;
    FakeRecordingClock clock;
    RecordingService recording(transaction, clock);
    RecordingPipelineCoordinator coordinator(recording);
    QSignalSpy drainSpy(&coordinator, &RecordingPipelineCoordinator::recordingDrainRequested);
    QSignalSpy persistenceSpy(&coordinator, &RecordingPipelineCoordinator::persistenceRequested);
    QSignalSpy saveSpy(&coordinator, &RecordingPipelineCoordinator::recordingSaveFinished);

    coordinator.startRecording(QByteArrayLiteral("mapping"), RuntimeRequestId{1});
    coordinator.snapshotRecording(QStringLiteral("first.lar"), RuntimeRequestId{10});
    QCOMPARE(drainSpy.size(), 1);
    coordinator.snapshotRecording(QStringLiteral("second.lar"), RuntimeRequestId{11});
    QCOMPARE(saveSpy.size(), 1);
    const RecordingSaveResult rejected =
        qvariant_cast<RecordingSaveResult>(saveSpy.constFirst().at(0));
    QCOMPARE(rejected.request, RuntimeRequestId{11});
    QVERIFY(!rejected.saved);
    QCOMPARE(rejected.path, QStringLiteral("second.lar"));

    coordinator.recordingInputDrained(drainSpy.constFirst().at(0).toULongLong());
    QCOMPARE(persistenceSpy.size(), 1);
    coordinator.persistenceFinished(persistenceSpy.constFirst().at(0).toULongLong(), false, true,
                                    QStringLiteral("first.lar"), {});
    QCOMPARE(saveSpy.size(), 2);
    const RecordingSaveResult completed =
        qvariant_cast<RecordingSaveResult>(saveSpy.constLast().at(0));
    QCOMPARE(completed.request, RuntimeRequestId{10});
    QVERIFY(completed.saved);
    QCOMPARE(completed.path, QStringLiteral("first.lar"));
}

void RecordingPipelineTests::pausedSnapshotDoesNotCapturePausedPackets() {
    FakeRecordingTransaction transaction;
    FakeRecordingClock clock;
    RecordingService recording(transaction, clock);
    RecordingPipelineCoordinator coordinator(recording);
    QSignalSpy drainSpy(&coordinator, &RecordingPipelineCoordinator::recordingDrainRequested);

    coordinator.startRecording(QByteArrayLiteral("mapping"));
    coordinator.pauseRecording();
    QCOMPARE(drainSpy.size(), 1);
    QVERIFY(!drainSpy.constLast().at(1).toBool());
    coordinator.recordingInputDrained(drainSpy.constLast().at(0).toULongLong(), 1'000'000'000);
    QVERIFY(recording.isPaused());

    coordinator.snapshotRecording(QStringLiteral("paused-snapshot.lar"));
    QCOMPARE(drainSpy.size(), 2);
    QVERIFY(!drainSpy.constLast().at(1).toBool());
}

QTEST_GUILESS_MAIN(RecordingPipelineTests)
#include "recording_pipeline_tests.moc"
