#pragma once

/**
 * @file qt_recording_clock.h
 * @brief QElapsedTimer implementation of the recording clock port.
 */

#include "application/ports/recording_clock.h"
#include <QtTypes>

#include <chrono>

/** @brief Process-wide steady-clock implementation used by receipt timestamps. */
class QtRecordingClock final : public IRecordingClock {
  public:
    qint64 nowNanoseconds() const override {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    }
};
