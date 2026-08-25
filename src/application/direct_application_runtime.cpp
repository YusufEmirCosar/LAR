
#include "application/direct_application_runtime.h"

#include <cmath>
#include <utility>

namespace {

QString shutdownRejection() {
    return QStringLiteral("The application runtime is shutting down");
}

} // namespace

DirectApplicationRuntime::DirectApplicationRuntime(
    OnlineCaptureService &capture, RecordingService &recording, PlaybackService &playback,
    MetricsService &metrics, IMappingRepository &mappingRepository,
    ISessionPersistence &sessionPersistence, QObject *parent)
    : IApplicationRuntime(parent), m_capture(capture), m_recording(recording), m_playback(playback),
      m_metrics(metrics), m_mappingRepository(mappingRepository),
      m_sessionPersistence(sessionPersistence) {
    registerRuntimeMessageTypes();
    connect(&m_capture, &OnlineCaptureService::stateReceived, this,
            [this](const DecodedState &state) {
                if (m_activeStateSource == RuntimeStateSource::Online) {
                    emit stateReady({{RuntimeStateSource::Online, m_activeStateGeneration}, state});
                }
            });
    connect(&m_capture, &OnlineCaptureService::rawPacketReceived, &m_recording,
            qOverload<const CapturedPacket &>(&RecordingService::recordPacket));
    connect(&m_capture, &OnlineCaptureService::captureError, this, [this](const QString &message) {
        const RuntimeSourceEpoch epoch{RuntimeStateSource::Online, m_activeStateGeneration};
        emit runtimeError(
            {{},
             epoch.isValid() ? std::optional<RuntimeSourceEpoch>(epoch) : std::nullopt,
             RuntimeFailureCode::Operational,
             message});
    });

    connect(&m_metrics, &MetricsService::processedCountChanged, this,
            [this](quint64) { publishMetrics(); });
    connect(&m_metrics, &MetricsService::rateChanged, this, [this](quint64 rate) {
        m_rate = rate;
        publishMetrics();
    });
    connect(&m_recording, &RecordingService::recordingStateChanged, this,
            [this] { publishRecordingState(); });
    connect(&m_recording, &RecordingService::recordCountChanged, this,
            [this] { publishRecordingState(); });
    connect(&m_recording, &RecordingService::recordingError, this, [this](const QString &message) {
        emit runtimeError({{}, std::nullopt, RuntimeFailureCode::Operational, message});
    });

    connect(&m_playback, &PlaybackService::frameReady, this, [this](const DecodedState &state) {
        if (m_activeStateSource == RuntimeStateSource::Playback) {
            emit stateReady({{RuntimeStateSource::Playback, m_activeStateGeneration}, state});
        }
    });
    connect(&m_playback, &PlaybackService::positionChanged, this,
            [this](SessionTimestamp position) {
                if (m_activeStateSource == RuntimeStateSource::Playback) {
                    emit playbackPositionChanged(
                        {{RuntimeStateSource::Playback, m_activeStateGeneration}, position});
                }
            });
    connect(&m_playback, &PlaybackService::recordsProcessed, &m_metrics,
            &MetricsService::recordPlaybackPackets);
    connect(&m_playback, &PlaybackService::playingChanged, this, [this](bool playing) {
        if (m_activeStateSource == RuntimeStateSource::Playback) {
            emit playbackPlayingChanged(
                {{RuntimeStateSource::Playback, m_activeStateGeneration}, playing});
        }
    });
    connect(&m_playback, &PlaybackService::playbackFinished, this, [this] {
        if (m_activeStateSource == RuntimeStateSource::Playback) {
            emit playbackFinished({{RuntimeStateSource::Playback, m_activeStateGeneration}});
        }
    });
    connect(&m_playback, &PlaybackService::playbackError, this, [this](const QString &message) {
        const RuntimeSourceEpoch epoch{RuntimeStateSource::Playback, m_activeStateGeneration};
        emit runtimeError(
            {{},
             epoch.isValid() ? std::optional<RuntimeSourceEpoch>(epoch) : std::nullopt,
             RuntimeFailureCode::Operational,
             message});
    });
}

