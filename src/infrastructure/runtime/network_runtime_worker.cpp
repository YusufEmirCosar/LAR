
#include "infrastructure/runtime/network_runtime_worker.h"

#include "application/metrics_service.h"
#include "application/online_capture_service.h"
#include "infrastructure/mapping/json_mapping_repository.h"
#include "infrastructure/mapping/mapped_packet_decoder.h"
#include "infrastructure/network/qt_udp_datagram_source.h"

#include <QTimer>

#include <chrono>
#include <utility>

namespace {
constexpr int RecordingBatchIntervalMs = 2;
constexpr int MaximumPacketsPerBatch = 2048;
constexpr qsizetype MaximumBytesPerBatch = 2 * 1024 * 1024;
constexpr int MaximumOutstandingBatches = 8;
constexpr qsizetype MaximumBufferedRecordingBytes = 32 * 1024 * 1024;

qint64 monotonicNowNanoseconds() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

class UnavailableDatagramSource final : public IDatagramSource {
  public:
    explicit UnavailableDatagramSource(QObject *parent) : IDatagramSource(parent) {}
    bool start(quint16, QString *error) override {
        if (error) {
            *error = QStringLiteral("The datagram source is unavailable");
        }
        return false;
    }
    void stop() override {}
    bool isListening() const override {
        return false;
    }
    quint16 localPort() const override {
        return 0;
    }
    bool setIpAccessPolicy(const IpAccessPolicy &) override {
        return false;
    }
};
} // namespace

NetworkRuntimeWorker::NetworkRuntimeWorker(std::unique_ptr<IMappingRepository> mappingRepository,
                                           std::unique_ptr<IPacketDecoder> decoder,
                                           DatagramSourceFactory sourceFactory, QObject *parent)
    : QObject(parent),
      m_mappingRepository(mappingRepository ? std::move(mappingRepository)
                                            : std::make_unique<JsonMappingRepository>()),
      m_decoder(decoder ? std::move(decoder) : std::make_unique<MappedPacketDecoder>()),
      m_sourceFactory(sourceFactory ? std::move(sourceFactory)
                                    : [](QObject *sourceParent) -> IDatagramSource * {
          return new QtUdpDatagramSource(sourceParent);
      }) {}

NetworkRuntimeWorker::~NetworkRuntimeWorker() = default;

void NetworkRuntimeWorker::initialize() {
    if (m_capture)
        return;

    m_source = m_sourceFactory(this);
    if (!m_source) {
        m_source = new UnavailableDatagramSource(this);
        emit runtimeError({{},
                           std::nullopt,
                           RuntimeFailureCode::Construction,
                           QStringLiteral("Datagram source factory returned null")});
    }
    m_metrics = new MetricsService(this);
    m_capture = new OnlineCaptureService(*m_source, *m_decoder, *m_metrics, this);
    m_batchTimer = new QTimer(this);
    m_batchTimer->setInterval(RecordingBatchIntervalMs);
    m_batchTimer->setTimerType(Qt::PreciseTimer);

    connect(m_capture, &OnlineCaptureService::stateReceived, this,
            [this](const DecodedState &state) {
                emit stateReady({{RuntimeStateSource::Online, m_stateGeneration}, state});
            });
    connect(m_capture, &OnlineCaptureService::rawPacketReceived, this,
            &NetworkRuntimeWorker::queueRecordingPacket);
    connect(m_capture, &OnlineCaptureService::captureError, this, [this](const QString &message) {
        emit runtimeError({{},
                           RuntimeSourceEpoch{RuntimeStateSource::Online, m_stateGeneration},
                           RuntimeFailureCode::Operational,
                           message});
    });
    connect(m_metrics, &MetricsService::rateChanged, this, [this](quint64 rate) {
        if (!m_capture->isListening())
            return;
        emit metricsChanged({{RuntimeStateSource::Online, m_stateGeneration},
                             m_metrics->totalProcessedPackets(),
                             rate});
    });
    connect(m_batchTimer, &QTimer::timeout, this, &NetworkRuntimeWorker::flushRecordingBatch);
    m_batchTimer->start();
}

