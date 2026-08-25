#pragma once

/**
 * @file playback_timeline_mapper.h
 * @brief Overflow-safe conversion between exact timestamps and slider positions.
 */

#include "application/session_timestamp.h"

/** Pure overflow-safe mapping between exact time and an integer slider. */
class PlaybackTimelineMapper final {
  public:
    [[nodiscard]] static SessionTimestamp fromSlider(SessionTimestamp duration, int sliderValue,
                                                     int sliderMaximum) noexcept;

    [[nodiscard]] static int toSlider(SessionTimestamp position, SessionTimestamp duration,
                                      int sliderMaximum) noexcept;
};
