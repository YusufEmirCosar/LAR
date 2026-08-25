
#include "infrastructure/runtime/recording_runtime_worker.h"

#include "application/recording_pipeline_coordinator.h"
#include "application/recording_service.h"
#include "infrastructure/session/lar_session_writer.h"
#include "infrastructure/timing/qt_recording_clock.h"

#include <utility>

RecordingRuntimeWorker::RecordingRuntimeWorker(std::unique_ptr<IRecordingTransaction> transaction,
                                               std::unique_ptr<IRecordingClock> clock,
                                               QObject *parent)
    : QObject(parent),
      m_transaction(transaction ? std::move(transaction) : std::make_unique<LarSessionWriter>()),
      m_clock(clock ? std::move(clock) : std::make_unique<QtRecordingClock>()) {}

RecordingRuntimeWorker::~RecordingRuntimeWorker() = default;

void RecordingRuntimeWorker::initialize() {
    if (m_coordinator)
        return;
    m_recording = new RecordingService(*m_transaction, *m_clock, this);
    m_coordinator = new RecordingPipelineCoordinator(*m_recording, this);

    connect(m_coordinator, &RecordingPipelineCoordinator::recordingInputEnabled, this,
            &RecordingRuntimeWorker::recordingInputEnabled);
    connect(m_coordinator, &RecordingPipelineCoordinator::recordingDrainRequested, this,
            &RecordingRuntimeWorker::recordingDrainRequested);
    connect(m_coordinator, &RecordingPipelineCoordinator::recordingBatchHandled, this,
            &RecordingRuntimeWorker::recordingBatchHandled);
    connect(m_coordinator, &RecordingPipelineCoordinator::recordingStateChanged, this,
            [this](bool hasSession, bool paused, quint64 count) {
                emit recordingStateChanged(
                    {hasSession, paused, count, m_recording->recordingDuration()});
            });
    connect(m_coordinator, &RecordingPipelineCoordinator::persistenceRequested, this,
            &RecordingRuntimeWorker::persistenceRequested);
    connect(m_coordinator, &RecordingPipelineCoordinator::commandFinished, this,
            &RecordingRuntimeWorker::commandFinished);
    connect(m_coordinator, &RecordingPipelineCoordinator::recordingSaveFinished, this,
            &RecordingRuntimeWorker::recordingSaveFinished);
    connect(m_coordinator, &RecordingPipelineCoordinator::recordingResetFinished, this,
            &RecordingRuntimeWorker::recordingResetFinished);
    connect(m_coordinator, &RecordingPipelineCoordinator::runtimeError, this,
            [this](const QString &message) {
                emit runtimeError({{}, std::nullopt, RuntimeFailureCode::Operational, message});
            });
}

void RecordingRuntimeWorker::startRecording(const QByteArray &mappingJson,
                                            RuntimeRequestId request) {
    initialize();
    m_coordinator->startRecording(mappingJson, request);
}

void RecordingRuntimeWorker::pauseRecording(RuntimeRequestId request) {
    initialize();
    m_coordinator->pauseRecording(request);
}

void RecordingRuntimeWorker::resumeRecording(RuntimeRequestId request) {
    initialize();
    m_coordinator->resumeRecording(request);
}

void RecordingRuntimeWorker::stopRecording(const QString &targetPath, RuntimeRequestId request) {
    initialize();
    m_coordinator->stopRecording(targetPath, request);
}

void RecordingRuntimeWorker::resetRecording(RuntimeRequestId request) {
    initialize();
    m_coordinator->resetRecording(request);
}

void RecordingRuntimeWorker::snapshotRecording(const QString &targetPath,
                                               RuntimeRequestId request) {
    initialize();
    m_coordinator->snapshotRecording(targetPath, request);
}

void RecordingRuntimeWorker::discardRecording(RuntimeRequestId request) {
    initialize();
    m_coordinator->discardRecording(request);
}

void RecordingRuntimeWorker::appendRecordingBatch(quint64 batchId,
                                                  const QVector<CapturedPacket> &packets) {
    initialize();
    m_coordinator->appendRecordingBatch(batchId, packets);
}

void RecordingRuntimeWorker::recordingInputDrained(quint64 token, qint64 boundaryNanoseconds) {
    initialize();
    m_coordinator->recordingInputDrained(token, boundaryNanoseconds);
}

void RecordingRuntimeWorker::recordingInputFailed(const QString &error) {
    initialize();
    m_coordinator->recordingInputFailed(error);
}

void RecordingRuntimeWorker::persistenceFinished(quint64 requestId, bool finalSave, bool saved,
                                                 const QString &targetPath, const QString &error) {
    initialize();
    m_coordinator->persistenceFinished(requestId, finalSave, saved, targetPath, error);
}

void RecordingRuntimeWorker::shutdown() {
    if (m_coordinator)
        m_coordinator->shutdown();
}
