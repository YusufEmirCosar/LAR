#pragma once

/**
 * @file dlz_presentation_controller.h
 * @brief Stateful filtering and cue-timing policy for DLZ presentation.
 */

#include "domain/dlz/dlz_types.h"
#include "viewer/hud/dlz_range_scale.h"

namespace dlz::presentation {

/** @brief Owns filtering, scale selection, and shoot-cue timing. */
class PresentationController final {
  public:
    void reset(double forcedScaleMaximumNm = 0.0) noexcept;

    void update(const dlz::Solution &solution, double rawRangeNm, double rawRangeRateKnots,
                dlz::HudMode mode, double dtSeconds) noexcept;

    void clear() noexcept;

    const dlz::HudState &state() const noexcept {
        return m_state;
    }
    double displayedRangeNm() const noexcept {
        return m_displayedRangeNm;
    }
    bool hasSolution() const noexcept {
        return m_hasSolution;
    }
    bool shootCueActive() const noexcept {
        return m_shootActive;
    }

  private:
    void updateShootPresentation(const dlz::Solution &solution, double rawRangeNm,
                                 double dtSeconds) noexcept;

    RangeScaleController m_scale;
    dlz::HudState m_state;
    dlz::Solution m_lastSolution;
    double m_displayedRangeNm = 0.0;
    double m_elapsedSeconds = 0.0;
    double m_shootVisibleUntil = 0.0;
    double m_forcedScaleMaximumNm = 0.0;
    bool m_shootActive = false;
    bool m_hasSolution = false;
    bool m_filterInitialized = false;
};

} // namespace dlz::presentation
