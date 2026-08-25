
#include "infrastructure/network/qt_ip_address_normalizer.h"

QString canonicalIpAddress(const QHostAddress &address) {
    if (address.isNull() || !address.scopeId().isEmpty())
        return {};

    bool isIpv4 = false;
    const quint32 ipv4 = address.toIPv4Address(&isIpv4);
    if (isIpv4)
        return QHostAddress(ipv4).toString();
    if (address.protocol() != QAbstractSocket::IPv6Protocol)
        return {};
    return address.toString().toLower();
}

QString canonicalIpAddress(const QString &address) {
    const QString trimmed = address.trimmed();
    if (trimmed.isEmpty())
        return {};
    QHostAddress parsed;
    if (!parsed.setAddress(trimmed))
        return {};
    return canonicalIpAddress(parsed);
}
