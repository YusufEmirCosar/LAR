#pragma once

/**
 * @file playbacktimeformatter.h
 * @brief Presentation-only formatting of playback time and slider values.
 */

#include "application/session_timestamp.h"

#include <QString>

/** @brief Formats exact session timestamps for presentation. */
class PlaybackTimeFormatter {
  public:
    static QString format(SessionTimestamp timestamp);
};
