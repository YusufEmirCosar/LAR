#pragma once

#include "application/ports/datagram_source.h"

/** Deterministic transport substitute with configurable bind/policy outcomes. */
class FakeDatagramSource final : public IDatagramSource {
  public:
    /**
     * @brief Constructs a FakeDatagramSource.
     *
     * @details The operation follows the contract of the owning type and preserves its documented
     * invariants.
     *
     * @param[in] parent Optional Qt parent that owns the created object.
     */
    explicit FakeDatagramSource(QObject *parent = nullptr) : IDatagramSource(parent) {}

    /**
     * @brief Starts start.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] requestedPort UDP port requested by the caller.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
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

    /**
     * @brief Ends or resets stop.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     */
    void stop() override {
        listening = false;
        port = 0;
    }

    /**
     * @brief Reports whether is Listening is true.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return True when the reported condition holds; false otherwise.
     */
    bool isListening() const override {
        return listening;
    }
    /**
     * @brief Performs the local Port operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    quint16 localPort() const override {
        return port;
    }

    /**
     * @brief Updates set Ip Access Policy.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] policy Policy supplied to the operation.
     *
     * @return True when the operation succeeds; false otherwise.
     */
    bool setIpAccessPolicy(const IpAccessPolicy &policy) override {
        Q_UNUSED(policy);
        return acceptPolicyChanges;
    }

    bool startSucceeds = false;
    bool acceptPolicyChanges = false;
    bool listening = false;
    quint16 port = 0;
};
