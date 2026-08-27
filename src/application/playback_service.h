#pragma once

/**
 * @file playback_service.h
 * @brief Clock-driven playback of indexed LAR session records.
 */

#include "application/ports/playback_clock.h"
#include "application/ports/session_reader.h"
#include "domain/decoded_state.h"

#include <QObject>
#include <QString>

/**
 * @brief Implements indexed session replay on a fixed-rate presentation clock.
 *
 * Session records remain ordered by their recorded timestamps, but playback
 * publishes at 60 presentation ticks per second. At each tick it selects the
 * newest record strictly before the target clock position; consequently fast
 * rates and dense sessions may skip source records rather than emitting a
 * burst. `recordsProcessed()` counts displayed-record changes, not clock ticks.
 *
 * Seeking is inclusive of a record whose timestamp exactly equals the target.
 * Non-repeating playback publishes the last eligible frame, moves to the exact
 * session duration, and emits `playbackFinished()`. Repeating playback wraps
 * the presentation position modulo the duration without emitting completion.
 */
class PlaybackService final : public QObject {
    Q_OBJECT

  public:
    /** Presentation sampling rate used by normal replay. */
    static constexpr int ReplayFramesPerSecond = 60;

    PlaybackService(ISessionReader &reader, IPlaybackClock &clock, QObject *parent = nullptr);

    /** Replaces any loaded session and publishes its first record, if present. */
    bool loadSession(const QString &path, QString *error = nullptr);
    /** In-memory equivalent of `loadSession()`, primarily for bounded inputs and tests. */
    bool loadData(const QByteArray &data, QString *error = nullptr);
    void closeSession();

    void play();
    /** Stops the presentation clock while preserving the current position. */
    void pause();
    /** Stops playback and returns the position and displayed frame to the start. */
    void stop();
    /** Clamps to the session duration and publishes the frame at or before the target. */
    void seek(SessionTimestamp position);
    /** Sets a finite, strictly positive multiplier for presentation-clock progress. */
    bool setRate(double rate);
    void setRepeat(bool enabled) noexcept;

    bool isLoaded() const noexcept {
        return m_reader.isValid();
    }
    bool isPlaying() const noexcept {
        return m_isPlaying;
    }
    SessionTimestamp position() const noexcept {
        return m_position;
    }
    SessionTimestamp duration() const noexcept {
        return m_reader.duration();
    }
    double rate() const noexcept {
        return m_rate;
    }
    bool repeatEnabled() const noexcept {
        return m_repeat;
    }
    qint64 recordCount() const noexcept {
        return m_reader.recordCount();
    }

  signals:
    void frameReady(const DecodedState &state);
    void recordsProcessed(quint64 count);
    void positionChanged(SessionTimestamp position);
    void playingChanged(bool isPlaying);
    void playbackFinished();
    void playbackError(const QString &error);

  private slots:
    void onClockTick();

  private:
    bool publishFrame(qint64 index, QString *error = nullptr);
    void publishDecodedFrame(qint64 index, const SessionStateItem &item);
    bool publishReplayFrame(qint64 index);
    bool findFrameBefore(long double targetMilliseconds, qint64 *index);
    bool resetToFirstFrame();
    void finishPlayback();
    void failPlayback(const QString &error);

    ISessionReader &m_reader;
    IPlaybackClock &m_clock;
    qint64 m_displayedIndex = -1;
    SessionTimestamp m_position;
    long double m_exactPositionMilliseconds = 0.0L;
    double m_rate = 1.0;
    bool m_isPlaying = false;
    bool m_repeat = false;
};
