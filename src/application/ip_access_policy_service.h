#pragma once

/**
 * @file ip_access_policy_service.h
 * @brief Application use case for loading sender access policies through a port.
 */

#include "application/ports/ip_access_policy_repository.h"

/** Application use case for obtaining a validated IP access policy. */
class IpAccessPolicyService final {
  public:
    explicit IpAccessPolicyService(const IIpAccessPolicyRepository &repository)
        : m_repository(repository) {}

    bool load(const QString &path, IpAccessPolicy *policy, QString *error = nullptr) const;

  private:
    const IIpAccessPolicyRepository &m_repository;
};
