
#include "application/source_lifecycle_coordinator.h"

SourceLifecycleCoordinator::SourceLifecycleCoordinator(ModeCoordinator &modes,
                                                       IApplicationRuntime &runtime,
                                                       ApplicationViewModel &viewModel,
                                                       ApplicationState &applicationState,
                                                       QObject *parent)
    : QObject(parent), m_modes(modes), m_runtime(runtime), m_viewModel(viewModel),
      m_applicationState(applicationState) {
    connect(&m_runtime, &IApplicationRuntime::onlineStartFinished, this,
            [this](const OnlineStartResult &result) { receive(result); });
    connect(&m_runtime, &IApplicationRuntime::onlineStopFinished, this,
            [this](const OnlineStopResult &result) { receive(result); });
    connect(&m_runtime, &IApplicationRuntime::sessionLoadFinished, this,
            [this](const SessionLoadResult &result) { receive(result); });
    connect(&m_runtime, &IApplicationRuntime::sessionClosed, this,
            [this](const SessionCloseResult &result) { receive(result); });
    connect(&m_runtime, &IApplicationRuntime::onlineStateChanged, this,
            [this](const OnlineStateEvent &event) { accept(event); });
    connect(&m_runtime, &IApplicationRuntime::stateReady, this,
            [this](const StateEvent &event) { accept(event); });
    connect(&m_runtime, &IApplicationRuntime::metricsChanged, this,
            [this](const MetricsEvent &event) { accept(event); });
    connect(&m_runtime, &IApplicationRuntime::playbackPositionChanged, this,
            [this](const PlaybackPositionEvent &event) { accept(event); });
    connect(&m_runtime, &IApplicationRuntime::playbackPlayingChanged, this,
            [this](const PlaybackStateEvent &event) { accept(event); });
    connect(&m_runtime, &IApplicationRuntime::playbackFinished, this,
            [this](const PlaybackFinishedEvent &event) { accept(event); });
    connect(&m_runtime, &IApplicationRuntime::runtimeError, this,
            [this](const RuntimeFailure &failure) {
                if (failure.epoch && !accepts(*failure.epoch))
                    return;
                if (failure.epoch)
                    emit errorRaised(failure.message);
            });
}

void SourceLifecycleCoordinator::setState(State state) {
    if (m_state == state)
        return;
    m_state = state;
    emit lifecycleStateChanged(state);
}

bool SourceLifecycleCoordinator::transitionTo(ApplicationMode mode) {
    QString error;
    if (m_modes.transitionTo(mode, &error))
        return true;
    emit errorRaised(error);
    return false;
}

void SourceLifecycleCoordinator::transitionBoundary() {
    if (m_transitionBoundaryActive)
        return;
    m_transitionBoundaryActive = true;

    // Invalidate identity before clearing presentation state. Any already-queued
    // publication from the departing source will then fail accepts(), even if it
    // arrives while the replacement source is being constructed.
    m_activeEpoch = {};
    clearDeferredPublications();
    const bool wasListening = m_applicationState.listening;
    m_applicationState.listening = false;
    m_applicationState.listeningPort = 0;
    m_applicationState.sessionLoaded = false;
    m_applicationState.loadedRecordCount = 0;
    m_viewModel.clearState();
    m_viewModel.setProcessedPacketCount(0);
    m_viewModel.setProcessedPacketRate(0);
    m_viewModel.setPlaybackPosition({});
    m_viewModel.setPlaybackDuration({});
    emit processedPacketCountChanged(0);
    emit processedPacketRateChanged(0);
    if (wasListening)
        emit onlineStateChanged(false);
    if (!m_applicationState.hasRecordingSession && m_modes.mode() != ApplicationMode::Idle) {
        transitionTo(ApplicationMode::Idle);
    }
}

bool SourceLifecycleCoordinator::startOnline(quint16 port) {
    if (m_state == State::ShuttingDown || port == 0)
        return false;
    if (m_state == State::Online && m_applicationState.listening &&
        m_applicationState.listeningPort == port) {
        return true;
    }
    m_desiredSource = DesiredSource::Online;
    m_desiredPort = port;
    transitionBoundary();
    if (m_state == State::StartingOnline || m_state == State::StoppingOnlineForPlayback ||
        m_state == State::LoadingPlayback || m_state == State::ClosingPlaybackForOnline) {
        return true;
    }
    if (m_state == State::Online)
        return beginStopOnline();
    if (m_state == State::SessionLoaded || m_state == State::Playing ||
        m_state == State::PlaybackPaused) {
        return beginClosePlayback();
    }
    return beginStartOnline();
}

