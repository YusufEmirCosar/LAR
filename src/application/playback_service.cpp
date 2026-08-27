
#include "application/playback_service.h"

#include <algorithm>
#include <cmath>

PlaybackService::PlaybackService(ISessionReader &reader, IPlaybackClock &clock, QObject *parent)
    : QObject(parent), m_reader(reader), m_clock(clock) {
    connect(&m_clock, &IPlaybackClock::tick, this, &PlaybackService::onClockTick);
}

bool PlaybackService::loadSession(const QString &path, QString *error) {
    closeSession();
    if (!m_reader.loadFile(path, error)) {
        return false;
    }
    m_displayedIndex = -1;
    m_position = {};
    m_exactPositionMilliseconds = 0.0L;
    if (m_reader.recordCount() > 0) {
        if (!publishFrame(0, error)) {
            m_reader.close();
            return false;
        }
    }
    emit positionChanged(m_position);
    return true;
}

bool PlaybackService::loadData(const QByteArray &data, QString *error) {
    closeSession();
    if (!m_reader.loadData(data, error)) {
        return false;
    }
    m_displayedIndex = -1;
    m_position = {};
    m_exactPositionMilliseconds = 0.0L;
    if (m_reader.recordCount() > 0) {
        if (!publishFrame(0, error)) {
            m_reader.close();
            return false;
        }
    }
    emit positionChanged(m_position);
    return true;
}

void PlaybackService::closeSession() {
    const bool wasPlaying = m_isPlaying;
    m_clock.stop();
    m_isPlaying = false;
    m_reader.close();
    m_displayedIndex = -1;
    m_position = {};
    m_exactPositionMilliseconds = 0.0L;
    if (wasPlaying)
        emit playingChanged(false);
    emit positionChanged({});
}

void PlaybackService::play() {
    if (!m_reader.isValid() || m_reader.recordCount() == 0 || m_isPlaying)
        return;
    if (m_exactPositionMilliseconds >=
            static_cast<long double>(m_reader.duration().milliseconds()) &&
        !resetToFirstFrame())
        return;
    m_isPlaying = true;
    m_clock.start(ReplayFramesPerSecond);
    emit playingChanged(true);
}

void PlaybackService::pause() {
    if (!m_isPlaying)
        return;
    m_isPlaying = false;
    m_clock.stop();
    emit playingChanged(false);
}

void PlaybackService::stop() {
    const bool wasPlaying = m_isPlaying;
    m_clock.stop();
    m_isPlaying = false;
    m_displayedIndex = -1;
    m_position = {};
    m_exactPositionMilliseconds = 0.0L;
    if (m_reader.isValid() && m_reader.recordCount() > 0) {
        QString error;
        if (!publishFrame(0, &error))
            emit playbackError(error);
    }
    if (wasPlaying)
        emit playingChanged(false);
    emit positionChanged(m_position);
}

void PlaybackService::seek(SessionTimestamp position) {
    if (!m_reader.isValid() || m_reader.recordCount() == 0)
        return;
    const SessionTimestamp targetTime = std::min(position, m_reader.duration());

    qint64 bestIndex = 0;
    // findFrameBefore() uses a strict comparison. Advancing the integer target
    // by one millisecond makes seeking inclusive without changing replay's
    // between-tick sampling rule.
    if (!findFrameBefore(static_cast<long double>(targetTime.milliseconds()) + 1.0L, &bestIndex))
        return;

    m_position = targetTime;
    m_exactPositionMilliseconds = static_cast<long double>(targetTime.milliseconds());
    QString error;
    if (!publishFrame(bestIndex, &error)) {
        failPlayback(error);
        return;
    }
    emit positionChanged(m_position);
}

bool PlaybackService::setRate(double rate) {
    if (rate <= 0.0 || !std::isfinite(rate))
        return false;
    m_rate = rate;
    return true;
}

void PlaybackService::setRepeat(bool enabled) noexcept {
    m_repeat = enabled;
}

