
#include "application/application_facade.h"
#include "application/source_lifecycle_coordinator.h"

#include <cmath>

ApplicationFacade::ApplicationFacade(ModeCoordinator &modes, IApplicationRuntime &runtime,
                                     ApplicationViewModel &viewModel, QObject *parent)
    : QObject(parent), m_modes(modes), m_runtime(runtime), m_viewModel(viewModel) {

    connect(&m_modes, &ModeCoordinator::modeChanged, &m_viewModel, &ApplicationViewModel::setMode);

    m_sources = new SourceLifecycleCoordinator(m_modes, m_runtime, m_viewModel, m_state, this);
    connect(m_sources, &SourceLifecycleCoordinator::errorRaised, this,
            [this](const QString &error) { reportError(error); });
    connect(m_sources, &SourceLifecycleCoordinator::onlineStateChanged, this,
            &ApplicationFacade::onlineStateChanged);
    connect(m_sources, &SourceLifecycleCoordinator::sessionLoaded, this,
            &ApplicationFacade::sessionLoaded);
    connect(m_sources, &SourceLifecycleCoordinator::processedPacketCountChanged, this,
            &ApplicationFacade::processedPacketCountChanged);
    connect(m_sources, &SourceLifecycleCoordinator::processedPacketRateChanged, this,
            &ApplicationFacade::processedPacketRateChanged);

    connect(&m_runtime, &IApplicationRuntime::mappingLoadFinished, this,
            [this](const MappingLoadResult &result) {
                if (const auto accepted = m_mappingLoads.receive(result)) {
                    applyResult(*accepted);
                }
            });

    connect(&m_runtime, &IApplicationRuntime::ipAccessPolicyChangeFinished, this,
            [this](const IpPolicyChangeResult &result) {
                if (const auto accepted = m_policyChanges.receive(result)) {
                    applyResult(*accepted);
                }
            });
    connect(&m_runtime, &IApplicationRuntime::recordingStateChanged, this,
            [this](const RecordingStateEvent &event) {
                m_state.hasRecordingSession = event.hasSession;
                m_state.recordingPaused = event.hasSession && event.paused;
                m_viewModel.setRecordedPacketCount(event.recordCount);
                m_viewModel.setRecordingDuration(event.duration);
                m_sources->applyRecordingState(event.hasSession, m_state.recordingPaused);
                if (!event.hasSession && m_stopOnlineAfterFinalSave) {
                    m_stopOnlineAfterFinalSave = false;
                    if (m_state.listening) {
                        m_sources->stopOnline();
                    }
                }
                if (!event.hasSession && m_stopOnlineAfterDiscard) {
                    m_stopOnlineAfterDiscard = false;
                    if (m_state.listening) {
                        m_sources->stopOnline();
                    }
                }
                emit recordingChanged(event.hasSession && !event.paused);
            });
    connect(&m_runtime, &IApplicationRuntime::recordingSaveFinished, this,
            [this](const RecordingSaveResult &result) {
                if (const auto accepted = m_recordingSaves.receive(result)) {
                    applyResult(*accepted);
                }
            });
    connect(&m_runtime, &IApplicationRuntime::recordingResetFinished, this,
            [this](const RecordingResetResult &result) {
                if (const auto accepted = m_recordingResets.receive(result)) {
                    applyResult(*accepted);
                }
            });
    connect(&m_runtime, &IApplicationRuntime::commandFinished, this,
            [this](const RuntimeCommandResult &result) {
                if (m_stopOnlineAfterDiscard &&
                    result.command == RuntimeCommandKind::RecordingDiscard && !result.succeeded) {
                    m_stopOnlineAfterDiscard = false;
                }
                if (!result.succeeded && !result.error.isEmpty())
                    reportError(result.error);
            });

    connect(&m_runtime, &IApplicationRuntime::runtimeError, this,
            [this](const RuntimeFailure &failure) {
                if (!failure.epoch)
                    reportError(failure.message);
            });
}

