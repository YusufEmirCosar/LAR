#pragma once

/**
 * @file qt_udp_datagram_source.h
 * @brief QUdpSocket implementation of the datagram-source port.
 */

#include "application/ports/datagram_source.h"
#include <QUdpSocket>

/** @brief Binds a UDP port and drains all pending datagrams asynchronously. */
class QtUdpDatagramSource final : public IDatagramSource {
    Q_OBJECT

  public:
    explicit QtUdpDatagramSource(QObject *parent = nullptr);
    ~QtUdpDatagramSource() override;

    bool start(quint16 port, QString *error = nullptr) override;
    void stop() override;
    bool isListening() const override;
    quint16 localPort() const override {
        return m_socket ? m_socket->localPort() : 0;
    }
    bool setIpAccessPolicy(const IpAccessPolicy &policy) override {
        m_ipPolicy = policy;
        return true;
    }

  private slots:
    /**
     * @brief Reads pending datagrams.
     */
    void readPendingDatagrams();

  private:
    QUdpSocket *m_socket = nullptr;
    bool m_drainScheduled = false;
    IpAccessPolicy m_ipPolicy;
};
