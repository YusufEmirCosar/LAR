#pragma once

/**
 * @file session_timestamp.h
 * @brief Exact validated session-relative timestamp value object.
 */

#include "application/session_limits.h"

#include <QMetaType>

#include <cmath>
#include <optional>

/**
 * Exact, bounded, non-negative session-relative time in milliseconds.
 *
 * LAR1 stores this value as an unsigned 64-bit integer, while the supported
 * application domain is deliberately capped at 365 days. Construction is
 * checked before an instance can contain a non-zero value.
 */
class SessionTimestamp final {
  public:
    /**
     * @brief Constructs the zero timestamp.
     */
    constexpr SessionTimestamp() noexcept = default;

    [[nodiscard]] static constexpr SessionTimestamp zero() noexcept {
        return SessionTimestamp(0);
    }

    [[nodiscard]] static constexpr SessionTimestamp maximum() noexcept {
        return SessionTimestamp(static_cast<qint64>(lar::session::MaximumDurationMilliseconds));
    }

    /**
     * @brief Creates a timestamp from signed milliseconds when it is in range.
     *
     * @param[in] milliseconds Non-negative session-relative duration in integer milliseconds.
     *
     * @return The value when the inputs are valid; std::nullopt otherwise.
     */
    [[nodiscard]] static constexpr std::optional<SessionTimestamp>
    fromMilliseconds(qint64 milliseconds) noexcept {
        if (milliseconds < 0 ||
            static_cast<quint64>(milliseconds) > lar::session::MaximumDurationMilliseconds) {
            return std::nullopt;
        }
        return SessionTimestamp(milliseconds);
    }

    /**
     * @brief Creates a timestamp from the unsigned representation stored by LAR1.
     *
     * @param[in] milliseconds Non-negative LAR1-stored duration in integer milliseconds.
     *
     * @return The value when the inputs are valid; std::nullopt otherwise.
     */
    [[nodiscard]] static constexpr std::optional<SessionTimestamp>
    fromStoredMilliseconds(quint64 milliseconds) noexcept {
        if (milliseconds > lar::session::MaximumDurationMilliseconds) {
            return std::nullopt;
        }
        return SessionTimestamp(static_cast<qint64>(milliseconds));
    }

    /**
     * @brief Converts finite, non-negative seconds using nearest-millisecond rounding.
     *
     * @param[in] seconds Time value expressed in seconds.
     *
     * @return The value when the inputs are valid; std::nullopt otherwise.
     */
    [[nodiscard]] static std::optional<SessionTimestamp> fromSeconds(double seconds) noexcept {
        if (!std::isfinite(seconds) || seconds < 0.0 ||
            seconds > lar::session::MaximumDurationSeconds) {
            return std::nullopt;
        }
        const long double milliseconds = static_cast<long double>(seconds) * 1000.0L;
        const long double rounded = std::round(milliseconds);
        if (rounded < 0.0L ||
            rounded > static_cast<long double>(lar::session::MaximumDurationMilliseconds)) {
            return std::nullopt;
        }
        return SessionTimestamp(static_cast<qint64>(rounded));
    }

    /**
     * @brief Converts monotonic nanoseconds using nearest-millisecond rounding.
     *
     * @param[in] nanoseconds Non-negative monotonic duration in nanoseconds.
     *
     * @return The value when the inputs are valid; std::nullopt otherwise.
     */
    [[nodiscard]] static constexpr std::optional<SessionTimestamp>
    fromNanoseconds(qint64 nanoseconds) noexcept {
        constexpr qint64 NanosecondsPerMillisecond = 1'000'000;
        constexpr qint64 MaximumNanoseconds =
            static_cast<qint64>(lar::session::MaximumDurationMilliseconds) *
            NanosecondsPerMillisecond;
        if (nanoseconds < 0 || nanoseconds > MaximumNanoseconds) {
            return std::nullopt;
        }
        return SessionTimestamp((nanoseconds + NanosecondsPerMillisecond / 2) /
                                NanosecondsPerMillisecond);
    }

    [[nodiscard]] static constexpr SessionTimestamp
    clampedMilliseconds(qint64 milliseconds) noexcept {
        if (milliseconds <= 0)
            return zero();
        if (static_cast<quint64>(milliseconds) >= lar::session::MaximumDurationMilliseconds) {
            return maximum();
        }
        return SessionTimestamp(milliseconds);
    }

    [[nodiscard]] constexpr qint64 milliseconds() const noexcept {
        return m_milliseconds;
    }

    [[nodiscard]] double seconds() const noexcept {
        return static_cast<double>(m_milliseconds) / 1000.0;
    }

    friend constexpr bool operator==(SessionTimestamp left, SessionTimestamp right) noexcept {
        return left.m_milliseconds == right.m_milliseconds;
    }
    friend constexpr bool operator!=(SessionTimestamp left, SessionTimestamp right) noexcept {
        return !(left == right);
    }
    /** Orders timestamps by their exact millisecond value. */
    friend constexpr bool operator<(SessionTimestamp left, SessionTimestamp right) noexcept {
        return left.m_milliseconds < right.m_milliseconds;
    }
    friend constexpr bool operator<=(SessionTimestamp left, SessionTimestamp right) noexcept {
        return !(right < left);
    }
    friend constexpr bool operator>(SessionTimestamp left, SessionTimestamp right) noexcept {
        return right < left;
    }
    friend constexpr bool operator>=(SessionTimestamp left, SessionTimestamp right) noexcept {
        return !(left < right);
    }

  private:
    explicit constexpr SessionTimestamp(qint64 milliseconds) noexcept
        : m_milliseconds(milliseconds) {}

    qint64 m_milliseconds = 0;
};

Q_DECLARE_METATYPE(SessionTimestamp)