ApplicationFacade::ApplicationFacade(ModeCoordinator &modes, IApplicationRuntime &runtime,
                                     ApplicationViewModel &viewModel,
                                     IpAccessPolicyService &ipAccessPolicyService, QObject *parent)
    : ApplicationFacade(modes, runtime, viewModel, parent) {
    m_ipAccessPolicyService = &ipAccessPolicyService;
}

bool ApplicationFacade::loadMapping(const QString &path) {
    if (m_state.listening || m_state.hasRecordingSession) {
        return reportError(
            QStringLiteral("Stop online capture before replacing the packet mapping"));
    }
    if (path.isEmpty()) {
        return reportError(QStringLiteral("Packet mapping path is empty"));
    }
    m_mappingLoads.beginDispatch();
    const CommandDispatch dispatch = m_runtime.loadMapping(path);
    const auto synchronous = m_mappingLoads.finishDispatch(dispatch);
    if (synchronous)
        applyResult(*synchronous);
    if (!dispatch.accepted && !dispatch.rejectionReason.isEmpty()) {
        reportError(dispatch.rejectionReason);
    }
    return dispatch.accepted;
}

bool ApplicationFacade::startOnline(quint16 port) {
    if (m_state.mappingJson.isEmpty()) {
        return reportError(QStringLiteral("Cannot start online capture without a valid mapping"));
    }
    if (m_state.hasRecordingSession) {
        return reportError(
            QStringLiteral("Save or reset the active recording before starting online capture"));
    }
    return m_sources->startOnline(port);
}

bool ApplicationFacade::setIpAccessPolicy(const IpAccessPolicy &policy) {
    if (m_state.listening) {
        return reportError(
            QStringLiteral("Stop online capture before changing the IP access policy"));
    }
    m_policyChanges.beginDispatch();
    const CommandDispatch dispatch = m_runtime.setIpAccessPolicy(policy);
    const auto synchronous = m_policyChanges.finishDispatch(dispatch);
    if (synchronous)
        applyResult(*synchronous);
    if (!dispatch.accepted && !dispatch.rejectionReason.isEmpty()) {
        reportError(dispatch.rejectionReason);
    }
    return dispatch.accepted;
}

bool ApplicationFacade::loadIpAccessPolicy(const QString &path) {
    if (m_state.listening) {
        return reportError(
            QStringLiteral("Stop online capture before changing the IP access policy"));
    }
    if (!m_ipAccessPolicyService) {
        return reportError(QStringLiteral("IP access policy repository is unavailable"));
    }

    IpAccessPolicy policy;
    QString error;
    if (!m_ipAccessPolicyService->load(path, &policy, &error)) {
        return reportError(error.isEmpty() ? QStringLiteral("Could not load IP whitelist") : error);
    }
    return setIpAccessPolicy(policy);
}

bool ApplicationFacade::stopOnline() {
    if (m_state.hasRecordingSession) {
        return reportError(
            QStringLiteral("Save or reset the active recording before stopping online capture"));
    }
    const bool stopped = m_sources->stopOnline();
    if (stopped) {
        m_viewModel.setStatusText(QStringLiteral("Online capture stopped"));
    }
    return stopped;
}

bool ApplicationFacade::stopOnlineAndDiscardRecording() {
    if (!m_state.hasRecordingSession) {
        return stopOnline();
    }
    if (recordingOperationPending()) {
        return reportError(QStringLiteral("A recording operation is already in progress"));
    }

    setRecordingOperationState(RecordingOperationState::Idle);
    m_recordingSaves.clear();
    m_recordingResets.clear();
    m_stopOnlineAfterFinalSave = false;
    m_stopOnlineAfterDiscard = true;
    const CommandDispatch dispatch = m_runtime.discardRecording();
    if (!dispatch.accepted) {
        m_stopOnlineAfterDiscard = false;
        if (!dispatch.rejectionReason.isEmpty()) {
            reportError(dispatch.rejectionReason);
        }
    }
    return dispatch.accepted;
}

bool ApplicationFacade::startRecording() {
    if (recordingOperationPending()) {
        return reportError(QStringLiteral("A recording operation is already in progress"));
    }
    if (m_state.mappingJson.isEmpty()) {
        return reportError(QStringLiteral("Cannot record without a valid mapping"));
    }
    if (!m_state.listening || m_modes.mode() != ApplicationMode::Online) {
        return reportError(
            QStringLiteral("Recording can start only while online capture is active"));
    }
    return m_runtime.startRecording(m_state.mappingJson);
}

