#include "application/source_lifecycle_coordinator.h"
#include "tests/support/controlled_application_runtime.h"

#include <QtTest>

class SourceLifecycleTests final : public QObject {
    Q_OBJECT

  private slots:
    void latestIntentWaitsForAcknowledgementsAndFiltersEpochs();
    void failedActivationSettlesWithoutRetry();
};

void SourceLifecycleTests::latestIntentWaitsForAcknowledgementsAndFiltersEpochs() {
    ControlledApplicationRuntime runtime;
    ModeCoordinator modes;
    ApplicationViewModel viewModel;
    ApplicationState applicationState;
    SourceLifecycleCoordinator sources(modes, runtime, viewModel, applicationState);

    QVERIFY(sources.startOnline(41001));
    QCOMPARE(runtime.starts.size(), 1);
    QCOMPARE(runtime.stops.size(), 0);
    QCOMPARE(sources.state(), SourceLifecycleCoordinator::State::StartingOnline);

    QVERIFY(sources.loadSession(QStringLiteral("session-a.lar")));
    QCOMPARE(runtime.stops.size(), 0);
    QCOMPARE(runtime.loads.size(), 0);
    const RuntimeSourceEpoch oldOnline = runtime.starts.at(0).epoch;

    runtime.completeStart(0, true);
    QCOMPARE(runtime.stops.size(), 1);
    QCOMPARE(runtime.loads.size(), 0);
    QCOMPARE(sources.state(), SourceLifecycleCoordinator::State::StoppingOnlineForPlayback);
    runtime.publishMetrics(oldOnline, 99, 10);
    QCOMPARE(viewModel.processedPacketCount(), quint64(0));

    runtime.completeStop(0, true);
    QCOMPARE(runtime.loads.size(), 1);
    QCOMPARE(runtime.loads.at(0).path, QStringLiteral("session-a.lar"));

    emit runtime.sessionLoadFinished({{999999},
                                      {RuntimeStateSource::Playback, 999999},
                                      true,
                                      QStringLiteral("stale.lar"),
                                      99,
                                      SessionTimestamp::maximum(),
                                      {}});
    QVERIFY(!applicationState.sessionLoaded);

    runtime.completeLoad(0, true, 3, SessionTimestamp::fromMilliseconds(1500).value());
    QVERIFY(applicationState.sessionLoaded);
    QCOMPARE(applicationState.loadedRecordCount, 3);
    QCOMPARE(modes.mode(), ApplicationMode::SessionLoaded);
    const RuntimeSourceEpoch firstPlayback = runtime.loads.at(0).epoch;
    runtime.publishMetrics(firstPlayback, 7, 2);
    QCOMPARE(viewModel.processedPacketCount(), quint64(7));
    runtime.publishMetrics(oldOnline, 100, 100);
    QCOMPARE(viewModel.processedPacketCount(), quint64(7));

    QVERIFY(sources.startOnline(42001));
    QCOMPARE(runtime.closes.size(), 1);
    QVERIFY(sources.loadSession(QStringLiteral("session-b.lar")));
    QCOMPARE(runtime.closes.size(), 1);
    runtime.completeClose(0, true);
    QCOMPARE(runtime.starts.size(), 1);
    QCOMPARE(runtime.loads.size(), 2);
    QCOMPARE(runtime.loads.at(1).path, QStringLiteral("session-b.lar"));
    runtime.completeLoad(1, true, 1, SessionTimestamp::fromMilliseconds(250).value());
    QVERIFY(applicationState.sessionLoaded);

    QVERIFY(sources.startOnline(43001));
    QCOMPARE(runtime.closes.size(), 2);
    QVERIFY(sources.startOnline(44001));
    QCOMPARE(runtime.closes.size(), 2);
    runtime.completeClose(1, true);
    QCOMPARE(runtime.starts.size(), 2);
    QCOMPARE(runtime.starts.at(1).port, quint16(44001));
    runtime.completeStart(1, true);
    QVERIFY(applicationState.listening);
    QCOMPARE(applicationState.listeningPort, quint16(44001));
    QCOMPARE(modes.mode(), ApplicationMode::Online);

    runtime.publishMetrics(firstPlayback, 200, 200);
    QCOMPARE(viewModel.processedPacketCount(), quint64(0));
}

void SourceLifecycleTests::failedActivationSettlesWithoutRetry() {
    ControlledApplicationRuntime runtime;
    ModeCoordinator modes;
    ApplicationViewModel viewModel;
    ApplicationState applicationState;
    SourceLifecycleCoordinator sources(modes, runtime, viewModel, applicationState);
    QSignalSpy errors(&sources, &SourceLifecycleCoordinator::errorRaised);

    QVERIFY(sources.startOnline(41002));
    runtime.completeStart(0, false, QStringLiteral("simulated bind failure"));
    QCOMPARE(runtime.starts.size(), 1);
    QCOMPARE(sources.state(), SourceLifecycleCoordinator::State::Idle);
    QVERIFY(!applicationState.listening);
    QCOMPARE(errors.size(), 1);

    QVERIFY(sources.loadSession(QStringLiteral("broken.lar")));
    runtime.completeLoad(0, false, 0, {}, QStringLiteral("simulated read failure"));
    QCOMPARE(runtime.loads.size(), 1);
    QCOMPARE(sources.state(), SourceLifecycleCoordinator::State::Idle);
    QVERIFY(!applicationState.sessionLoaded);
    QCOMPARE(errors.size(), 2);
}

QTEST_GUILESS_MAIN(SourceLifecycleTests)
#include "source_lifecycle_tests.moc"
