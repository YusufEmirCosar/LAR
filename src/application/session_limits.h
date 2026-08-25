#pragma once

/**
 * @file session_limits.h
 * @brief Shared resource and duration bounds for the LAR1 session format.
 */

#include <QtGlobal>

namespace lar::session {

inline constexpr qint64 HeaderSize = 8;
inline constexpr quint32 MaximumMappingSize = 16U * 1024U * 1024U;
inline constexpr quint32 MaximumPacketSize = 16U * 1024U * 1024U;

// LAR1 keeps an unsigned 64-bit field on disk, but the supported application
// domain is intentionally smaller so clocks, formatting, and UI ratios remain
// exact and bounded on every supported platform.
inline constexpr quint64 MaximumDurationMilliseconds = 365ULL * 24ULL * 60ULL * 60ULL * 1000ULL;
inline constexpr double MaximumDurationSeconds =
    static_cast<double>(MaximumDurationMilliseconds) / 1000.0;

} // namespace lar::session