void ApplicationFacade::pauseRecording() {
    if (recordingOperationPending())
        return;
    m_runtime.pauseRecording();
}

void ApplicationFacade::resumeRecording() {
    if (recordingOperationPending())
        return;
    m_runtime.resumeRecording();
}

bool ApplicationFacade::stopRecording(const QString &targetPath) {
    if (targetPath.isEmpty()) {
        return reportError(QStringLiteral("Recording destination path is empty"));
    }
    if (recordingOperationPending()) {
        return reportError(QStringLiteral("A recording operation is already in progress"));
    }
    setRecordingOperationState(RecordingOperationState::Finalizing);
    m_recordingSaves.beginDispatch();
    const CommandDispatch dispatch = m_runtime.stopRecording(targetPath);
    const auto synchronous = m_recordingSaves.finishDispatch(dispatch);
    if (synchronous)
        applyResult(*synchronous);
    if (!dispatch.accepted && m_recordingOperationState == RecordingOperationState::Finalizing) {
        setRecordingOperationState(RecordingOperationState::Idle);
    }
    if (!dispatch.accepted && !dispatch.rejectionReason.isEmpty()) {
        reportError(dispatch.rejectionReason);
    }
    return dispatch.accepted;
}

bool ApplicationFacade::resetRecording() {
    if (recordingOperationPending()) {
        return reportError(QStringLiteral("A recording operation is already in progress"));
    }
    setRecordingOperationState(RecordingOperationState::Resetting);
    m_recordingResets.beginDispatch();
    const CommandDispatch dispatch = m_runtime.resetRecording();
    const auto synchronous = m_recordingResets.finishDispatch(dispatch);
    if (synchronous)
        applyResult(*synchronous);
    if (!dispatch.accepted && m_recordingOperationState == RecordingOperationState::Resetting) {
        setRecordingOperationState(RecordingOperationState::Idle);
    }
    if (!dispatch.accepted && !dispatch.rejectionReason.isEmpty()) {
        reportError(dispatch.rejectionReason);
    }
    return dispatch.accepted;
}

bool ApplicationFacade::snapshotRecording(const QString &targetPath) {
    if (targetPath.isEmpty()) {
        return reportError(QStringLiteral("Snapshot destination path is empty"));
    }
    if (recordingOperationPending()) {
        return reportError(QStringLiteral("A recording operation is already in progress"));
    }
    setRecordingOperationState(RecordingOperationState::SavingSnapshot);
    m_recordingSaves.beginDispatch();
    const CommandDispatch dispatch = m_runtime.snapshotRecording(targetPath);
    const auto synchronous = m_recordingSaves.finishDispatch(dispatch);
    if (synchronous)
        applyResult(*synchronous);
    if (!dispatch.accepted &&
        m_recordingOperationState == RecordingOperationState::SavingSnapshot) {
        setRecordingOperationState(RecordingOperationState::Idle);
    }
    if (!dispatch.accepted && !dispatch.rejectionReason.isEmpty()) {
        reportError(dispatch.rejectionReason);
    }
    return dispatch.accepted;
}

void ApplicationFacade::discardRecording() noexcept {
    setRecordingOperationState(RecordingOperationState::Idle);
    m_recordingSaves.clear();
    m_recordingResets.clear();
    m_stopOnlineAfterFinalSave = false;
    m_stopOnlineAfterDiscard = false;
    m_runtime.discardRecording();
}

bool ApplicationFacade::loadSession(const QString &path) {
    if (m_state.hasRecordingSession) {
        return reportError(
            QStringLiteral("Save or reset the active recording before loading a session"));
    }
    if (path.isEmpty()) {
        return reportError(QStringLiteral("Session path is empty"));
    }
    return m_sources->loadSession(path);
}

void ApplicationFacade::closeSession() {
    m_sources->closeSession();
}