void PlaybackService::onClockTick() {
    if (!m_isPlaying || !m_reader.isValid() || m_reader.recordCount() == 0)
        return;

    // The clock advances independently of record density. Selecting only the
    // newest eligible record prevents a dense capture or high rate from
    // flooding the UI with a catch-up burst.
    const long double frameStepMilliseconds = 1000.0L * static_cast<long double>(m_rate) /
                                              static_cast<long double>(ReplayFramesPerSecond);
    const long double durationMilliseconds =
        static_cast<long double>(m_reader.duration().milliseconds());
    long double nextPosition = m_exactPositionMilliseconds + frameStepMilliseconds;

    if (m_repeat && durationMilliseconds > 0.0L && std::isfinite(nextPosition)) {
        nextPosition = std::fmod(nextPosition, durationMilliseconds);
        qint64 bestIndex = 0;
        if (!findFrameBefore(nextPosition, &bestIndex) || !publishReplayFrame(bestIndex))
            return;
        m_exactPositionMilliseconds = nextPosition;
        m_position = SessionTimestamp::clampedMilliseconds(
            static_cast<qint64>(std::round(m_exactPositionMilliseconds)));
        emit positionChanged(m_position);
        return;
    }

    if (durationMilliseconds <= 0.0L || !std::isfinite(nextPosition) ||
        nextPosition > durationMilliseconds) {
        qint64 bestIndex = 0;
        const long double searchPosition =
            std::isfinite(nextPosition) ? nextPosition : durationMilliseconds + 1.0L;
        if (!findFrameBefore(searchPosition, &bestIndex) || !publishReplayFrame(bestIndex))
            return;
        finishPlayback();
        return;
    }

    qint64 bestIndex = 0;
    if (!findFrameBefore(nextPosition, &bestIndex) || !publishReplayFrame(bestIndex))
        return;
    m_exactPositionMilliseconds = nextPosition;
    m_position = SessionTimestamp::clampedMilliseconds(
        static_cast<qint64>(std::round(m_exactPositionMilliseconds)));
    emit positionChanged(m_position);
}

bool PlaybackService::publishFrame(qint64 index, QString *error) {
    if (!m_reader.isValid() || index < 0 || index >= m_reader.recordCount()) {
        if (error)
            *error = QStringLiteral("Playback frame index is out of range");
        return false;
    }
    if (index == m_displayedIndex)
        return true;
    SessionStateItem item;
    if (!m_reader.recordAt(index, &item, error))
        return false;
    publishDecodedFrame(index, item);
    return true;
}

void PlaybackService::publishDecodedFrame(qint64 index, const SessionStateItem &item) {
    if (!m_reader.isValid() || index < 0 || index >= m_reader.recordCount() ||
        index == m_displayedIndex) {
        return;
    }
    m_displayedIndex = index;
    emit frameReady(item.state);
}

bool PlaybackService::publishReplayFrame(qint64 index) {
    const qint64 previousIndex = m_displayedIndex;
    QString error;
    if (!publishFrame(index, &error)) {
        failPlayback(error);
        return false;
    }
    if (m_displayedIndex != previousIndex)
        emit recordsProcessed(1);
    return true;
}

bool PlaybackService::findFrameBefore(long double targetMilliseconds, qint64 *index) {
    if (index == nullptr || !m_reader.isValid() || m_reader.recordCount() <= 0 ||
        !std::isfinite(targetMilliseconds)) {
        return false;
    }

    // No record can precede zero. The historical behavior clamps this case to
    // the first record rather than selecting later records with timestamp zero.
    if (targetMilliseconds <= 0.0L) {
        *index = 0;
        return true;
    }

    // Stored timestamps are integral milliseconds. Therefore the greatest
    // timestamp strictly below x is ceil(x) - 1. Passing that inclusive bound
    // lets the reader use its two-level sparse index and materialize at most
    // one location page per lookup.
    const long double inclusiveMilliseconds =
        std::min(std::ceil(targetMilliseconds) - 1.0L,
                 static_cast<long double>(SessionTimestamp::maximum().milliseconds()));
    const SessionTimestamp position =
        SessionTimestamp::clampedMilliseconds(static_cast<qint64>(inclusiveMilliseconds));
    QString error;
    if (m_reader.findRecordAtOrBefore(position, index, &error))
        return true;
    failPlayback(error);
    return false;
}

bool PlaybackService::resetToFirstFrame() {
    m_exactPositionMilliseconds = 0.0L;
    m_position = {};
    if (m_reader.isValid() && m_reader.recordCount() > 0 && !publishReplayFrame(0))
        return false;
    emit positionChanged(m_position);
    return true;
}

void PlaybackService::finishPlayback() {
    if (!m_isPlaying)
        return;
    m_clock.stop();
    m_isPlaying = false;
    m_exactPositionMilliseconds = static_cast<long double>(m_reader.duration().milliseconds());
    m_position = m_reader.duration();
    emit positionChanged(m_position);
    emit playingChanged(false);
    emit playbackFinished();
}

void PlaybackService::failPlayback(const QString &error) {
    const bool wasPlaying = m_isPlaying;
    m_clock.stop();
    m_isPlaying = false;
    if (wasPlaying)
        emit playingChanged(false);
    emit playbackError(error.isEmpty() ? QStringLiteral("Session playback failed") : error);
}
