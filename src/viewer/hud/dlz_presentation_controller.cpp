
#include "viewer/hud/dlz_presentation_controller.h"

#include <algorithm>
#include <cmath>

namespace dlz::presentation {
namespace {

constexpr double FilterTimeConstantSeconds = 0.25;
constexpr double ShootEntryMinimumMarginNm = 0.10;
constexpr double ShootEntryMaximumMarginNm = 0.20;
constexpr double ShootExitMinimumMarginNm = 0.10;
constexpr double ShootExitMaximumMarginNm = 0.20;
constexpr double ShootMinimumVisibleSeconds = 0.50;
constexpr double ShootFlashPeriodSeconds = 0.50;

} // namespace

void PresentationController::reset(double forcedScaleMaximumNm) noexcept {
    m_scale = {};
    m_state = {};
    m_state.scaleMinimumNm = 0.0;
    m_state.scaleMaximumNm = forcedScaleMaximumNm > 0.0 ? forcedScaleMaximumNm : 40.0;
    m_lastSolution = {};
    m_displayedRangeNm = 0.0;
    m_elapsedSeconds = 0.0;
    m_shootVisibleUntil = 0.0;
    m_forcedScaleMaximumNm = forcedScaleMaximumNm;
    m_shootActive = false;
    m_hasSolution = false;
    m_filterInitialized = false;
}

void PresentationController::clear() noexcept {
    reset();
    m_state.mode = dlz::HudMode::NoTrack;
}

void PresentationController::update(const dlz::Solution &solution, double rawRangeNm,
                                    double rawRangeRateKnots, dlz::HudMode mode,
                                    double dtSeconds) noexcept {
    if (mode != dlz::HudMode::Prelaunch) {
        clear();
        m_state.mode = mode;
        return;
    }
    if (!std::isfinite(rawRangeNm) || rawRangeNm < 0.0 || !std::isfinite(rawRangeRateKnots)) {
        clear();
        return;
    }
    const double dt = std::clamp(std::isfinite(dtSeconds) ? dtSeconds : 0.0, 0.0, 0.25);
    m_elapsedSeconds += dt;
    m_lastSolution = solution;
    m_hasSolution = true;

    const double decisionRange = std::max(rawRangeNm, solution.aerodynamicMaximumRangeNm);
    if (!m_scale.initialized()) {
        m_scale.reset(decisionRange, m_forcedScaleMaximumNm);
        m_forcedScaleMaximumNm = 0.0;
    } else {
        m_scale.update(decisionRange);
    }
    m_state.scaleMinimumNm = 0.0;
    m_state.scaleMaximumNm = m_scale.scaleMaximumNm();
    m_state.currentRangeNm = rawRangeNm;
    m_state.rangeRateKnots = rawRangeRateKnots;
    m_state.caretParked = rawRangeNm > m_state.scaleMaximumNm;
    m_state.mode = mode;
    m_state.timeToImpactRemainingSeconds = std::max(0.0, solution.timeOfFlightSeconds);

    if (!m_filterInitialized) {
        m_displayedRangeNm = rawRangeNm;
        m_filterInitialized = true;
    } else {
        const double gain = std::clamp(dt / FilterTimeConstantSeconds, 0.0, 1.0);
        m_displayedRangeNm += (rawRangeNm - m_displayedRangeNm) * gain;
    }
    updateShootPresentation(solution, rawRangeNm, dt);
}

void PresentationController::updateShootPresentation(const dlz::Solution &solution,
                                                     double rawRangeNm, double dtSeconds) noexcept {
    const bool entry = solution.shootCue &&
                       rawRangeNm > solution.minimumRangeNm + ShootEntryMinimumMarginNm &&
                       rawRangeNm < solution.interceptRangeNm - ShootEntryMaximumMarginNm;
    const bool exit = rawRangeNm <= solution.minimumRangeNm - ShootExitMinimumMarginNm ||
                      rawRangeNm > solution.interceptRangeNm + ShootExitMaximumMarginNm;

    if (!m_shootActive && entry) {
        m_shootActive = true;
        m_shootVisibleUntil = m_elapsedSeconds + ShootMinimumVisibleSeconds;
    } else if (m_shootActive && exit && m_elapsedSeconds >= m_shootVisibleUntil) {
        m_shootActive = false;
    }
    (void)dtSeconds;
    const double phase = std::fmod(std::max(0.0, m_elapsedSeconds), ShootFlashPeriodSeconds);
    m_state.shootFlash = m_shootActive && phase < ShootFlashPeriodSeconds * 0.5;
}

} // namespace dlz::presentation
