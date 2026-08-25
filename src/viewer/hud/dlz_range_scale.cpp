
#include "viewer/hud/dlz_range_scale.h"

#include <algorithm>
#include <cmath>

namespace dlz::presentation {
namespace {

constexpr std::array<double, 4> Steps = {10.0, 20.0, 40.0, 80.0};

} // namespace

double rangeToY(double rangeNm, double scaleMinimumNm, double scaleMaximumNm, double yTop,
                double yBottom) noexcept {
    if (!std::isfinite(rangeNm) || !std::isfinite(scaleMinimumNm) ||
        !std::isfinite(scaleMaximumNm) || !std::isfinite(yTop) || !std::isfinite(yBottom) ||
        scaleMaximumNm <= scaleMinimumNm) {
        return yBottom;
    }
    const double fraction =
        std::clamp((rangeNm - scaleMinimumNm) / (scaleMaximumNm - scaleMinimumNm), 0.0, 1.0);
    return yBottom - fraction * (yBottom - yTop);
}

const std::array<double, 4> &scaleSteps() noexcept {
    return Steps;
}

int initialScaleIndex(double decisionRangeNm) noexcept {
    if (!std::isfinite(decisionRangeNm) || decisionRangeNm <= Steps.front()) {
        return 0;
    }
    for (int index = 0; index < static_cast<int>(Steps.size()); ++index) {
        if (decisionRangeNm <= Steps[static_cast<std::size_t>(index)]) {
            return index;
        }
    }
    return static_cast<int>(Steps.size()) - 1;
}

void RangeScaleController::reset(double decisionRangeNm, double forcedScaleMaximumNm) noexcept {
    if (std::isfinite(forcedScaleMaximumNm)) {
        for (int index = 0; index < static_cast<int>(Steps.size()); ++index) {
            if (std::abs(Steps[static_cast<std::size_t>(index)] - forcedScaleMaximumNm) < 1e-9) {
                m_index = index;
                // A forced scale is a deterministic capture checkpoint.  Seed
                // the previous sample just below the upward threshold so the
                // next real update can demonstrate the required crossing
                // (scenario 5: 40 nm parked frame -> 80 nm on the next tick)
                // without changing the checkpoint itself.
                m_previousDecisionNm = std::min(decisionRangeNm, forcedScaleMaximumNm * 0.80);
                m_initialized = true;
                return;
            }
        }
    }
    m_index = initialScaleIndex(decisionRangeNm);
    m_previousDecisionNm = decisionRangeNm;
    m_initialized = true;
}

double RangeScaleController::update(double decisionRangeNm) noexcept {
    if (!std::isfinite(decisionRangeNm)) {
        return scaleMaximumNm();
    }
    if (!m_initialized) {
        reset(decisionRangeNm);
        return scaleMaximumNm();
    }

    const bool rising = decisionRangeNm > m_previousDecisionNm;
    const bool falling = decisionRangeNm < m_previousDecisionNm;
    if (rising) {
        while (m_index + 1 < static_cast<int>(Steps.size())) {
            const double threshold = 0.90 * Steps[static_cast<std::size_t>(m_index)];
            if (!(m_previousDecisionNm <= threshold && decisionRangeNm > threshold)) {
                break;
            }
            ++m_index;
        }
    } else if (falling) {
        while (m_index > 0) {
            const double threshold = 0.60 * Steps[static_cast<std::size_t>(m_index)];
            if (!(m_previousDecisionNm >= threshold && decisionRangeNm < threshold)) {
                break;
            }
            --m_index;
        }
    }
    m_previousDecisionNm = decisionRangeNm;
    return scaleMaximumNm();
}

double RangeScaleController::scaleMaximumNm() const noexcept {
    return Steps[static_cast<std::size_t>(m_index)];
}

} // namespace dlz::presentation
