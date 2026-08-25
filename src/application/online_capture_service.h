#pragma once

/**
 * @file online_capture_service.h
 * @brief UDP decoding service with rate-limited state publication.
 */

#include "application/captured_packet.h"
#include "application/metrics_service.h"
#include "application/ports/datagram_source.h"
#include "application/ports/packet_decoder.h"
#include "domain/decoded_state.h"
#include "domain/packet_mapping.h"

#include <QBitArray>
#include <QObject>
#include <QString>
#include <QTimer>

/**
 * @brief Converts incoming datagrams into validated states for the UI.
 *
 * Every packet is available to recording, while visual state publication is
 * coalesced by a timer to avoid flooding the main thread.
 */
class OnlineCaptureService final : public QObject {
    Q_OBJECT

  public:
    OnlineCaptureService(IDatagramSource &source, IPacketDecoder &decoder, MetricsService &metrics,
                         QObject *parent = nullptr);

    bool start(quint16 port, QString *error = nullptr);
    void stop();
    bool isListening() const;
    quint16 port() const {
        return m_source.localPort();
    }

    bool setIpAccessPolicy(const IpAccessPolicy &policy) {
        return m_source.setIpAccessPolicy(policy);
    }

    void setMapping(PacketMapping mapping);
    const PacketMapping &mapping() const {
        return m_mapping;
    }

  signals:
    void stateReceived(const DecodedState &state);
    void rawPacketReceived(const CapturedPacket &packet);
    void captureError(const QString &error);

  private slots:
    void onDatagramReceived(const QByteArray &data, qint64 receivedAtNanoseconds);
    void publishLatestState();

  private:
    IDatagramSource &m_source;
    IPacketDecoder &m_decoder;
    MetricsService &m_metrics;
    PacketMapping m_mapping;
    QTimer *m_publishTimer = nullptr;
    DecodedState m_latestState;
    bool m_hasPendingState = false;
};
