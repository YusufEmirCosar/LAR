#pragma once

#include "application/ports/datagram_source.h"

/** Deterministic transport substitute with configurable bind/policy outcomes. */
class FakeDatagramSource final : public IDatagramSource {
  public:
    explicit FakeDatagramSource(QObject *parent = nullptr) : IDatagramSource(parent) {}

    bool start(quint16 requestedPort, QString *error) override {
        Q_UNUSED(error);
        if (!startSucceeds) {
            if (error)
                *error = QStringLiteral("simulated bind failure");
            return false;
        }
        listening = true;
        port = requestedPort;
        return true;
    }

    void stop() override {
        listening = false;
        port = 0;
    }

    bool isListening() const override {
        return listening;
    }
    quint16 localPort() const override {
        return port;
    }

    bool setIpAccessPolicy(const IpAccessPolicy &policy) override {
        Q_UNUSED(policy);
        return acceptPolicyChanges;
    }

    bool startSucceeds = false;
    bool acceptPolicyChanges = false;
    bool listening = false;
    quint16 port = 0;
};
