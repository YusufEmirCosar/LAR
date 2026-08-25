#include "application/ports/runtime_messages.h"
#include "infrastructure/mapping/json_mapping_repository.h"
#include "infrastructure/runtime/threaded_application_runtime.h"
#include "infrastructure/session/lar_session_reader.h"
#include "testsender/scenarios.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QtTest>

#include <algorithm>
#include <optional>
#include <sys/resource.h>

namespace {

constexpr int PacketIntervalMilliseconds = 10;
constexpr int FinalVerificationPacketCount = 100;
constexpr qint64 MaximumSoakSeconds = 1800;
constexpr qint64 MaximumResidentGrowthBytes = 256LL * 1024LL * 1024LL;

qint64 maximumResidentBytes() noexcept {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        return 0;
#if defined(Q_OS_MACOS)
    return static_cast<qint64>(usage.ru_maxrss);
#else
    return static_cast<qint64>(usage.ru_maxrss) * 1024;
#endif
}

template <typename Result>
bool waitForResult(QSignalSpy &spy, RuntimeRequestId request, Result *result,
                   int timeoutMilliseconds = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMilliseconds) {
        for (const QList<QVariant> &arguments : spy) {
            const Result candidate = qvariant_cast<Result>(arguments.at(0));
            if (candidate.request == request) {
                if (result)
                    *result = candidate;
                return true;
            }
        }
        const int remaining = std::max(1, timeoutMilliseconds - static_cast<int>(timer.elapsed()));
        spy.wait(std::min(remaining, 100));
    }
    return false;
}

bool hasRecordingState(const QSignalSpy &spy, bool hasSession, bool paused) {
    for (const QList<QVariant> &arguments : spy) {
        const RecordingStateEvent state = qvariant_cast<RecordingStateEvent>(arguments.at(0));
        if (state.hasSession == hasSession && state.paused == paused)
            return true;
    }
    return false;
}

} // namespace

class OperationalSoakTests final : public QObject {
    Q_OBJECT

  private slots:
    void loopbackRecordingLifecycleRemainsBoundedAndReplayable();
};

