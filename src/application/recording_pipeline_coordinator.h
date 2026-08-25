#pragma once

/**
 * @file recording_pipeline_coordinator.h
 * @brief Drain-and-persist policy for the asynchronous recording pipeline.
 */

#include "application/captured_packet.h"
#include "application/ports/runtime_messages.h"
#include "application/ports/session_snapshot.h"

#include <QObject>
#include <QString>
#include <QVector>

class RecordingService;

/**
 * Application-layer state machine for the asynchronous recording pipeline.
 *
 * The coordinator knows recording policy, drain ordering, immutable snapshots,
 * and persistence request identity. It does not know about threads, Qt file
 * APIs, sockets, or concrete session formats.
 */
class RecordingPipelineCoordinator final : public QObject {
    Q_OBJECT

  public:
    explicit RecordingPipelineCoordinator(RecordingService &recording, QObject *parent = nullptr);

    void startRecording(const QByteArray &mappingJson, RuntimeRequestId request = {});
    void pauseRecording(RuntimeRequestId request = {});
    void resumeRecording(RuntimeRequestId request = {});
    void stopRecording(const QString &targetPath, RuntimeRequestId request = {});
    void resetRecording(RuntimeRequestId request = {});
    void snapshotRecording(const QString &targetPath, RuntimeRequestId request = {});
    void discardRecording(RuntimeRequestId request = {});
    void appendRecordingBatch(quint64 batchId, const QVector<CapturedPacket> &packets);
    void recordingInputDrained(quint64 token, qint64 boundaryNanoseconds = 0);
    void recordingInputFailed(const QString &error);
    void persistenceFinished(quint64 requestId, bool finalSave, bool saved,
                             const QString &targetPath, const QString &error);
    /**
     * @brief Controls the shutdown operation.
     */
    void shutdown();

  signals:
    void recordingInputEnabled(bool enabled);
    void recordingDrainRequested(quint64 token, bool preserveIncoming);
    void recordingBatchHandled(quint64 batchId);
    void recordingStateChanged(bool hasSession, bool paused, quint64 recordCount);
    void persistenceRequested(quint64 requestId, bool finalSave, const SessionSnapshot &snapshot,
                              const QString &targetPath);
    void commandFinished(const RuntimeCommandResult &result);
    void recordingSaveFinished(const RecordingSaveResult &result);
    void recordingResetFinished(const RecordingResetResult &result);
    void runtimeError(const QString &error);

  private:
    enum class PendingOperation { None, Pause, Reset, Snapshot, FinalSave, Discard };

    bool beginDrain(PendingOperation operation, RuntimeRequestId request,
                    const QString &targetPath = {});
    void completeOperation(PendingOperation operation, RuntimeRequestId request, bool succeeded,
                           const QString &error = {}, const QString &targetPath = {});
    void publishState();
    bool preparePersistence(bool finalSave, RuntimeRequestId request, const QString &targetPath);

    RecordingService &m_recording;
    PendingOperation m_pendingOperation = PendingOperation::None;
    RuntimeRequestId m_pendingRequest;
    QString m_pendingTargetPath;
    bool m_resumeAfterOperation = false;
    quint64 m_nextDrainToken = 0;
    quint64 m_activeDrainToken = 0;
    quint64 m_nextPersistenceRequest = 0;
    quint64 m_activePersistenceRequest = 0;
    bool m_persistenceInFlight = false;
    bool m_finalSaveInFlight = false;
    RuntimeRequestId m_activeSaveRequest;
    bool m_hasPublishedState = false;
    bool m_lastHasSession = false;
    bool m_lastPaused = false;
    quint64 m_lastRecordCount = 0;
    SessionTimestamp m_lastRecordingDuration;
};
