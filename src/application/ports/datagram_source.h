#pragma once

/**
 * @file datagram_source.h
 * @brief Abstract datagram transport used by online capture.
 */

#include <QByteArray>
#include <QObject>
#include <QString>

#include "application/ip_access_policy.h"

/** @brief Emits complete datagrams without exposing a concrete socket API. */
class IDatagramSource : public QObject {
    Q_OBJECT

  public:
    explicit IDatagramSource(QObject *parent = nullptr) : QObject(parent) {}
    ~IDatagramSource() override = default;

    virtual bool start(quint16 port, QString *error = nullptr) = 0;
    /**
     * @brief Closes the transport; repeated calls are safe.
     */
    virtual void stop() = 0;
    /** True while the adapter owns a bound transport. */
    virtual bool isListening() const = 0;
    virtual quint16 localPort() const = 0;
    virtual bool setIpAccessPolicy(const IpAccessPolicy &policy) = 0;

  signals:
    /**
     * @brief Notifies observers that a datagram was received.
     *
     * @param[in] data Raw UDP datagram bytes.
     * @param[in] receivedAtNanoseconds Monotonic receipt timestamp in nanoseconds.
     */
    void datagramReceived(const QByteArray &data, qint64 receivedAtNanoseconds);
    void transportError(const QString &error);
};
