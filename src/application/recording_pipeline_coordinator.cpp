
#include "application/recording_pipeline_coordinator.h"

#include "application/recording_service.h"

#include <utility>

RecordingPipelineCoordinator::RecordingPipelineCoordinator(RecordingService &recording,
                                                           QObject *parent)
    : QObject(parent), m_recording(recording) {
    connect(&m_recording, &RecordingService::recordingStateChanged, this,
            [this] { publishState(); });
    connect(&m_recording, &RecordingService::recordCountChanged, this, [this] { publishState(); });
    connect(&m_recording, &RecordingService::recordingError, this, [this](const QString &error) {
        emit recordingInputEnabled(false);
        emit runtimeError(error);
    });
}

void RecordingPipelineCoordinator::startRecording(const QByteArray &mappingJson,
                                                  RuntimeRequestId request) {
    QString error;
    const bool started = m_recording.startRecording(mappingJson, &error);
    if (!started) {
        publishState();
    } else {
        emit recordingInputEnabled(true);
    }
    emit commandFinished({request, RuntimeCommandKind::RecordingStart, started, std::move(error)});
}

void RecordingPipelineCoordinator::pauseRecording(RuntimeRequestId request) {
    if (m_recording.state() != RecordingService::State::Recording) {
        emit commandFinished({request, RuntimeCommandKind::RecordingPause, false,
                              QStringLiteral("No active recording can be paused")});
        return;
    }
    beginDrain(PendingOperation::Pause, request);
}

void RecordingPipelineCoordinator::resumeRecording(RuntimeRequestId request) {
    if (m_finalSaveInFlight) {
        emit commandFinished(
            {request, RuntimeCommandKind::RecordingResume, false,
             QStringLiteral("Recording cannot resume while its final save is in progress")});
        return;
    }
    if (m_pendingOperation != PendingOperation::None) {
        emit commandFinished(
            {request, RuntimeCommandKind::RecordingResume, false,
             QStringLiteral("Recording cannot resume while another operation is pending")});
        return;
    }
    if (m_recording.state() != RecordingService::State::Paused) {
        emit commandFinished({request, RuntimeCommandKind::RecordingResume, false,
                              QStringLiteral("No paused recording can be resumed")});
        return;
    }
    m_recording.resumeRecording();
    const bool resumed = m_recording.state() == RecordingService::State::Recording;
    if (resumed)
        emit recordingInputEnabled(true);
    emit commandFinished(
        {request, RuntimeCommandKind::RecordingResume, resumed,
         resumed ? QString{} : QStringLiteral("The recording could not be resumed")});
}

void RecordingPipelineCoordinator::stopRecording(const QString &targetPath,
                                                 RuntimeRequestId request) {
    if (m_persistenceInFlight) {
        emit recordingSaveFinished({request, true, false, targetPath,
                                    QStringLiteral("Another session save is already in progress")});
        return;
    }
    if (m_recording.state() == RecordingService::State::Idle) {
        emit recordingSaveFinished(
            {request, true, false, targetPath, QStringLiteral("No recording session is active")});
        return;
    }
    beginDrain(PendingOperation::FinalSave, request, targetPath);
}

void RecordingPipelineCoordinator::resetRecording(RuntimeRequestId request) {
    if (m_persistenceInFlight) {
        emit recordingResetFinished(
            {request, false,
             QStringLiteral("Recording cannot reset while a session save is in progress")});
        return;
    }
    if (m_recording.state() == RecordingService::State::Idle) {
        emit recordingResetFinished(
            {request, false, QStringLiteral("No recording session is active")});
        return;
    }
    beginDrain(PendingOperation::Reset, request);
}

void RecordingPipelineCoordinator::snapshotRecording(const QString &targetPath,
                                                     RuntimeRequestId request) {
    if (m_persistenceInFlight) {
        emit recordingSaveFinished({request, false, false, targetPath,
                                    QStringLiteral("Another session save is already in progress")});
        return;
    }
    if (m_recording.state() == RecordingService::State::Idle) {
        emit recordingSaveFinished(
            {request, false, false, targetPath, QStringLiteral("No recording session is active")});
        return;
    }
    beginDrain(PendingOperation::Snapshot, request, targetPath);
}