CommandDispatch DirectApplicationRuntime::loadMapping(const QString &path) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (path.isEmpty())
        return rejectCommand(QStringLiteral("Mapping path is empty"));
    const CommandDispatch dispatch = acceptCommand();
    if (m_capture.isListening()) {
        emit mappingLoadFinished(
            {dispatch.request,
             false,
             path,
             0,
             0,
             {},
             QStringLiteral("Stop online capture before replacing the packet mapping")});
        return dispatch;
    }
    PacketMapping mapping;
    QString error;
    const bool loaded = m_mappingRepository.loadFile(path, &mapping, &error);
    if (loaded) {
        m_mapping = std::move(mapping);
        m_capture.setMapping(m_mapping);
    }
    emit mappingLoadFinished({dispatch.request, loaded, path,
                              loaded ? static_cast<int>(m_mapping.bindings().size()) : 0,
                              loaded ? m_mapping.minimumPacketSize() : 0,
                              loaded ? m_mapping.json() : QByteArray{}, error});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::startOnline(quint16 port) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (port == 0)
        return rejectCommand(QStringLiteral("UDP port must be non-zero"));
    const CommandDispatch dispatch = acceptCommand();
    const RuntimeSourceEpoch epoch{RuntimeStateSource::Online, ++m_nextStateGeneration};
    m_activeStateSource = epoch.source;
    m_activeStateGeneration = epoch.generation;
    QString error;
    const bool started = m_capture.start(port, &error);
    emit onlineStartFinished({dispatch.request, epoch, started, error});
    emit onlineStateChanged({epoch, m_capture.isListening(), m_capture.port()});
    if (!started)
        m_activeStateSource = RuntimeStateSource::None;
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::setIpAccessPolicy(const IpAccessPolicy &policy) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    QString error;
    bool applied = false;
    if (m_capture.isListening()) {
        error = QStringLiteral("Stop online capture before changing the IP access policy");
    } else if (!m_capture.setIpAccessPolicy(policy)) {
        error = QStringLiteral("Datagram source rejected the IP access policy");
    } else {
        applied = true;
        m_ipPolicy = policy;
    }
    emit ipAccessPolicyChangeFinished({dispatch.request, applied, m_ipPolicy, error});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::stopOnline() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    const RuntimeSourceEpoch epoch{RuntimeStateSource::Online, m_activeStateGeneration};
    if (m_activeStateSource == RuntimeStateSource::Online) {
        m_activeStateSource = RuntimeStateSource::None;
    }
    m_capture.stop();
    emit onlineStateChanged({epoch, false, 0});
    emit onlineStopFinished({dispatch.request, epoch, true, {}});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::startRecording(const QByteArray &mappingJson) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (mappingJson.isEmpty()) {
        return rejectCommand(QStringLiteral("Recording mapping is empty"));
    }
    const CommandDispatch dispatch = acceptCommand();
    QString error;
    const bool started = m_recording.startRecording(mappingJson, &error);
    publishRecordingState();
    emit commandFinished({dispatch.request, RuntimeCommandKind::RecordingStart, started, error});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::pauseRecording() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    const bool eligible = m_recording.state() == RecordingService::State::Recording;
    if (eligible)
        m_recording.pauseRecording();
    const bool paused = eligible && m_recording.state() == RecordingService::State::Paused;
    publishRecordingState();
    emit commandFinished(
        {dispatch.request, RuntimeCommandKind::RecordingPause, paused,
         paused ? QString{} : QStringLiteral("No active recording can be paused")});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::resumeRecording() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    const bool eligible = m_recording.state() == RecordingService::State::Paused;
    if (eligible)
        m_recording.resumeRecording();
    const bool resumed = eligible && m_recording.state() == RecordingService::State::Recording;
    publishRecordingState();
    emit commandFinished(
        {dispatch.request, RuntimeCommandKind::RecordingResume, resumed,
         resumed ? QString{} : QStringLiteral("No paused recording can be resumed")});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::stopRecording(const QString &targetPath) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (targetPath.isEmpty())
        return rejectCommand(QStringLiteral("Save path is empty"));
    const CommandDispatch dispatch = acceptCommand();
    SessionSnapshot snapshot;
    QString error;
    m_recording.pauseRecording();
    bool saved = m_recording.createSnapshot(&snapshot, &error);
    if (saved)
        saved = m_sessionPersistence.save(snapshot, targetPath, &error);
    if (saved)
        m_recording.completeRecording();
    emit recordingSaveFinished({dispatch.request, true, saved, targetPath, error});
    publishRecordingState();
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::resetRecording() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    QString error;
    const bool reset = m_recording.resetRecording(&error);
    emit recordingResetFinished({dispatch.request, reset, error});
    publishRecordingState();
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::snapshotRecording(const QString &targetPath) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (targetPath.isEmpty())
        return rejectCommand(QStringLiteral("Save path is empty"));
    const CommandDispatch dispatch = acceptCommand();
    SessionSnapshot snapshot;
    QString error;
    bool saved = m_recording.createSnapshot(&snapshot, &error);
    if (saved)
        saved = m_sessionPersistence.save(snapshot, targetPath, &error);
    emit recordingSaveFinished({dispatch.request, false, saved, targetPath, error});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::discardRecording() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    m_recording.cancelRecording();
    publishRecordingState();
    emit commandFinished({dispatch.request, RuntimeCommandKind::RecordingDiscard, true, {}});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::loadSession(const QString &path) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (path.isEmpty())
        return rejectCommand(QStringLiteral("Session path is empty"));
    const CommandDispatch dispatch = acceptCommand();
    const RuntimeSourceEpoch epoch{RuntimeStateSource::Playback, ++m_nextStateGeneration};
    m_activeStateSource = epoch.source;
    m_activeStateGeneration = epoch.generation;
    QString error;
    const bool loaded = m_playback.loadSession(path, &error);
    emit sessionLoadFinished({dispatch.request, epoch, loaded, path,
                              loaded ? m_playback.recordCount() : 0,
                              loaded ? m_playback.duration() : SessionTimestamp{}, error});
    if (!loaded)
        m_activeStateSource = RuntimeStateSource::None;
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::closeSession() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    const RuntimeSourceEpoch epoch{RuntimeStateSource::Playback, m_activeStateGeneration};
    if (m_activeStateSource == RuntimeStateSource::Playback) {
        m_activeStateSource = RuntimeStateSource::None;
    }
    m_playback.closeSession();
    emit sessionClosed({dispatch.request, epoch, true, {}});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::play() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    const bool eligible = m_playback.isLoaded() && m_playback.recordCount() > 0;
    if (eligible)
        m_playback.play();
    const bool succeeded = eligible && m_playback.isPlaying();
    emit commandFinished(
        {dispatch.request, RuntimeCommandKind::PlaybackPlay, succeeded,
         succeeded ? QString{} : QStringLiteral("No loaded playback session can be played")});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::pause() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    const bool succeeded = m_playback.isLoaded();
    if (succeeded)
        m_playback.pause();
    emit commandFinished({dispatch.request, RuntimeCommandKind::PlaybackPause, succeeded,
                          succeeded ? QString{} : QStringLiteral("No playback session is loaded")});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::stop() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    const bool succeeded = m_playback.isLoaded();
    if (succeeded)
        m_playback.stop();
    emit commandFinished({dispatch.request, RuntimeCommandKind::PlaybackStop, succeeded,
                          succeeded ? QString{} : QStringLiteral("No playback session is loaded")});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::seek(SessionTimestamp position) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    const bool eligible = m_playback.isLoaded() && m_playback.recordCount() > 0;
    if (eligible)
        m_playback.seek(position);
    const bool succeeded = eligible && m_playback.isLoaded();
    emit commandFinished(
        {dispatch.request, RuntimeCommandKind::PlaybackSeek, succeeded,
         succeeded ? QString{} : QStringLiteral("No loaded playback record can be sought")});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::setPlaybackRepeat(bool enabled) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    m_playback.setRepeat(enabled);
    emit commandFinished({dispatch.request, RuntimeCommandKind::PlaybackRepeatChange, true, {}});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::setPlaybackRate(double rate) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (rate <= 0.0 || !std::isfinite(rate)) {
        return rejectCommand(QStringLiteral("Playback rate must be a positive finite number"));
    }
    const CommandDispatch dispatch = acceptCommand();
    const bool applied = m_playback.setRate(rate);
    emit commandFinished(
        {dispatch.request, RuntimeCommandKind::PlaybackRateChange, applied,
         applied ? QString{} : QStringLiteral("Playback rate could not be applied")});
    return dispatch;
}

