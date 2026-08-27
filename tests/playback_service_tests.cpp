#include "tests/support/architecture_test_support.h"

class PlaybackServiceTests final : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void replayAdvancesByFrameStepAndUsesBoundedReaderLookup();
    void seekIsInclusiveWhileReplaySelectionRemainsStrict();
    void repeatWrapsNextTimestampBeforeLookup();
    void playbackStopsOnExplicitReaderFailure();
    void playbackRejectsNonFiniteSeekAtEveryBoundary();
};

void PlaybackServiceTests::initTestCase() {
    qRegisterMetaType<SessionSnapshot>("SessionSnapshot");
    qRegisterMetaType<QVector<CapturedPacket>>("QVector<CapturedPacket>");
    qRegisterMetaType<RuntimeStateSource>("RuntimeStateSource");
    qRegisterMetaType<IpAccessPolicy>("IpAccessPolicy");
}

void PlaybackServiceTests::replayAdvancesByFrameStepAndUsesBoundedReaderLookup() {
    constexpr int RecordCount = 1024;
    FakeSessionReader reader;
    reader.items.resize(RecordCount);
    for (int index = 0; index < RecordCount; ++index)
        reader.items[index] = stateItem(double(index) / 1000.0, double(index));
    FakePlaybackClock clock;
    PlaybackService playback(reader, clock);
    QSignalSpy frameSpy(&playback, &PlaybackService::frameReady);
    QSignalSpy finishedSpy(&playback, &PlaybackService::playbackFinished);

    QString error;
    QVERIFY(playback.loadData(QByteArrayLiteral("session"), &error));
    QCOMPARE(frameSpy.size(), 1);
    QVERIFY(playback.setRate(30.0));

    const quint64 lookupsBeforeFrame = reader.findRecordCalls;
    const quint64 timestampReadsBeforeFrame = reader.timestampAtCalls;
    const quint64 recordReadsBeforeFrame = reader.recordAtCalls;
    playback.play();
    QCOMPARE(clock.framesPerSecond, PlaybackService::ReplayFramesPerSecond);
    clock.advanceFrames();

    QCOMPARE(playback.position().milliseconds(), qint64(500));
    QCOMPARE(frameSpy.size(), 2);
    QCOMPARE(qvariant_cast<DecodedState>(frameSpy.constLast().at(0)).target.time, 499.0);
    QCOMPARE(reader.recordAtCalls - recordReadsBeforeFrame, quint64(1));
    QCOMPARE(reader.findRecordCalls - lookupsBeforeFrame, quint64(1));
    QCOMPARE(reader.timestampAtCalls - timestampReadsBeforeFrame, quint64(0));
    QCOMPARE(finishedSpy.size(), 0);

    clock.advanceFrames();
    QCOMPARE(playback.position().milliseconds(), qint64(1000));
    QCOMPARE(qvariant_cast<DecodedState>(frameSpy.constLast().at(0)).target.time, 999.0);
    QCOMPARE(finishedSpy.size(), 0);

    clock.advanceFrames();
    QCOMPARE(qvariant_cast<DecodedState>(frameSpy.constLast().at(0)).target.time, 1023.0);
    QCOMPARE(playback.position().milliseconds(), qint64(1023));
    QCOMPARE(finishedSpy.size(), 1);
    QVERIFY(!playback.isPlaying());
    QVERIFY(!clock.isActive());
}

void PlaybackServiceTests::seekIsInclusiveWhileReplaySelectionRemainsStrict() {
    FakeSessionReader reader;
    reader.items = {stateItem(0.0, 10.0), stateItem(0.5, 20.0), stateItem(0.5, 30.0),
                    stateItem(1.0, 40.0)};
    FakePlaybackClock clock;
    PlaybackService playback(reader, clock);
    QSignalSpy frameSpy(&playback, &PlaybackService::frameReady);

    QString error;
    QVERIFY(playback.loadData(QByteArrayLiteral("session"), &error));
    playback.seek(sessionTimestamp(0.5));
    QCOMPARE(playback.position().milliseconds(), qint64(500));
    QCOMPARE(qvariant_cast<DecodedState>(frameSpy.constLast().at(0)).target.time, 30.0);

    playback.stop();
    QVERIFY(playback.setRate(30.0));
    playback.play();
    clock.advanceFrames();
    QCOMPARE(playback.position().milliseconds(), qint64(500));
    QCOMPARE(qvariant_cast<DecodedState>(frameSpy.constLast().at(0)).target.time, 10.0);
}

