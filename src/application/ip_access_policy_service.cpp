
#include "application/ip_access_policy_service.h"

#include <utility>

bool IpAccessPolicyService::load(const QString &path, IpAccessPolicy *policy,
                                 QString *error) const {
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("IP whitelist path is empty");
        return false;
    }
    if (!policy) {
        if (error)
            *error = QStringLiteral("IP policy output is null");
        return false;
    }

    IpAccessPolicy candidate;
    if (!m_repository.load(path, &candidate, error))
        return false;
    *policy = std::move(candidate);
    return true;
}
