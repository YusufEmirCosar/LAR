#include "application/session_timestamp.h"
#include "viewer/playback_timeline_mapper.h"
#include "viewer/playbacktimeformatter.h"

#include <QtTest>

#include <cmath>
#include <cstdlib>
#include <limits>

class SessionTimeTests final : public QObject {
    Q_OBJECT

  private slots:
    void constructionEnforcesClosedDomain();
    void sliderMappingIsBoundedAndRoundTrips();
    void formatterCoversFullSupportedDuration();
};

void SessionTimeTests::constructionEnforcesClosedDomain() {
    QCOMPARE(SessionTimestamp{}.milliseconds(), qint64(0));
    QCOMPARE(SessionTimestamp::fromMilliseconds(1)->milliseconds(), qint64(1));
    QCOMPARE(SessionTimestamp::maximum().milliseconds(),
             static_cast<qint64>(lar::session::MaximumDurationMilliseconds));
    QVERIFY(!SessionTimestamp::fromMilliseconds(-1));
    QVERIFY(!SessionTimestamp::fromMilliseconds(SessionTimestamp::maximum().milliseconds() + 1));
    QVERIFY(!SessionTimestamp::fromStoredMilliseconds(std::numeric_limits<quint64>::max()));
    QVERIFY(!SessionTimestamp::fromSeconds(std::numeric_limits<double>::infinity()));
    QVERIFY(!SessionTimestamp::fromSeconds(std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(SessionTimestamp::fromNanoseconds(499'999)->milliseconds(), qint64(0));
    QCOMPARE(SessionTimestamp::fromNanoseconds(500'000)->milliseconds(), qint64(1));
}

void SessionTimeTests::sliderMappingIsBoundedAndRoundTrips() {
    constexpr int SliderMaximum = 10000;
    const SessionTimestamp duration = SessionTimestamp::maximum();
    const qint64 quantum = (duration.milliseconds() + SliderMaximum - 1) / SliderMaximum;

    for (int slider = 0; slider <= SliderMaximum; slider += 37) {
        const SessionTimestamp timestamp =
            PlaybackTimelineMapper::fromSlider(duration, slider, SliderMaximum);
        QVERIFY(timestamp >= SessionTimestamp{});
        QVERIFY(timestamp <= duration);
        const int roundTrip = PlaybackTimelineMapper::toSlider(timestamp, duration, SliderMaximum);
        QVERIFY(std::abs(roundTrip - slider) <= 1);

        const SessionTimestamp reconstructed =
            PlaybackTimelineMapper::fromSlider(duration, roundTrip, SliderMaximum);
        QVERIFY(std::llabs(reconstructed.milliseconds() - timestamp.milliseconds()) <= quantum);
    }

    QCOMPARE(PlaybackTimelineMapper::toSlider(duration, duration, SliderMaximum), SliderMaximum);
    QCOMPARE(
        PlaybackTimelineMapper::fromSlider(duration, SliderMaximum, SliderMaximum).milliseconds(),
        duration.milliseconds());
}

void SessionTimeTests::formatterCoversFullSupportedDuration() {
    QCOMPARE(PlaybackTimeFormatter::format({}), QStringLiteral("00:00.000"));
    QCOMPARE(PlaybackTimeFormatter::format(SessionTimestamp::fromMilliseconds(3'723'004).value()),
             QStringLiteral("01:02:03.004"));
    QCOMPARE(PlaybackTimeFormatter::format(SessionTimestamp::maximum()),
             QStringLiteral("365d 00:00:00.000"));
}

QTEST_GUILESS_MAIN(SessionTimeTests)
#include "session_time_tests.moc"