bool SourceLifecycleCoordinator::stopOnline() {
    if (m_state == State::ShuttingDown)
        return false;
    if (m_state == State::LoadingPlayback || m_state == State::SessionLoaded ||
        m_state == State::Playing || m_state == State::PlaybackPaused) {
        return true;
    }
    m_desiredSource = DesiredSource::None;
    transitionBoundary();
    if (m_state == State::StartingOnline || m_state == State::StoppingOnlineForPlayback ||
        m_state == State::ClosingPlaybackForOnline)
        return true;
    if (m_state == State::Online)
        return beginStopOnline();
    setState(State::Idle);
    m_transitionBoundaryActive = false;
    transitionTo(ApplicationMode::Idle);
    return true;
}

bool SourceLifecycleCoordinator::loadSession(const QString &path) {
    if (m_state == State::ShuttingDown || path.isEmpty())
        return false;
    m_desiredSource = DesiredSource::Playback;
    m_desiredSessionPath = path;
    transitionBoundary();
    if (m_state == State::StartingOnline || m_state == State::StoppingOnlineForPlayback ||
        m_state == State::LoadingPlayback || m_state == State::ClosingPlaybackForOnline) {
        return true;
    }
    if (m_state == State::Online)
        return beginStopOnline();
    if (m_state == State::SessionLoaded || m_state == State::Playing ||
        m_state == State::PlaybackPaused) {
        return beginClosePlayback();
    }
    return beginLoadPlayback();
}

bool SourceLifecycleCoordinator::closeSession() {
    if (m_state == State::ShuttingDown)
        return false;
    if (m_state == State::StartingOnline || m_state == State::Online) {
        return true;
    }
    m_desiredSource = DesiredSource::None;
    transitionBoundary();
    if (m_state == State::LoadingPlayback || m_state == State::ClosingPlaybackForOnline ||
        m_state == State::StoppingOnlineForPlayback)
        return true;
    if (m_state == State::SessionLoaded || m_state == State::Playing ||
        m_state == State::PlaybackPaused) {
        return beginClosePlayback();
    }
    setState(State::Idle);
    m_transitionBoundaryActive = false;
    transitionTo(ApplicationMode::Idle);
    return true;
}

bool SourceLifecycleCoordinator::beginStartOnline() {
    setState(State::StartingOnline);
    m_synchronousStart.reset();

    // Direct runtimes may emit their completion from inside startOnline(). Hold
    // that re-entrant result until the dispatch returns and its request ID can be
    // installed; threaded runtimes naturally take the normal pending-ID path.
    m_dispatching = DispatchKind::StartOnline;
    const CommandDispatch dispatch = m_runtime.startOnline(m_desiredPort);
    m_dispatching = DispatchKind::None;
    if (!dispatch.accepted) {
        rejectDispatch(dispatch);
        return false;
    }
    m_pendingStart = dispatch.request;
    m_pendingStartPort = m_desiredPort;
    if (m_synchronousStart) {
        const OnlineStartResult result = *m_synchronousStart;
        m_synchronousStart.reset();
        handle(result);
    }
    return true;
}

bool SourceLifecycleCoordinator::beginStopOnline() {
    setState(State::StoppingOnlineForPlayback);
    m_synchronousStop.reset();
    m_dispatching = DispatchKind::StopOnline;
    const CommandDispatch dispatch = m_runtime.stopOnline();
    m_dispatching = DispatchKind::None;
    if (!dispatch.accepted) {
        rejectDispatch(dispatch);
        return false;
    }
    m_pendingStop = dispatch.request;
    if (m_synchronousStop) {
        const OnlineStopResult result = *m_synchronousStop;
        m_synchronousStop.reset();
        handle(result);
    }
    return true;
}

