#pragma once

/**
 * @file ip_access_policy_repository.h
 * @brief Storage-independent input port for validated IP access policies.
 */

#include "application/ip_access_policy.h"

#include <QString>

/** Loads and validates exact-match sender policies from external storage. */
class IIpAccessPolicyRepository {
  public:
    virtual ~IIpAccessPolicyRepository() = default;

    virtual bool load(const QString &path, IpAccessPolicy *policy,
                      QString *error = nullptr) const = 0;
};