void ApplicationFacade::play() {
    m_sources->play();
}
void ApplicationFacade::pause() {
    m_sources->pause();
}
void ApplicationFacade::stop() {
    m_sources->stop();
}
void ApplicationFacade::seek(double positionSeconds) {
    const auto position = SessionTimestamp::fromSeconds(positionSeconds);
    if (!position) {
        reportError(QStringLiteral("Playback position must be within the supported finite range"));
        return;
    }
    seek(*position);
}

void ApplicationFacade::seek(SessionTimestamp position) {
    m_sources->seek(position);
}

bool ApplicationFacade::setPlaybackRate(double rate) {
    if (rate <= 0.0 || !std::isfinite(rate)) {
        return reportError(QStringLiteral("Playback rate must be a positive finite number"));
    }
    const CommandDispatch dispatch = m_runtime.setPlaybackRate(rate);
    if (dispatch.accepted)
        m_viewModel.setPlaybackRate(rate);
    else if (!dispatch.rejectionReason.isEmpty()) {
        reportError(dispatch.rejectionReason);
    }
    return dispatch.accepted;
}

bool ApplicationFacade::setPlaybackRepeat(bool enabled) {
    const CommandDispatch dispatch = m_runtime.setPlaybackRepeat(enabled);
    if (!dispatch.accepted && !dispatch.rejectionReason.isEmpty())
        reportError(dispatch.rejectionReason);
    return dispatch.accepted;
}

void ApplicationFacade::resetProcessedPacketCount() {
    m_sources->resetMetrics();
}

void ApplicationFacade::shutdown() noexcept {
    if (m_state.shuttingDown)
        return;
    m_state.shuttingDown = true;
    setRecordingOperationState(RecordingOperationState::Idle);
    m_mappingLoads.clear();
    m_policyChanges.clear();
    m_recordingSaves.clear();
    m_recordingResets.clear();
    m_stopOnlineAfterDiscard = false;
    m_sources->shutdown();
    m_runtime.shutdown();
}

bool ApplicationFacade::reportError(const QString &error) {
    const QString message =
        error.isEmpty() ? QStringLiteral("The requested operation failed") : error;
    m_viewModel.setLastError(message);
    emit errorRaised(message);
    return false;
}

void ApplicationFacade::applyResult(const MappingLoadResult &result) {
    if (!result.loaded) {
        reportError(result.error);
        return;
    }
    m_state.mappingJson = result.mappingJson;
    m_state.mappedFieldCount = result.mappedFieldCount;
    m_state.minimumPacketSize = result.minimumPacketSize;
    m_viewModel.setStatusText(QStringLiteral("Mapping loaded successfully"));
    emit mappingLoaded(result.path, result.mappedFieldCount, result.minimumPacketSize);
}

void ApplicationFacade::applyResult(const IpPolicyChangeResult &result) {
    if (result.applied) {
        m_ipAccessPolicy = result.policy;
    } else {
        reportError(result.error);
    }
    emit ipAccessPolicyChangeFinished(result.applied, m_ipAccessPolicy);
}

void ApplicationFacade::applyResult(const RecordingSaveResult &result) {
    if (!result.saved) {
        setRecordingOperationState(RecordingOperationState::FailedRetained);
        reportError(result.error);
        return;
    }

    setRecordingOperationState(RecordingOperationState::Idle);
    if (!result.finalSave)
        return;

    m_stopOnlineAfterFinalSave = true;
    if (!m_state.hasRecordingSession) {
        m_stopOnlineAfterFinalSave = false;
        if (m_state.listening)
            m_sources->stopOnline();
    }
    m_viewModel.setStatusText(QStringLiteral("Recording saved successfully"));
}

void ApplicationFacade::applyResult(const RecordingResetResult &result) {
    setRecordingOperationState(result.reset ? RecordingOperationState::Idle
                                            : RecordingOperationState::FailedRetained);
    if (!result.reset)
        reportError(result.error);
}

void ApplicationFacade::setRecordingOperationState(RecordingOperationState state) {
    if (m_recordingOperationState == state)
        return;
    m_recordingOperationState = state;
    emit recordingOperationStateChanged(state);
}
