
#include "infrastructure/network/qt_ip_access_policy_repository.h"

#include "infrastructure/network/qt_ip_address_normalizer.h"

#include <QFile>

#include <utility>

bool QtIpAccessPolicyRepository::parseText(const QByteArray &text, IpAccessPolicy *policy,
                                           QString *error) {
    if (!policy) {
        if (error)
            *error = QStringLiteral("IP policy output is null");
        return false;
    }
    if (text.size() > MaximumFileBytes) {
        if (error)
            *error = QStringLiteral("IP whitelist exceeds the 1 MiB limit");
        return false;
    }

    QSet<QString> addresses;
    const QList<QByteArray> lines = text.split('\n');
    if (lines.size() > MaximumLineCount) {
        if (error) {
            *error = QStringLiteral("IP whitelist exceeds the %1-line limit").arg(MaximumLineCount);
        }
        return false;
    }
    for (qsizetype index = 0; index < lines.size(); ++index) {
        QByteArray line = lines.at(index);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.size() > MaximumLineBytes) {
            if (error) {
                *error = QStringLiteral("IP whitelist line %1 exceeds the %2-byte limit")
                             .arg(index + 1)
                             .arg(MaximumLineBytes);
            }
            return false;
        }
        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;
        const QString canonical = canonicalIpAddress(QString::fromUtf8(trimmed));
        if (canonical.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Invalid IP address on whitelist line %1").arg(index + 1);
            }
            return false;
        }
        addresses.insert(canonical);
        if (addresses.size() > MaximumAddresses) {
            if (error) {
                *error = QStringLiteral("IP whitelist exceeds the %1-address limit")
                             .arg(MaximumAddresses);
            }
            return false;
        }
    }

    if (addresses.isEmpty()) {
        if (error)
            *error = QStringLiteral("IP whitelist does not contain an address");
        return false;
    }
    *policy = IpAccessPolicy(IpAccessPolicy::Mode::WhitelistOnly, std::move(addresses));
    return true;
}

bool QtIpAccessPolicyRepository::load(const QString &path, IpAccessPolicy *policy,
                                      QString *error) const {
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("IP whitelist path is empty");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open IP whitelist: %1").arg(file.errorString());
        }
        return false;
    }
    if (file.size() > MaximumFileBytes) {
        if (error)
            *error = QStringLiteral("IP whitelist exceeds the 1 MiB limit");
        return false;
    }

    const QByteArray bytes = file.read(MaximumFileBytes + 1);
    if (bytes.size() > MaximumFileBytes) {
        if (error)
            *error = QStringLiteral("IP whitelist exceeds the 1 MiB limit");
        return false;
    }
    return parseText(bytes, policy, error);
}
