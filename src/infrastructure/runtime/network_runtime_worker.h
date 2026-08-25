#pragma once

/**
 * @file network_runtime_worker.h
 * @brief Network-thread worker for mapping, capture, metrics, and batching.
 */

#include "application/captured_packet.h"
#include "application/ip_access_policy.h"
#include "application/ports/mapping_repository.h"
#include "application/ports/packet_decoder.h"
#include "application/ports/runtime_event_context.h"
#include "domain/decoded_state.h"

#include <QBitArray>
#include <QObject>
#include <QSet>
#include <QVector>

#include <functional>
#include <memory>

class IDatagramSource;
class MetricsService;
class OnlineCaptureService;
class QTimer;

/**
 * @brief Owns all live-capture objects on the dedicated network thread.
 *
 * Raw recording packets are batched and back-pressured until the recording
 * worker acknowledges each batch.
 */
class NetworkRuntimeWorker final : public QObject {
    Q_OBJECT

  public:
    using DatagramSourceFactory = std::function<IDatagramSource *(QObject *parent)>;

    explicit NetworkRuntimeWorker(std::unique_ptr<IMappingRepository> mappingRepository = {},
                                  std::unique_ptr<IPacketDecoder> decoder = {},
                                  DatagramSourceFactory sourceFactory = {},
                                  QObject *parent = nullptr);
    ~NetworkRuntimeWorker() override;

  public slots:
    void initialize();
    void loadMapping(const QString &path, RuntimeRequestId request = {});
    void startOnline(quint16 port, quint64 generation = 0, RuntimeRequestId request = {});
    void setIpAccessPolicy(const IpAccessPolicy &policy, RuntimeRequestId request = {});
    void stopOnline(quint64 generation = 0, RuntimeRequestId request = {});
    void resetMetrics(RuntimeRequestId request = {});

    void setRecordingInputEnabled(bool enabled);
    void drainRecordingInput(quint64 token, bool preserveIncoming = false);
    void acknowledgeRecordingBatch(quint64 batchId);
    /**
     * @brief Controls the shutdown operation.
     */
    void shutdown();

  signals:
    void commandFinished(const RuntimeCommandResult &result);
    void mappingLoadFinished(const MappingLoadResult &result);
    void onlineStateChanged(const OnlineStateEvent &event);
    void onlineStartFinished(const OnlineStartResult &result);
    void onlineStopFinished(const OnlineStopResult &result);
    void ipAccessPolicyChangeFinished(const IpPolicyChangeResult &result);
    void stateReady(const StateEvent &event);
    void metricsChanged(const MetricsEvent &event);
    void recordingBatchReady(quint64 batchId, const QVector<CapturedPacket> &packets);
    void recordingInputDrained(quint64 token, qint64 boundaryNanoseconds);
    void recordingInputFailed(const QString &error);
    void runtimeError(const RuntimeFailure &failure);

  private slots:
    void queueRecordingPacket(const CapturedPacket &packet);
    void flushRecordingBatch();

  private:
    void reportDrainIfComplete();

    std::unique_ptr<IMappingRepository> m_mappingRepository;
    std::unique_ptr<IPacketDecoder> m_decoder;
    DatagramSourceFactory m_sourceFactory;
    IDatagramSource *m_source = nullptr;
    MetricsService *m_metrics = nullptr;
    OnlineCaptureService *m_capture = nullptr;
    QTimer *m_batchTimer = nullptr;

    QVector<CapturedPacket> m_recordingBatch;
    qsizetype m_recordingBatchBytes = 0;
    QVector<CapturedPacket> m_postDrainBatch;
    qsizetype m_postDrainBatchBytes = 0;
    quint64 m_nextBatchId = 0;
    QSet<quint64> m_outstandingBatchIds;
    bool m_recordingInputEnabled = false;
    bool m_bufferPostDrainPackets = false;
    quint64 m_pendingDrainToken = 0;
    qint64 m_pendingDrainBoundaryNanoseconds = 0;
    QString m_pendingRecordingFailure;
    quint64 m_stateGeneration = 0;
    IpAccessPolicy m_ipPolicy;
};
