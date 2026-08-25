#pragma once

/**
 * @file source_lifecycle_coordinator.h
 * @brief Single-owner state machine for online and playback source lifecycles.
 */

#include "application/application_state.h"
#include "application/application_view_model.h"
#include "application/mode_coordinator.h"
#include "application/ports/application_runtime.h"

#include <QObject>

#include <optional>

/** Single owner of online/playback activation, switching, and publications. */
class SourceLifecycleCoordinator final : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Idle,
        StartingOnline,
        Online,
        StoppingOnlineForPlayback,
        LoadingPlayback,
        SessionLoaded,
        Playing,
        PlaybackPaused,
        ClosingPlaybackForOnline,
        ShuttingDown,
    };
    Q_ENUM(State)

    SourceLifecycleCoordinator(ModeCoordinator &modes, IApplicationRuntime &runtime,
                               ApplicationViewModel &viewModel, ApplicationState &applicationState,
                               QObject *parent = nullptr);

    bool startOnline(quint16 port);
    bool stopOnline();
    bool loadSession(const QString &path);
    bool closeSession();
    void play();
    /**
     * @brief Pauses the current operation.
     */
    void pause();
    void stop();
    void seek(SessionTimestamp position);
    void resetMetrics();
    void applyRecordingState(bool hasSession, bool paused);
    /**
     * @brief Shuts down the active source lifecycle.
     */
    void shutdown() noexcept;

    State state() const noexcept {
        return m_state;
    }
    RuntimeSourceEpoch activeEpoch() const noexcept {
        return m_activeEpoch;
    }

  signals:
    void errorRaised(const QString &message);
    void onlineStateChanged(bool listening);
    void sessionLoaded(const QString &path, qint64 recordCount);
    void processedPacketCountChanged(quint64 count);
    void processedPacketRateChanged(quint64 rate);
    void lifecycleStateChanged(SourceLifecycleCoordinator::State state);

  private:
    enum class DesiredSource { None, Online, Playback };
    enum class DispatchKind { None, StartOnline, StopOnline, LoadPlayback, ClosePlayback };

    void setState(State state);
    bool transitionTo(ApplicationMode mode);
    void transitionBoundary();
    bool beginStartOnline();
    bool beginStopOnline();
    bool beginLoadPlayback();
    bool beginClosePlayback();
    void advanceIntent();
    void rejectDispatch(const CommandDispatch &dispatch);
    void reportRejectedCommand(const CommandDispatch &dispatch);

    void receive(const OnlineStartResult &result);
    void receive(const OnlineStopResult &result);
    void receive(const SessionLoadResult &result);
    void receive(const SessionCloseResult &result);
    void handle(const OnlineStartResult &result);
    void handle(const OnlineStopResult &result);
    void handle(const SessionLoadResult &result);
    void handle(const SessionCloseResult &result);
    void accept(const OnlineStateEvent &event);
    void accept(const StateEvent &event);
    void accept(const MetricsEvent &event);
    void accept(const PlaybackPositionEvent &event);
    void accept(const PlaybackStateEvent &event);
    void accept(const PlaybackFinishedEvent &event);
    bool accepts(const RuntimeSourceEpoch &epoch) const noexcept;
    void replayDeferredPublications();
    void clearDeferredPublications();

    ModeCoordinator &m_modes;
    IApplicationRuntime &m_runtime;
    ApplicationViewModel &m_viewModel;
    ApplicationState &m_applicationState;
    State m_state = State::Idle;
    DesiredSource m_desiredSource = DesiredSource::None;
    DispatchKind m_dispatching = DispatchKind::None;
    quint16 m_desiredPort = 0;
    quint16 m_pendingStartPort = 0;
    QString m_desiredSessionPath;
    QString m_pendingLoadPath;
    RuntimeSourceEpoch m_activeEpoch;
    RuntimeRequestId m_pendingStart;
    RuntimeRequestId m_pendingStop;
    RuntimeRequestId m_pendingLoad;
    RuntimeRequestId m_pendingClose;

    std::optional<OnlineStartResult> m_synchronousStart;
    std::optional<OnlineStopResult> m_synchronousStop;
    std::optional<SessionLoadResult> m_synchronousLoad;
    std::optional<SessionCloseResult> m_synchronousClose;
    std::optional<OnlineStateEvent> m_deferredOnlineState;
    std::optional<StateEvent> m_deferredState;
    std::optional<MetricsEvent> m_deferredMetrics;
    std::optional<PlaybackPositionEvent> m_deferredPosition;
    std::optional<PlaybackStateEvent> m_deferredPlaybackState;
    std::optional<PlaybackFinishedEvent> m_deferredFinished;
    bool m_transitionBoundaryActive = false;
};