bool SourceLifecycleCoordinator::beginLoadPlayback() {
    setState(State::LoadingPlayback);
    m_synchronousLoad.reset();
    m_dispatching = DispatchKind::LoadPlayback;
    const QString requestedPath = m_desiredSessionPath;
    const CommandDispatch dispatch = m_runtime.loadSession(requestedPath);
    m_dispatching = DispatchKind::None;
    if (!dispatch.accepted) {
        rejectDispatch(dispatch);
        return false;
    }
    m_pendingLoad = dispatch.request;
    m_pendingLoadPath = requestedPath;
    if (m_synchronousLoad) {
        const SessionLoadResult result = *m_synchronousLoad;
        m_synchronousLoad.reset();
        handle(result);
    }
    return true;
}

bool SourceLifecycleCoordinator::beginClosePlayback() {
    setState(State::ClosingPlaybackForOnline);
    m_synchronousClose.reset();
    m_dispatching = DispatchKind::ClosePlayback;
    const CommandDispatch dispatch = m_runtime.closeSession();
    m_dispatching = DispatchKind::None;
    if (!dispatch.accepted) {
        rejectDispatch(dispatch);
        return false;
    }
    m_pendingClose = dispatch.request;
    if (m_synchronousClose) {
        const SessionCloseResult result = *m_synchronousClose;
        m_synchronousClose.reset();
        handle(result);
    }
    return true;
}

void SourceLifecycleCoordinator::rejectDispatch(const CommandDispatch &dispatch) {
    setState(State::Idle);
    m_desiredSource = DesiredSource::None;
    m_transitionBoundaryActive = false;
    if (!dispatch.rejectionReason.isEmpty()) {
        emit errorRaised(dispatch.rejectionReason);
    }
}

void SourceLifecycleCoordinator::reportRejectedCommand(const CommandDispatch &dispatch) {
    if (!dispatch.accepted && !dispatch.rejectionReason.isEmpty()) {
        emit errorRaised(dispatch.rejectionReason);
    }
}

void SourceLifecycleCoordinator::receive(const OnlineStartResult &result) {
    if (!result.request.isValid())
        return;
    if (m_dispatching == DispatchKind::StartOnline) {
        m_synchronousStart = result;
    } else if (m_pendingStart.isValid() && result.request == m_pendingStart) {
        handle(result);
    }
}

void SourceLifecycleCoordinator::receive(const OnlineStopResult &result) {
    if (!result.request.isValid())
        return;
    if (m_dispatching == DispatchKind::StopOnline) {
        m_synchronousStop = result;
    } else if (m_pendingStop.isValid() && result.request == m_pendingStop) {
        handle(result);
    }
}

void SourceLifecycleCoordinator::receive(const SessionLoadResult &result) {
    if (!result.request.isValid())
        return;
    if (m_dispatching == DispatchKind::LoadPlayback) {
        m_synchronousLoad = result;
    } else if (m_pendingLoad.isValid() && result.request == m_pendingLoad) {
        handle(result);
    }
}

void SourceLifecycleCoordinator::receive(const SessionCloseResult &result) {
    if (!result.request.isValid())
        return;
    if (m_dispatching == DispatchKind::ClosePlayback) {
        m_synchronousClose = result;
    } else if (m_pendingClose.isValid() && result.request == m_pendingClose) {
        handle(result);
    }
}

void SourceLifecycleCoordinator::handle(const OnlineStartResult &result) {
    if (result.request != m_pendingStart)
        return;
    m_pendingStart = {};
    const quint16 completedPort = m_pendingStartPort;
    m_pendingStartPort = 0;
    if (!result.started) {
        setState(State::Idle);
        if (!result.error.isEmpty())
            emit errorRaised(result.error);
        if (m_desiredSource == DesiredSource::Online && m_desiredPort == completedPort) {
            m_desiredSource = DesiredSource::None;
        }
        m_transitionBoundaryActive = false;
        advanceIntent();
        return;
    }
    if (m_desiredSource != DesiredSource::Online || m_desiredPort != completedPort) {
        clearDeferredPublications();
        beginStopOnline();
        return;
    }
    m_activeEpoch = result.epoch;
    m_transitionBoundaryActive = false;
    replayDeferredPublications();
}

