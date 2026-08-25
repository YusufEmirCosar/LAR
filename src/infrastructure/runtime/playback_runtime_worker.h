#pragma once

/**
 * @file playback_runtime_worker.h
 * @brief Session-thread worker that owns playback services and clocks.
 */

#include "application/ports/runtime_event_context.h"
#include "application/ports/session_reader.h"
#include "domain/decoded_state.h"

#include <QBitArray>
#include <QObject>

#include <functional>
#include <memory>

class IPlaybackClock;
class PlaybackMetrics;
class PlaybackPublicationThrottle;
class PlaybackService;

/**
 * Session-thread host for session loading and playback only.
 */
class PlaybackRuntimeWorker final : public QObject {
    Q_OBJECT

  public:
    using PlaybackClockFactory = std::function<IPlaybackClock *(QObject *parent)>;

    explicit PlaybackRuntimeWorker(std::unique_ptr<ISessionReader> reader = {},
                                   PlaybackClockFactory clockFactory = {},
                                   QObject *parent = nullptr);
    ~PlaybackRuntimeWorker() override;

  public slots:
    void initialize();
    void loadSession(const QString &path, quint64 generation = 0, RuntimeRequestId request = {});
    void closeSession(quint64 generation = 0, RuntimeRequestId request = {});
    void play(RuntimeRequestId request = {});
    void pause(RuntimeRequestId request = {});
    void stop(RuntimeRequestId request = {});
    void seek(SessionTimestamp position, RuntimeRequestId request = {});
    void setPlaybackRate(double rate, RuntimeRequestId request = {});
    void setPlaybackRepeat(bool enabled, RuntimeRequestId request = {});
    void resetMetrics(RuntimeRequestId request = {});
    /**
     * @brief Controls the shutdown operation.
     */
    void shutdown();

  signals:
    void commandFinished(const RuntimeCommandResult &result);
    void stateReady(const StateEvent &event);
    void metricsChanged(const MetricsEvent &event);
    void sessionLoadFinished(const SessionLoadResult &result);
    void sessionClosed(const SessionCloseResult &result);
    void playbackPositionChanged(const PlaybackPositionEvent &event);
    void playbackPlayingChanged(const PlaybackStateEvent &event);
    void playbackFinished(const PlaybackFinishedEvent &event);
    void runtimeError(const RuntimeFailure &failure);

  private:
    std::unique_ptr<ISessionReader> m_reader;
    PlaybackClockFactory m_clockFactory;
    IPlaybackClock *m_clock = nullptr;
    PlaybackService *m_playback = nullptr;
    PlaybackPublicationThrottle *m_publication = nullptr;
    PlaybackMetrics *m_metrics = nullptr;
    quint64 m_stateGeneration = 0;
};