void NetworkRuntimeWorker::loadMapping(const QString &path, RuntimeRequestId request) {
    initialize();
    if (m_capture->isListening()) {
        const QString error =
            QStringLiteral("Stop online capture before replacing the packet mapping");
        emit mappingLoadFinished({request, false, path, 0, 0, {}, error});
        return;
    }

    PacketMapping mapping;
    QString error;
    const bool loaded = m_mappingRepository->loadFile(path, &mapping, &error);
    if (loaded)
        m_capture->setMapping(mapping);
    emit mappingLoadFinished(
        {request, loaded, path, loaded ? static_cast<int>(mapping.bindings().size()) : 0,
         loaded ? mapping.minimumPacketSize() : 0, loaded ? mapping.json() : QByteArray{}, error});
}

void NetworkRuntimeWorker::startOnline(quint16 port, quint64 generation, RuntimeRequestId request) {
    initialize();
    m_stateGeneration = generation != 0 ? generation : m_stateGeneration + 1;
    m_metrics->reset();
    QString error;
    const bool started = m_capture->start(port, &error);
    const RuntimeSourceEpoch epoch{RuntimeStateSource::Online, m_stateGeneration};
    emit onlineStartFinished({request, epoch, started, error});
    emit onlineStateChanged({epoch, m_capture->isListening(),
                             m_capture->isListening() ? m_capture->port() : quint16{0}});
}

void NetworkRuntimeWorker::setIpAccessPolicy(const IpAccessPolicy &policy,
                                             RuntimeRequestId request) {
    initialize();
    if (m_capture->isListening()) {
        emit ipAccessPolicyChangeFinished(
            {request, false, m_ipPolicy,
             QStringLiteral("Stop online capture before changing the IP access policy")});
        return;
    }
    if (!m_capture->setIpAccessPolicy(policy)) {
        emit ipAccessPolicyChangeFinished(
            {request, false, m_ipPolicy,
             QStringLiteral("Datagram source rejected the IP access policy")});
        return;
    }
    m_ipPolicy = policy;
    emit ipAccessPolicyChangeFinished({request, true, policy, {}});
}

void NetworkRuntimeWorker::stopOnline(quint64 generation, RuntimeRequestId request) {
    initialize();
    const RuntimeSourceEpoch epoch{RuntimeStateSource::Online,
                                   generation != 0 ? generation : m_stateGeneration};
    m_recordingInputEnabled = false;
    m_capture->stop();
    flushRecordingBatch();
    emit onlineStateChanged({epoch, false, 0});
    emit onlineStopFinished({request, epoch, true, {}});
}

void NetworkRuntimeWorker::resetMetrics(RuntimeRequestId request) {
    initialize();
    m_metrics->reset();
    emit metricsChanged({{RuntimeStateSource::Online, m_stateGeneration}, 0, 0});
    emit commandFinished({request, RuntimeCommandKind::MetricsReset, true, {}});
}

void NetworkRuntimeWorker::setRecordingInputEnabled(bool enabled) {
    initialize();
    m_recordingInputEnabled = enabled && m_pendingRecordingFailure.isEmpty();
    if (enabled) {
        m_bufferPostDrainPackets = false;
        if (!m_postDrainBatch.isEmpty()) {
            m_recordingBatch.reserve(m_recordingBatch.size() + m_postDrainBatch.size());
            for (CapturedPacket &packet : m_postDrainBatch)
                m_recordingBatch.append(std::move(packet));
            m_recordingBatchBytes += m_postDrainBatchBytes;
            m_postDrainBatch.clear();
            m_postDrainBatchBytes = 0;
            flushRecordingBatch();
        }
    } else {
        m_bufferPostDrainPackets = false;
        m_postDrainBatch.clear();
        m_postDrainBatchBytes = 0;
        flushRecordingBatch();
    }
}

void NetworkRuntimeWorker::drainRecordingInput(quint64 token, bool preserveIncoming) {
    initialize();
    m_recordingInputEnabled = false;
    m_bufferPostDrainPackets = preserveIncoming;
    m_postDrainBatch.clear();
    m_postDrainBatchBytes = 0;
    m_pendingDrainToken = token;
    m_pendingDrainBoundaryNanoseconds = monotonicNowNanoseconds();
    flushRecordingBatch();
    reportDrainIfComplete();
}

