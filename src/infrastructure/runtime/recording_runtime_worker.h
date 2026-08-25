#pragma once

/**
 * @file recording_runtime_worker.h
 * @brief Session-thread worker for recording batches and drain coordination.
 */

#include "application/captured_packet.h"
#include "application/ports/recording_clock.h"
#include "application/ports/recording_transaction.h"
#include "application/ports/runtime_messages.h"

#include <QObject>
#include <QVector>

#include <memory>

class RecordingService;
class RecordingPipelineCoordinator;

/**
 * Session-thread host for the recording pipeline only.
 *
 * It creates thread-affine recording collaborators and forwards queued commands
 * to the application-layer RecordingPipelineCoordinator.
 */
class RecordingRuntimeWorker final : public QObject {
    Q_OBJECT

  public:
    explicit RecordingRuntimeWorker(std::unique_ptr<IRecordingTransaction> transaction = {},
                                    std::unique_ptr<IRecordingClock> clock = {},
                                    QObject *parent = nullptr);
    ~RecordingRuntimeWorker() override;

  public slots:
    void initialize();
    void startRecording(const QByteArray &mappingJson, RuntimeRequestId request = {});
    void pauseRecording(RuntimeRequestId request = {});
    void resumeRecording(RuntimeRequestId request = {});
    void stopRecording(const QString &targetPath, RuntimeRequestId request = {});
    void resetRecording(RuntimeRequestId request = {});
    void snapshotRecording(const QString &targetPath, RuntimeRequestId request = {});
    void discardRecording(RuntimeRequestId request = {});
    void appendRecordingBatch(quint64 batchId, const QVector<CapturedPacket> &packets);
    void recordingInputDrained(quint64 token, qint64 boundaryNanoseconds);
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
    void recordingStateChanged(const RecordingStateEvent &event);
    void persistenceRequested(quint64 requestId, bool finalSave, const SessionSnapshot &snapshot,
                              const QString &targetPath);
    void commandFinished(const RuntimeCommandResult &result);
    void recordingSaveFinished(const RecordingSaveResult &result);
    void recordingResetFinished(const RecordingResetResult &result);
    void runtimeError(const RuntimeFailure &failure);

  private:
    std::unique_ptr<IRecordingTransaction> m_transaction;
    std::unique_ptr<IRecordingClock> m_clock;
    RecordingService *m_recording = nullptr;
    RecordingPipelineCoordinator *m_coordinator = nullptr;
};
