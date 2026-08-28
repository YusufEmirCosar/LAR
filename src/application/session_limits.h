#pragma once

/**
 * @file session_limits.h
 * @brief Shared resource and duration bounds for the LAR1 session format.
 */

#include <QtGlobal>

namespace lar::session {

/**
 * @namespace lar::session
 * @brief On-disk layout and accepted resource bounds for a LAR1 session.
 *
 * All integer fields use QDataStream's Qt 6.5 little-endian encoding. Bytes
 * 0..3 contain `LAR1`; bytes 4..7 contain the unsigned 32-bit offset of the
 * first packet record. Canonical mapping JSON occupies bytes 8 up to that
 * offset. Each remaining record is an unsigned 64-bit relative timestamp in
 * milliseconds, an unsigned 32-bit payload size, then that many payload bytes.
 * Records are monotonic and continue to end-of-file; the format has no stored
 * record-count field.
 */

/** Fixed bytes occupied by the magic and first-record offset. */
inline constexpr qint64 HeaderSize = 8;
/** Maximum accepted canonical mapping JSON size, in bytes. */
inline constexpr quint32 MaximumMappingSize = 16U * 1024U * 1024U;
/** Maximum accepted payload size for one packet record, in bytes. */
inline constexpr quint32 MaximumPacketSize = 16U * 1024U * 1024U;

/**
 * Largest file-backed session copied into an immutable in-memory snapshot.
 *
 * Larger sessions remain stream-backed, so accepting a valid recording never
 * requires allocating memory proportional to an arbitrarily large file.
 */
inline constexpr qint64 MaximumResidentSourceBytes = 512LL * 1024LL * 1024LL;
/** Maximum memory committed to the constant-time per-record playback index. */
inline constexpr qint64 MaximumResidentRecordIndexBytes = 128LL * 1024LL * 1024LL;

// LAR1 keeps an unsigned 64-bit field on disk, but the supported application
// domain is intentionally smaller so clocks, formatting, and UI ratios remain
// exact and bounded on every supported platform.
inline constexpr quint64 MaximumDurationMilliseconds = 365ULL * 24ULL * 60ULL * 60ULL * 1000ULL;
/** Floating-point equivalent of `MaximumDurationMilliseconds`. */
inline constexpr double MaximumDurationSeconds =
    static_cast<double>(MaximumDurationMilliseconds) / 1000.0;

} // namespace lar::session
