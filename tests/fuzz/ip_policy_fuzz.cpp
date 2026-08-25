#include "infrastructure/network/qt_ip_access_policy_repository.h"

#include <QByteArray>

#include <cstddef>
#include <cstdint>
#include <limits>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    const auto maximum = static_cast<std::size_t>(QtIpAccessPolicyRepository::MaximumFileBytes + 1);
    if (size > maximum || size > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
        return 0;
    }
    const QByteArray bytes(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
    IpAccessPolicy policy;
    QString error;
    (void)QtIpAccessPolicyRepository::parseText(bytes, &policy, &error);
    return 0;
}