void RecordingPipelineCoordinator::discardRecording(RuntimeRequestId request) {
    if (m_persistenceInFlight) {
        emit commandFinished(
            {request, RuntimeCommandKind::RecordingDiscard, false,
             QStringLiteral("Recording cannot be discarded while a session save is in progress")});
        return;
    }
    if (m_recording.state() == RecordingService::State::Idle) {
        publishState();
        emit commandFinished({request, RuntimeCommandKind::RecordingDiscard, true, {}});
        return;
    }
    beginDrain(PendingOperation::Discard, request);
}

void RecordingPipelineCoordinator::appendRecordingBatch(quint64 batchId,
                                                        const QVector<CapturedPacket> &packets) {
    if (m_recording.state() == RecordingService::State::Recording) {
        m_recording.recordPackets(packets);
    }
    emit recordingBatchHandled(batchId);
}

void RecordingPipelineCoordinator::recordingInputDrained(quint64 token,
                                                         qint64 boundaryNanoseconds) {
    if (token == 0 || token != m_activeDrainToken)
        return;
    m_activeDrainToken = 0;

    const PendingOperation operation = m_pendingOperation;
    const RuntimeRequestId request = m_pendingRequest;
    const QString targetPath = m_pendingTargetPath;
    m_pendingOperation = PendingOperation::None;
    m_pendingRequest = {};
    m_pendingTargetPath.clear();

    switch (operation) {
    case PendingOperation::Pause:
        m_recording.pauseRecordingAt(boundaryNanoseconds);
        completeOperation(operation, request,
                          m_recording.state() == RecordingService::State::Paused,
                          m_recording.state() == RecordingService::State::Paused
                              ? QString{}
                              : QStringLiteral("The recording could not be paused"));
        break;
    case PendingOperation::Reset: {
        QString error;
        const bool reset = m_recording.resetRecordingAt(boundaryNanoseconds, &error);
        completeOperation(operation, request, reset, error);
        if (m_resumeAfterOperation && m_recording.state() == RecordingService::State::Recording)
            emit recordingInputEnabled(true);
        break;
    }
    case PendingOperation::Snapshot:
        preparePersistence(false, request, targetPath);
        if (m_resumeAfterOperation)
            emit recordingInputEnabled(true);
        break;
    case PendingOperation::FinalSave:
        m_recording.pauseRecordingAt(boundaryNanoseconds);
        preparePersistence(true, request, targetPath);
        break;
    case PendingOperation::Discard:
        m_recording.cancelRecording();
        completeOperation(operation, request, true);
        break;
    case PendingOperation::None:
        break;
    }
    m_resumeAfterOperation = false;
}

void RecordingPipelineCoordinator::recordingInputFailed(const QString &error) {
    const PendingOperation operation = m_pendingOperation;
    const RuntimeRequestId request = m_pendingRequest;
    const QString targetPath = m_pendingTargetPath;
    m_pendingOperation = PendingOperation::None;
    m_pendingRequest = {};
    m_pendingTargetPath.clear();
    m_activeDrainToken = 0;
    m_resumeAfterOperation = false;
    if (m_recording.state() == RecordingService::State::Recording)
        m_recording.pauseRecording();
    emit recordingInputEnabled(false);
    if (operation == PendingOperation::None)
        emit runtimeError(error);
    else
        completeOperation(operation, request, false, error, targetPath);
}

void RecordingPipelineCoordinator::persistenceFinished(quint64 requestId, bool finalSave,
                                                       bool saved, const QString &targetPath,
                                                       const QString &error) {
    // A save can finish after shutdown or after another transaction has claimed
    // the completion channel. Both identities must match before the recording is
    // allowed to transition to its finalized state.
    if (!m_persistenceInFlight || requestId != m_activePersistenceRequest ||
        finalSave != m_finalSaveInFlight) {
        return;
    }
    m_persistenceInFlight = false;
    m_finalSaveInFlight = false;
    m_activePersistenceRequest = 0;
    const RuntimeRequestId commandRequest = m_activeSaveRequest;
    m_activeSaveRequest = {};
    if (finalSave && saved)
        m_recording.completeRecording();
    emit recordingSaveFinished({commandRequest, finalSave, saved, targetPath, error});
}

