
#include "viewer/playback_timeline_mapper.h"

#include <algorithm>
#include <cmath>

SessionTimestamp PlaybackTimelineMapper::fromSlider(SessionTimestamp duration, int sliderValue,
                                                    int sliderMaximum) noexcept {
    if (duration == SessionTimestamp::zero() || sliderMaximum <= 0) {
        return {};
    }
    const int boundedValue = std::clamp(sliderValue, 0, sliderMaximum);
    const long double milliseconds = static_cast<long double>(duration.milliseconds()) *
                                     static_cast<long double>(boundedValue) /
                                     static_cast<long double>(sliderMaximum);
    return SessionTimestamp::clampedMilliseconds(static_cast<qint64>(std::round(milliseconds)));
}

int PlaybackTimelineMapper::toSlider(SessionTimestamp position, SessionTimestamp duration,
                                     int sliderMaximum) noexcept {
    if (duration == SessionTimestamp::zero() || sliderMaximum <= 0)
        return 0;
    const SessionTimestamp boundedPosition = std::min(position, duration);
    const long double sliderValue = static_cast<long double>(boundedPosition.milliseconds()) *
                                    static_cast<long double>(sliderMaximum) /
                                    static_cast<long double>(duration.milliseconds());
    return std::clamp(static_cast<int>(std::round(sliderValue)), 0, sliderMaximum);
}
