#pragma once

#include <QtTypes>

/**
 * @file recording_clock.h
 * @brief Monotonic timestamp source used by RecordingService.
 */

/** @brief Process-wide monotonic time abstraction for deterministic recording tests. */
class IRecordingClock {
  public:
    virtual ~IRecordingClock() = default;

    virtual qint64 nowNanoseconds() const = 0;
};