void RecordingPipelineCoordinator::shutdown() {
    m_recording.cancelRecording();
    m_pendingOperation = PendingOperation::None;
    m_pendingRequest = {};
    m_activeDrainToken = 0;
    m_activePersistenceRequest = 0;
    m_persistenceInFlight = false;
    m_finalSaveInFlight = false;
    m_activeSaveRequest = {};
}

bool RecordingPipelineCoordinator::beginDrain(PendingOperation operation, RuntimeRequestId request,
                                              const QString &targetPath) {
    if (m_pendingOperation != PendingOperation::None || m_activeDrainToken != 0) {
        completeOperation(operation, request, false,
                          QStringLiteral("Another recording operation is already pending"),
                          targetPath);
        return false;
    }
    m_resumeAfterOperation = (m_recording.state() == RecordingService::State::Recording);
    m_pendingOperation = operation;
    m_pendingRequest = request;
    m_pendingTargetPath = targetPath;
    m_activeDrainToken = ++m_nextDrainToken;

    // Snapshot and reset are non-terminal: packets arriving behind the fence
    // belong to the continuing transaction. Terminal operations deliberately
    // keep input disabled once the pre-fence batch has drained.
    const bool preserveIncoming =
        m_resumeAfterOperation &&
        (operation == PendingOperation::Snapshot || operation == PendingOperation::Reset);
    emit recordingDrainRequested(m_activeDrainToken, preserveIncoming);
    return true;
}

void RecordingPipelineCoordinator::completeOperation(PendingOperation operation,
                                                     RuntimeRequestId request, bool succeeded,
                                                     const QString &error,
                                                     const QString &targetPath) {
    switch (operation) {
    case PendingOperation::Pause:
        emit commandFinished({request, RuntimeCommandKind::RecordingPause, succeeded, error});
        break;
    case PendingOperation::Reset:
        emit recordingResetFinished({request, succeeded, error});
        break;
    case PendingOperation::Snapshot:
        emit recordingSaveFinished({request, false, succeeded, targetPath, error});
        break;
    case PendingOperation::FinalSave:
        emit recordingSaveFinished({request, true, succeeded, targetPath, error});
        break;
    case PendingOperation::Discard:
        emit commandFinished({request, RuntimeCommandKind::RecordingDiscard, succeeded, error});
        break;
    case PendingOperation::None:
        break;
    }
}

void RecordingPipelineCoordinator::publishState() {
    const bool hasSession = m_recording.isRecording();
    const bool paused = m_recording.isPaused();
    const quint64 recordCount = m_recording.recordCount();
    const SessionTimestamp duration = m_recording.recordingDuration();
    if (m_hasPublishedState && hasSession == m_lastHasSession && paused == m_lastPaused &&
        recordCount == m_lastRecordCount && duration == m_lastRecordingDuration) {
        return;
    }
    m_hasPublishedState = true;
    m_lastHasSession = hasSession;
    m_lastPaused = paused;
    m_lastRecordCount = recordCount;
    m_lastRecordingDuration = duration;
    emit recordingStateChanged(hasSession, paused, recordCount);
}

bool RecordingPipelineCoordinator::preparePersistence(bool finalSave, RuntimeRequestId request,
                                                      const QString &targetPath) {
    SessionSnapshot snapshot;
    QString error;
    if (!m_recording.createSnapshot(&snapshot, &error)) {
        emit recordingSaveFinished({request, finalSave, false, targetPath, error});
        return false;
    }
    m_persistenceInFlight = true;
    m_finalSaveInFlight = finalSave;
    m_activeSaveRequest = request;
    m_activePersistenceRequest = ++m_nextPersistenceRequest;
    emit persistenceRequested(m_activePersistenceRequest, finalSave, snapshot, targetPath);
    return true;
}
