
#include "application/online_capture_service.h"

#include <utility>

OnlineCaptureService::OnlineCaptureService(IDatagramSource &source, IPacketDecoder &decoder,
                                           MetricsService &metrics, QObject *parent)
    : QObject(parent), m_source(source), m_decoder(decoder), m_metrics(metrics) {
    connect(&m_source, &IDatagramSource::datagramReceived, this,
            &OnlineCaptureService::onDatagramReceived);
    connect(&m_source, &IDatagramSource::transportError, this, &OnlineCaptureService::captureError);
    m_publishTimer = new QTimer(this);
    m_publishTimer->setInterval(16);
    m_publishTimer->setTimerType(Qt::PreciseTimer);
    connect(m_publishTimer, &QTimer::timeout, this, &OnlineCaptureService::publishLatestState);
}

bool OnlineCaptureService::start(quint16 port, QString *error) {
    if (!m_mapping.isValid()) {
        if (error)
            *error = QStringLiteral("Online capture requires a valid packet mapping");
        return false;
    }
    const bool started = m_source.start(port, error);
    if (started) {
        m_hasPendingState = false;
        m_publishTimer->start();
    }
    return started;
}

void OnlineCaptureService::stop() {
    m_source.stop();
    m_publishTimer->stop();
    m_hasPendingState = false;
}

bool OnlineCaptureService::isListening() const {
    return m_source.isListening();
}

void OnlineCaptureService::setMapping(PacketMapping mapping) {
    m_mapping = mapping;
    m_decoder.setMapping(m_mapping);
}

void OnlineCaptureService::onDatagramReceived(const QByteArray &data,
                                              qint64 receivedAtNanoseconds) {
    m_metrics.recordDatagramAttempted();

    DecodedState state;
    QString error;
    if (m_decoder.decode(data, &state, &error)) {
        m_metrics.recordPacketProcessed();
        emit rawPacketReceived(CapturedPacket{data, receivedAtNanoseconds});
        m_latestState = std::move(state);
        m_hasPendingState = true;
    } else {
        emit captureError(error);
    }
}

void OnlineCaptureService::publishLatestState() {
    if (!m_hasPendingState)
        return;
    m_hasPendingState = false;
    emit stateReceived(m_latestState);
}