CommandDispatch DirectApplicationRuntime::resetMetrics() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    m_metrics.reset();
    m_rate = 0;
    publishMetrics();
    emit commandFinished({dispatch.request, RuntimeCommandKind::MetricsReset, true, {}});
    return dispatch;
}

void DirectApplicationRuntime::shutdown() {
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    m_activeStateSource = RuntimeStateSource::None;
    ++m_nextStateGeneration;
    m_capture.stop();
    m_playback.closeSession();
    m_recording.cancelRecording();
}

void DirectApplicationRuntime::publishMetrics() {
    if (m_activeStateSource == RuntimeStateSource::None)
        return;
    emit metricsChanged({{m_activeStateSource, m_activeStateGeneration},
                         m_metrics.totalProcessedPackets(),
                         m_rate});
}

void DirectApplicationRuntime::publishRecordingState() {
    const bool hasSession = m_recording.isRecording();
    const bool paused = m_recording.isPaused();
    const quint64 recordCount = m_recording.recordCount();
    const SessionTimestamp duration = m_recording.recordingDuration();
    if (m_hasPublishedRecordingState && hasSession == m_lastHasRecordingSession &&
        paused == m_lastRecordingPaused && recordCount == m_lastRecordCount &&
        duration == m_lastRecordingDuration)
        return;
    m_hasPublishedRecordingState = true;
    m_lastHasRecordingSession = hasSession;
    m_lastRecordingPaused = paused;
    m_lastRecordCount = recordCount;
    m_lastRecordingDuration = duration;
    emit recordingStateChanged({hasSession, paused, recordCount, duration});
}