void NetworkRuntimeWorker::acknowledgeRecordingBatch(quint64 batchId) {
    if (batchId == 0 || !m_outstandingBatchIds.remove(batchId))
        return;
    flushRecordingBatch();
    reportDrainIfComplete();
}

void NetworkRuntimeWorker::shutdown() {
    if (!m_capture)
        return;
    m_recordingInputEnabled = false;
    m_bufferPostDrainPackets = false;
    m_recordingBatch.clear();
    m_recordingBatchBytes = 0;
    m_postDrainBatch.clear();
    m_postDrainBatchBytes = 0;
    m_outstandingBatchIds.clear();
    m_pendingDrainToken = 0;
    m_pendingDrainBoundaryNanoseconds = 0;
    m_pendingRecordingFailure.clear();
    m_batchTimer->stop();
    m_capture->stop();
    m_metrics->shutdown();
}

void NetworkRuntimeWorker::queueRecordingPacket(const CapturedPacket &packet) {
    if (!m_recordingInputEnabled) {
        if (!m_bufferPostDrainPackets)
            return;
        m_postDrainBatch.append(packet);
        m_postDrainBatchBytes += packet.data.size();
        if (m_postDrainBatchBytes > MaximumBufferedRecordingBytes) {
            m_bufferPostDrainPackets = false;
            m_pendingRecordingFailure =
                QStringLiteral("Recording snapshot barrier exceeded its bounded post-snapshot "
                               "buffer; recording was paused");
        }
        return;
    }
    m_recordingBatch.append(packet);
    m_recordingBatchBytes += packet.data.size();

    if (m_recordingBatch.size() >= MaximumPacketsPerBatch ||
        m_recordingBatchBytes >= MaximumBytesPerBatch) {
        flushRecordingBatch();
    }

    if (m_recordingBatchBytes > MaximumBufferedRecordingBytes) {
        m_recordingInputEnabled = false;
        m_pendingRecordingFailure =
            QStringLiteral("Recording pipeline cannot keep up; capture recording was paused before "
                           "unbounded memory growth");
        flushRecordingBatch();
    }
}

void NetworkRuntimeWorker::flushRecordingBatch() {
    if (m_recordingBatch.isEmpty() || m_outstandingBatchIds.size() >= MaximumOutstandingBatches) {
        reportDrainIfComplete();
        return;
    }

    QVector<CapturedPacket> packets;
    packets.reserve(qMin(qsizetype(MaximumPacketsPerBatch), m_recordingBatch.size()));
    qsizetype batchBytes = 0;
    qsizetype count = 0;
    while (count < m_recordingBatch.size() && count < MaximumPacketsPerBatch) {
        const qsizetype nextSize = m_recordingBatch.at(count).data.size();
        if (count > 0 && batchBytes + nextSize > MaximumBytesPerBatch) {
            break;
        }
        batchBytes += nextSize;
        ++count;
    }
    for (qsizetype index = 0; index < count; ++index)
        packets.append(std::move(m_recordingBatch[index]));
    m_recordingBatch.erase(m_recordingBatch.begin(), m_recordingBatch.begin() + count);
    m_recordingBatchBytes -= batchBytes;
    const quint64 batchId = ++m_nextBatchId;
    m_outstandingBatchIds.insert(batchId);
    emit recordingBatchReady(batchId, packets);
}

void NetworkRuntimeWorker::reportDrainIfComplete() {
    if (!m_recordingBatch.isEmpty() || !m_outstandingBatchIds.isEmpty()) {
        return;
    }
    if (!m_pendingRecordingFailure.isEmpty()) {
        const QString error = m_pendingRecordingFailure;
        m_pendingRecordingFailure.clear();
        emit recordingInputFailed(error);
    }

    if (m_pendingDrainToken != 0) {
        const quint64 token = m_pendingDrainToken;
        const qint64 boundaryNanoseconds = m_pendingDrainBoundaryNanoseconds;
        m_pendingDrainToken = 0;
        m_pendingDrainBoundaryNanoseconds = 0;
        emit recordingInputDrained(token, boundaryNanoseconds);
    }
}
