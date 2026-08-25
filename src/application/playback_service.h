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

/** @brief Implements indexed, fixed-frame-rate session replay. */
class PlaybackService final : public QObject {
    Q_OBJECT

  public:
    /** Presentation sampling rate used by normal replay. */
    static constexpr int ReplayFramesPerSecond = 60;

    PlaybackService(ISessionReader &reader, IPlaybackClock &clock, QObject *parent = nullptr);

    bool loadSession(const QString &path, QString *error = nullptr);
    bool loadData(const QByteArray &data, QString *error = nullptr);
    void closeSession();

    void play();
    /**
     * @brief Pauses the current operation.
     */
    void pause();
    void stop();
    void seek(SessionTimestamp position);
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
    bool readTimestamp(qint64 index, SessionTimestamp *timestamp);
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