void OperationalSoakTests::loopbackRecordingLifecycleRemainsBoundedAndReplayable() {
    bool durationConfigured = false;
    const int configuredSeconds =
        qEnvironmentVariableIntValue("LAR_SOAK_SECONDS", &durationConfigured);
    const qint64 durationSeconds =
        durationConfigured ? std::clamp<qint64>(configuredSeconds, 1, MaximumSoakSeconds) : 1;

    const QString mappingPath =
        QStringLiteral(LAR_TEST_SOURCE_DIR) + QStringLiteral("/maps/full-state.json");
    PacketMapping mapping;
    QString error;
    JsonMappingRepository mappingRepository;
    QVERIFY2(mappingRepository.loadFile(mappingPath, &mapping, &error), qPrintable(error));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QUdpSocket portProbe;
    if (!portProbe.bind(QHostAddress::LocalHost, 0)) {
        QSKIP(qPrintable(
            QStringLiteral("Loopback UDP is unavailable: %1").arg(portProbe.errorString())));
    }
    const quint16 port = portProbe.localPort();
    QVERIFY(port != 0);
    portProbe.close();

    const qint64 initialResidentBytes = maximumResidentBytes();
    ThreadedApplicationRuntime runtime;
    QSignalSpy mappingSpy(&runtime, &IApplicationRuntime::mappingLoadFinished);
    QSignalSpy startSpy(&runtime, &IApplicationRuntime::onlineStartFinished);
    QSignalSpy stopSpy(&runtime, &IApplicationRuntime::onlineStopFinished);
    QSignalSpy recordingSpy(&runtime, &IApplicationRuntime::recordingStateChanged);
    QSignalSpy saveSpy(&runtime, &IApplicationRuntime::recordingSaveFinished);
    QSignalSpy resetSpy(&runtime, &IApplicationRuntime::recordingResetFinished);
    QSignalSpy stateSpy(&runtime, &IApplicationRuntime::stateReady);
    QSignalSpy failureSpy(&runtime, &IApplicationRuntime::runtimeError);

    const CommandDispatch mappingCommand = runtime.loadMapping(mappingPath);
    QVERIFY(mappingCommand.accepted);
    MappingLoadResult mappingResult;
    QVERIFY(waitForResult(mappingSpy, mappingCommand.request, &mappingResult));
    QVERIFY2(mappingResult.loaded, qPrintable(mappingResult.error));

    const CommandDispatch startCommand = runtime.startOnline(port);
    QVERIFY(startCommand.accepted);
    OnlineStartResult startResult;
    QVERIFY(waitForResult(startSpy, startCommand.request, &startResult));
    if (!startResult.started) {
        QSKIP(qPrintable(QStringLiteral("Loopback UDP is unavailable: %1").arg(startResult.error)));
    }

    const CommandDispatch recordingCommand = runtime.startRecording(mapping.json());
    QVERIFY(recordingCommand.accepted);
    QTRY_VERIFY_WITH_TIMEOUT(hasRecordingState(recordingSpy, true, false), 5000);

    Plane plane{};
    Target target{};
    TestSenderScenarios::initialize(QStringLiteral("hundred-hz"), &plane, &target);
    QUdpSocket sender;
    int packetIndex = 0;
    const auto sendPacket = [&] {
        TestSenderScenarios::update(QStringLiteral("hundred-hz"), packetIndex,
                                    static_cast<double>(packetIndex) / 100.0, &plane, &target);
        const QByteArray datagram = mapping.encode(plane, target);
        const qint64 written = sender.writeDatagram(datagram, QHostAddress::LocalHost, port);
        ++packetIndex;
        return written == datagram.size();
    };

    QElapsedTimer soakTimer;
    soakTimer.start();
    qint64 nextSnapshotMilliseconds = 60000;
    qint64 nextPauseMilliseconds = 300000;
    qint64 nextResetMilliseconds = 600000;
    int snapshotNumber = 0;
    while (soakTimer.elapsed() < durationSeconds * 1000) {
        QVERIFY2(sendPacket(), qPrintable(sender.errorString()));
        QTest::qWait(PacketIntervalMilliseconds);

        if (soakTimer.elapsed() >= nextSnapshotMilliseconds) {
            const QString path =
                directory.filePath(QStringLiteral("periodic-%1.lar").arg(snapshotNumber++));
            const CommandDispatch command = runtime.snapshotRecording(path);
            QVERIFY(command.accepted);
            RecordingSaveResult result;
            QVERIFY(waitForResult(saveSpy, command.request, &result));
            QVERIFY2(result.saved, qPrintable(result.error));
            nextSnapshotMilliseconds += 60000;
        }
        if (soakTimer.elapsed() >= nextPauseMilliseconds) {
            recordingSpy.clear();
            QVERIFY(runtime.pauseRecording().accepted);
            QTRY_VERIFY_WITH_TIMEOUT(hasRecordingState(recordingSpy, true, true), 5000);
            for (int ignored = 0; ignored < 5; ++ignored) {
                QVERIFY2(sendPacket(), qPrintable(sender.errorString()));
                QTest::qWait(PacketIntervalMilliseconds);
            }
            recordingSpy.clear();
            QVERIFY(runtime.resumeRecording().accepted);
            QTRY_VERIFY_WITH_TIMEOUT(hasRecordingState(recordingSpy, true, false), 5000);
            nextPauseMilliseconds += 300000;
        }
        if (soakTimer.elapsed() >= nextResetMilliseconds) {
            const CommandDispatch command = runtime.resetRecording();
            QVERIFY(command.accepted);
            RecordingResetResult result;
            QVERIFY(waitForResult(resetSpy, command.request, &result));
            QVERIFY2(result.reset, qPrintable(result.error));
            nextResetMilliseconds += 600000;
        }
    }

    const QString snapshotPath = directory.filePath(QStringLiteral("soak-snapshot.lar"));
    const CommandDispatch snapshotCommand = runtime.snapshotRecording(snapshotPath);
    QVERIFY(snapshotCommand.accepted);
    RecordingSaveResult snapshotResult;
    QVERIFY(waitForResult(saveSpy, snapshotCommand.request, &snapshotResult));
    QVERIFY2(snapshotResult.saved, qPrintable(snapshotResult.error));
    LarSessionReader snapshotReader;
    QVERIFY2(snapshotReader.loadFile(snapshotPath, &error), qPrintable(error));
    QVERIFY(snapshotReader.recordCount() > 0);

    recordingSpy.clear();
    QVERIFY(runtime.pauseRecording().accepted);
    QTRY_VERIFY_WITH_TIMEOUT(hasRecordingState(recordingSpy, true, true), 5000);
    for (int ignored = 0; ignored < 5; ++ignored) {
        QVERIFY2(sendPacket(), qPrintable(sender.errorString()));
        QTest::qWait(PacketIntervalMilliseconds);
    }
    recordingSpy.clear();
    QVERIFY(runtime.resumeRecording().accepted);
    QTRY_VERIFY_WITH_TIMEOUT(hasRecordingState(recordingSpy, true, false), 5000);

    const CommandDispatch resetCommand = runtime.resetRecording();
    QVERIFY(resetCommand.accepted);
    RecordingResetResult resetResult;
    QVERIFY(waitForResult(resetSpy, resetCommand.request, &resetResult));
    QVERIFY2(resetResult.reset, qPrintable(resetResult.error));

    QVector<double> expectedTargetTimes;
    expectedTargetTimes.reserve(FinalVerificationPacketCount);
    stateSpy.clear();
    for (int sent = 0; sent < FinalVerificationPacketCount; ++sent) {
        QVERIFY2(sendPacket(), qPrintable(sender.errorString()));
        expectedTargetTimes.append(target.time);
        QTest::qWait(PacketIntervalMilliseconds);
    }
    const double finalTargetTime = expectedTargetTimes.constLast();
    QTRY_VERIFY_WITH_TIMEOUT(
        !stateSpy.isEmpty() &&
            qvariant_cast<StateEvent>(stateSpy.constLast().at(0)).state.target.time >=
                finalTargetTime,
        5000);

    const CommandDispatch stopCommand = runtime.stopOnline();
    QVERIFY(stopCommand.accepted);
    OnlineStopResult stopResult;
    QVERIFY(waitForResult(stopSpy, stopCommand.request, &stopResult));
    QVERIFY2(stopResult.stopped, qPrintable(stopResult.error));

    const QString finalPath = directory.filePath(QStringLiteral("soak-final.lar"));
    const CommandDispatch finalSaveCommand = runtime.stopRecording(finalPath);
    QVERIFY(finalSaveCommand.accepted);
    RecordingSaveResult finalSaveResult;
    QVERIFY(waitForResult(saveSpy, finalSaveCommand.request, &finalSaveResult));
    QVERIFY2(finalSaveResult.saved, qPrintable(finalSaveResult.error));
    QVERIFY(finalSaveResult.finalSave);

    LarSessionReader finalReader;
    QVERIFY2(finalReader.loadFile(finalPath, &error), qPrintable(error));
    QCOMPARE(finalReader.recordCount(), FinalVerificationPacketCount);
    SessionTimestamp previousTimestamp;
    for (qint64 index = 0; index < finalReader.recordCount(); ++index) {
        SessionStateItem item;
        QVERIFY2(finalReader.recordAt(index, &item, &error), qPrintable(error));
        QCOMPARE(item.state.target.time, expectedTargetTimes.at(index));
        QVERIFY(item.timestamp >= previousTimestamp);
        previousTimestamp = item.timestamp;
    }

    QVERIFY(QFileInfo(finalPath).size() <=
            qint64(mapping.json().size()) + qint64(FinalVerificationPacketCount) *
                                                (qint64(mapping.minimumPacketSize()) + qint64(64)));
    runtime.shutdown();
    QCOMPARE(failureSpy.size(), 0);

    const qint64 finalResidentBytes = maximumResidentBytes();
    if (initialResidentBytes > 0 && finalResidentBytes > initialResidentBytes) {
        QVERIFY2(finalResidentBytes - initialResidentBytes <= MaximumResidentGrowthBytes,
                 qPrintable(QStringLiteral("Resident-memory growth exceeded %1 bytes")
                                .arg(MaximumResidentGrowthBytes)));
    }
}

QTEST_GUILESS_MAIN(OperationalSoakTests)
#include "operational_soak_tests.moc"
