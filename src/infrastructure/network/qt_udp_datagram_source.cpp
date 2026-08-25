
#include "infrastructure/network/qt_udp_datagram_source.h"

#include "infrastructure/network/qt_ip_address_normalizer.h"

#include <QElapsedTimer>
#include <QNetworkDatagram>
#include <QTimer>

#include <chrono>

namespace {
qint64 monotonicNowNanoseconds() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}
} // namespace

QtUdpDatagramSource::QtUdpDatagramSource(QObject *parent) : IDatagramSource(parent) {}

QtUdpDatagramSource::~QtUdpDatagramSource() {
    stop();
}

bool QtUdpDatagramSource::start(quint16 port, QString *error) {
    stop();

    QString lastError;
    auto tryBind = [this, port, &lastError](const QHostAddress &addr,
                                            QUdpSocket::BindMode mode) -> bool {
        auto *candidate = new QUdpSocket(this);
        if (!candidate->bind(addr, port, mode)) {
            lastError = candidate->errorString();
            delete candidate;
            return false;
        }
        m_socket = candidate;
        connect(m_socket, &QUdpSocket::readyRead, this, &QtUdpDatagramSource::readPendingDatagrams);
        connect(m_socket, &QUdpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            if (m_socket)
                emit transportError(m_socket->errorString());
        });
        return true;
    };

    if (tryBind(QHostAddress::Any, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        return true;
    if (tryBind(QHostAddress::AnyIPv4, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        return true;
    if (tryBind(QHostAddress::AnyIPv4, QUdpSocket::DefaultForPlatform))
        return true;
    if (tryBind(QHostAddress(QStringLiteral("127.0.0.1")), QUdpSocket::DefaultForPlatform))
        return true;

    if (error) {
        *error = lastError.isEmpty() ? QStringLiteral("Cannot bind UDP socket") : lastError;
    }
    stop();
    return false;
}

void QtUdpDatagramSource::stop() {
    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }
    m_drainScheduled = false;
}

bool QtUdpDatagramSource::isListening() const {
    return m_socket && m_socket->state() == QAbstractSocket::BoundState;
}

void QtUdpDatagramSource::readPendingDatagrams() {
    if (!m_socket)
        return;
    m_drainScheduled = false;
    QElapsedTimer budget;
    budget.start();
    int processed = 0;
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();
        const qint64 receivedAtNanoseconds = monotonicNowNanoseconds();
        if (!m_ipPolicy.accepts(canonicalIpAddress(datagram.senderAddress()))) {
            ++processed;
            if (processed >= 1024 || budget.nsecsElapsed() >= 2000000)
                break;
            continue;
        }
        emit datagramReceived(datagram.data(), receivedAtNanoseconds);
        ++processed;
        if (processed >= 1024 || budget.nsecsElapsed() >= 2000000)
            break;
    }
    if (m_socket && m_socket->hasPendingDatagrams() && !m_drainScheduled) {
        m_drainScheduled = true;
        QTimer::singleShot(0, this, &QtUdpDatagramSource::readPendingDatagrams);
    }
}
