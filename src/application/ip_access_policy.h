#pragma once

/**
 * @file ip_access_policy.h
 * @brief Bounded, exact-match UDP sender address policy.
 */

#include <QMetaType>
#include <QSet>
#include <QString>

/**
 * @brief Optional exact IP allow-list used before packet decoding.
 *
 * The policy deliberately stores canonical textual addresses rather than
 * socket objects.  This keeps the application boundary value-like and makes
 * it safe to copy across Qt queued connections.  A whitelist is exact-match:
 * CIDR ranges, host names, and wildcard expressions are not accepted.
 */
class IpAccessPolicy final {
  public:
    enum class Mode : quint8 {
        AllowAll = 0,
        WhitelistOnly = 1,
    };

    IpAccessPolicy() = default;

    IpAccessPolicy(Mode mode, QSet<QString> addresses)
        : m_mode(mode), m_addresses(std::move(addresses)) {}

    Mode mode() const noexcept {
        return m_mode;
    }
    const QSet<QString> &addresses() const noexcept {
        return m_addresses;
    }

    bool accepts(const QString &canonicalSenderAddress) const noexcept {
        return m_mode == Mode::AllowAll ||
               (!canonicalSenderAddress.isEmpty() && m_addresses.contains(canonicalSenderAddress));
    }

  private:
    Mode m_mode = Mode::AllowAll;
    QSet<QString> m_addresses;
};

Q_DECLARE_METATYPE(IpAccessPolicy)