void SourceLifecycleCoordinator::handle(const OnlineStopResult &result) {
    if (result.request != m_pendingStop)
        return;
    m_pendingStop = {};
    m_applicationState.listening = false;
    m_applicationState.listeningPort = 0;
    setState(State::Idle);
    if (!result.stopped) {
        m_desiredSource = DesiredSource::None;
        m_transitionBoundaryActive = false;
        if (!result.error.isEmpty())
            emit errorRaised(result.error);
        transitionTo(ApplicationMode::Idle);
        return;
    }
    advanceIntent();
}

void SourceLifecycleCoordinator::handle(const SessionLoadResult &result) {
    if (result.request != m_pendingLoad)
        return;
    m_pendingLoad = {};
    const QString completedPath = m_pendingLoadPath;
    m_pendingLoadPath.clear();
    if (!result.loaded) {
        setState(State::Idle);
        if (!result.error.isEmpty())
            emit errorRaised(result.error);
        if (m_desiredSource == DesiredSource::Playback && m_desiredSessionPath == completedPath) {
            m_desiredSource = DesiredSource::None;
        }
        m_transitionBoundaryActive = false;
        advanceIntent();
        return;
    }
    if (m_desiredSource != DesiredSource::Playback || result.path != m_desiredSessionPath) {
        clearDeferredPublications();
        beginClosePlayback();
        return;
    }
    m_activeEpoch = result.epoch;
    m_applicationState.sessionLoaded = true;
    m_applicationState.loadedRecordCount = result.recordCount;
    m_viewModel.setPlaybackDuration(result.duration);
    setState(State::SessionLoaded);
    transitionTo(ApplicationMode::SessionLoaded);
    emit sessionLoaded(result.path, result.recordCount);
    m_transitionBoundaryActive = false;
    replayDeferredPublications();
}

void SourceLifecycleCoordinator::handle(const SessionCloseResult &result) {
    if (result.request != m_pendingClose)
        return;
    m_pendingClose = {};
    m_applicationState.sessionLoaded = false;
    m_applicationState.loadedRecordCount = 0;
    setState(State::Idle);
    if (!result.closed) {
        m_desiredSource = DesiredSource::None;
        m_transitionBoundaryActive = false;
        if (!result.error.isEmpty())
            emit errorRaised(result.error);
        transitionTo(ApplicationMode::Idle);
        return;
    }
    advanceIntent();
}

void SourceLifecycleCoordinator::advanceIntent() {
    if (m_state == State::ShuttingDown)
        return;
    if (m_desiredSource == DesiredSource::Online)
        beginStartOnline();
    else if (m_desiredSource == DesiredSource::Playback)
        beginLoadPlayback();
    else {
        m_transitionBoundaryActive = false;
        transitionTo(ApplicationMode::Idle);
    }
}

bool SourceLifecycleCoordinator::accepts(const RuntimeSourceEpoch &epoch) const noexcept {
    return epoch.isValid() && epoch == m_activeEpoch;
}

void SourceLifecycleCoordinator::accept(const OnlineStateEvent &event) {
    if (m_dispatching != DispatchKind::None) {
        m_deferredOnlineState = event;
        return;
    }
    if (!accepts(event.epoch) || m_desiredSource != DesiredSource::Online)
        return;
    m_applicationState.listening = event.listening;
    m_applicationState.listeningPort = event.listening ? event.port : 0;
    setState(event.listening ? State::Online : State::Idle);
    if (event.listening) {
        transitionTo(ApplicationMode::Online);
        m_viewModel.setStatusText(QStringLiteral("Listening on UDP port %1").arg(event.port));
    } else {
        m_activeEpoch = {};
        m_desiredSource = DesiredSource::None;
    }
    emit onlineStateChanged(event.listening);
}

void SourceLifecycleCoordinator::accept(const StateEvent &event) {
    if (m_dispatching != DispatchKind::None) {
        m_deferredState = event;
        return;
    }
    if (accepts(event.epoch))
        m_viewModel.setState(event.state);
}

void SourceLifecycleCoordinator::accept(const MetricsEvent &event) {
    if (m_dispatching != DispatchKind::None) {
        m_deferredMetrics = event;
        return;
    }
    if (!accepts(event.epoch))
        return;
    m_viewModel.setProcessedPacketCount(event.processedCount);
    m_viewModel.setProcessedPacketRate(event.packetsPerSecond);
    emit processedPacketCountChanged(event.processedCount);
    emit processedPacketRateChanged(event.packetsPerSecond);
}

