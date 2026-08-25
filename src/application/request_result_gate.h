#pragma once

/**
 * @file request_result_gate.h
 * @brief Latest-request correlation guard for synchronous and queued results.
 */

#include "application/ports/runtime_messages.h"

#include <algorithm>
#include <optional>
#include <vector>

/**
 * Correlates one latest-wins command with either synchronous or asynchronous
 * delivery of its typed result. Invalid, stale, and duplicate results are
 * discarded before application state can observe them.
 */
template <typename Result> class RequestResultGate final {
  public:
    /**
     * @brief Opens a dispatch window in which a direct runtime may reply synchronously.
     */
    void beginDispatch() {
        m_dispatching = true;
        m_pending = {};
        m_synchronous.clear();
    }

    [[nodiscard]] std::optional<Result> finishDispatch(const CommandDispatch &dispatch) {
        m_dispatching = false;
        if (!dispatch.accepted || !dispatch.request.isValid()) {
            m_synchronous.clear();
            m_pending = {};
            return std::nullopt;
        }

        m_pending = dispatch.request;
        const auto match = std::find_if(
            m_synchronous.begin(), m_synchronous.end(),
            [this](const Result &candidate) { return candidate.request == m_pending; });
        if (match == m_synchronous.end()) {
            m_synchronous.clear();
            return std::nullopt;
        }

        std::optional<Result> result = std::move(*match);
        m_synchronous.clear();
        m_pending = {};
        return result;
    }

    [[nodiscard]] std::optional<Result> receive(const Result &result) {
        if (!result.request.isValid())
            return std::nullopt;
        if (m_dispatching) {
            m_synchronous.push_back(result);
            return std::nullopt;
        }
        if (!m_pending.isValid() || result.request != m_pending) {
            return std::nullopt;
        }
        m_pending = {};
        return result;
    }

    /**
     * @brief Invalidates the pending request and discards buffered completions.
     */
    void clear() noexcept {
        m_dispatching = false;
        m_pending = {};
        m_synchronous.clear();
    }

    [[nodiscard]] RuntimeRequestId pendingRequest() const noexcept {
        return m_pending;
    }

  private:
    RuntimeRequestId m_pending;
    std::vector<Result> m_synchronous;
    bool m_dispatching = false;
};