void PlaybackServiceTests::repeatWrapsNextTimestampBeforeLookup() {
    FakeSessionReader reader;
    reader.items = {stateItem(0.0, 10.0), stateItem(0.020, 20.0), stateItem(0.055, 30.0)};
    FakePlaybackClock clock;
    PlaybackService playback(reader, clock);
    QSignalSpy frameSpy(&playback, &PlaybackService::frameReady);
    QSignalSpy finishedSpy(&playback, &PlaybackService::playbackFinished);

    QString error;
    QVERIFY(playback.loadData(QByteArrayLiteral("session"), &error));
    playback.setRepeat(true);
    QVERIFY(playback.repeatEnabled());
    playback.play();

    clock.advanceFrames(2);
    QCOMPARE(qvariant_cast<DecodedState>(frameSpy.constLast().at(0)).target.time, 20.0);
    clock.advanceFrames(2);

    QCOMPARE(qvariant_cast<DecodedState>(frameSpy.constLast().at(0)).target.time, 10.0);
    QCOMPARE(playback.position().milliseconds(), qint64(12));
    QCOMPARE(finishedSpy.size(), 0);
    QVERIFY(playback.isPlaying());
    QVERIFY(clock.isActive());

    QVERIFY(playback.setRate(10.5));
    clock.advanceFrames();
    QCOMPARE(qvariant_cast<DecodedState>(frameSpy.constLast().at(0)).target.time, 20.0);
    QCOMPARE(playback.position().milliseconds(), qint64(22));
    QCOMPARE(finishedSpy.size(), 0);
    QVERIFY(playback.isPlaying());

    playback.setRepeat(false);
    clock.advanceFrames();
    QCOMPARE(finishedSpy.size(), 1);
    QVERIFY(!playback.isPlaying());
}

void PlaybackServiceTests::playbackStopsOnExplicitReaderFailure() {
    FakeSessionReader reader;
    reader.items = {stateItem(0.0, 10.0), stateItem(0.5, 20.0), stateItem(2.0, 30.0)};
    FakePlaybackClock clock;
    PlaybackService playback(reader, clock);
    QSignalSpy errorSpy(&playback, &PlaybackService::playbackError);
    QSignalSpy finishedSpy(&playback, &PlaybackService::playbackFinished);

    QString error;
    QVERIFY(playback.loadData(QByteArrayLiteral("session"), &error));
    reader.failingRecordIndex = 1;
    QVERIFY(playback.setRate(60.0));
    playback.play();
    clock.advanceFrames();

    QCOMPARE(errorSpy.size(), 1);
    QVERIFY(errorSpy.constFirst().at(0).toString().contains(
        QStringLiteral("simulated record read failure")));
    QCOMPARE(finishedSpy.size(), 0);
    QVERIFY(!playback.isPlaying());
}

void PlaybackServiceTests::playbackRejectsNonFiniteSeekAtEveryBoundary() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double positiveInfinity = std::numeric_limits<double>::infinity();
    const double negativeInfinity = -positiveInfinity;

    FakeSessionReader reader;
    reader.items = {stateItem(0.0, 10.0), stateItem(1.0, 20.0)};
    FakePlaybackClock clock;
    PlaybackService playback(reader, clock);
    QSignalSpy serviceErrors(&playback, &PlaybackService::playbackError);
    QString error;
    QVERIFY(playback.loadData(QByteArrayLiteral("session"), &error));
    QVERIFY(!SessionTimestamp::fromSeconds(nan));
    QVERIFY(!SessionTimestamp::fromSeconds(positiveInfinity));
    QVERIFY(!SessionTimestamp::fromSeconds(negativeInfinity));
    QCOMPARE(serviceErrors.size(), 0);
    QCOMPARE(playback.position().milliseconds(), qint64(0));

    FacadeTestContext context;
    QSignalSpy facadeErrors(&context.facade, &ApplicationFacade::errorRaised);
    context.facade.seek(nan);
    context.facade.seek(positiveInfinity);
    context.facade.seek(negativeInfinity);
    QCOMPARE(facadeErrors.size(), 3);
}

QTEST_GUILESS_MAIN(PlaybackServiceTests)
#include "playback_service_tests.moc"