void SourceLifecycleCoordinator::accept(const PlaybackPositionEvent &event) {
    if (m_dispatching != DispatchKind::None) {
        m_deferredPosition = event;
        return;
    }
    if (accepts(event.epoch)) {
        m_viewModel.setPlaybackPosition(event.position);
    }
}

void SourceLifecycleCoordinator::accept(const PlaybackStateEvent &event) {
    if (m_dispatching != DispatchKind::None) {
        m_deferredPlaybackState = event;
        return;
    }
    if (!accepts(event.epoch))
        return;
    if (event.playing) {
        setState(State::Playing);
        transitionTo(ApplicationMode::Playing);
    } else if (m_applicationState.sessionLoaded) {
        setState(State::PlaybackPaused);
        transitionTo(ApplicationMode::PlaybackPaused);
    }
}

void SourceLifecycleCoordinator::accept(const PlaybackFinishedEvent &event) {
    if (m_dispatching != DispatchKind::None) {
        m_deferredFinished = event;
        return;
    }
    if (!accepts(event.epoch))
        return;
    setState(State::SessionLoaded);
    transitionTo(ApplicationMode::SessionLoaded);
}

void SourceLifecycleCoordinator::replayDeferredPublications() {
    // Publications describe replaceable state, not an audit stream. Keeping the
    // latest value of each type avoids replaying an obsolete burst while still
    // restoring the complete presentation after a synchronous activation.
    const auto online = m_deferredOnlineState;
    const auto state = m_deferredState;
    const auto metrics = m_deferredMetrics;
    const auto position = m_deferredPosition;
    const auto playback = m_deferredPlaybackState;
    const auto finished = m_deferredFinished;
    clearDeferredPublications();
    if (online)
        accept(*online);
    if (state)
        accept(*state);
    if (metrics)
        accept(*metrics);
    if (position)
        accept(*position);
    if (playback)
        accept(*playback);
    if (finished)
        accept(*finished);
}

void SourceLifecycleCoordinator::clearDeferredPublications() {
    m_deferredOnlineState.reset();
    m_deferredState.reset();
    m_deferredMetrics.reset();
    m_deferredPosition.reset();
    m_deferredPlaybackState.reset();
    m_deferredFinished.reset();
}

void SourceLifecycleCoordinator::play() {
    reportRejectedCommand(m_runtime.play());
}

void SourceLifecycleCoordinator::pause() {
    reportRejectedCommand(m_runtime.pause());
}

void SourceLifecycleCoordinator::stop() {
    reportRejectedCommand(m_runtime.stop());
}

void SourceLifecycleCoordinator::seek(SessionTimestamp position) {
    const CommandDispatch dispatch = m_runtime.seek(position);
    reportRejectedCommand(dispatch);
}

void SourceLifecycleCoordinator::resetMetrics() {
    m_viewModel.setProcessedPacketCount(0);
    m_viewModel.setProcessedPacketRate(0);
    emit processedPacketCountChanged(0);
    emit processedPacketRateChanged(0);
    reportRejectedCommand(m_runtime.resetMetrics());
}

void SourceLifecycleCoordinator::applyRecordingState(bool hasSession, bool paused) {
    if (m_state == State::ShuttingDown)
        return;
    if (hasSession) {
        transitionTo(paused ? ApplicationMode::RecordingPaused : ApplicationMode::Recording);
    } else if (m_applicationState.listening) {
        transitionTo(ApplicationMode::Online);
    } else if (m_applicationState.sessionLoaded) {
        transitionTo(ApplicationMode::SessionLoaded);
    } else {
        transitionTo(ApplicationMode::Idle);
    }
}

void SourceLifecycleCoordinator::shutdown() noexcept {
    m_desiredSource = DesiredSource::None;
    m_activeEpoch = {};
    m_pendingStart = {};
    m_pendingStartPort = 0;
    m_pendingStop = {};
    m_pendingLoad = {};
    m_pendingLoadPath.clear();
    m_pendingClose = {};
    m_transitionBoundaryActive = false;
    clearDeferredPublications();
    setState(State::ShuttingDown);
    QString ignored;
    m_modes.transitionTo(ApplicationMode::ShuttingDown, &ignored);
}
