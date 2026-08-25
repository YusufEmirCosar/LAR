#pragma once

/**
 * @file qt_ip_access_policy_repository.h
 * @brief Qt file-backed adapter for the application IP-policy repository port.
 */

#include "application/ports/ip_access_policy_repository.h"

#include <QByteArray>

/** Bounded Qt filesystem/parser adapter for exact IP whitelist files. */
class QtIpAccessPolicyRepository final : public IIpAccessPolicyRepository {
  public:
    static constexpr qint64 MaximumFileBytes = 1024 * 1024;
    static constexpr qsizetype MaximumLineBytes = 256;
    static constexpr qsizetype MaximumLineCount = 4096;
    static constexpr qsizetype MaximumAddresses = 4096;

    bool load(const QString &path, IpAccessPolicy *policy, QString *error = nullptr) const override;

    static bool parseText(const QByteArray &text, IpAccessPolicy *policy, QString *error = nullptr);
};
