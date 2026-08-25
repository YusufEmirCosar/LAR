#pragma once

/**
 * @file dlz_range_scale.h
 * @brief Range-axis mapping and hysteretic scale-selection policy.
 */

#include <array>

namespace dlz::presentation {

double rangeToY(double rangeNm, double scaleMinimumNm, double scaleMaximumNm, double yTop,
                double yBottom) noexcept;

const std::array<double, 4> &scaleSteps() noexcept;

int initialScaleIndex(double decisionRangeNm) noexcept;

/** @brief Stateful directional-crossing scale selector. */
class RangeScaleController final {
  public:
    void reset(double decisionRangeNm, double forcedScaleMaximumNm = 0.0) noexcept;

    double update(double decisionRangeNm) noexcept;

    double scaleMaximumNm() const noexcept;
    bool initialized() const noexcept {
        return m_initialized;
    }

  private:
    int m_index = 2;
    double m_previousDecisionNm = 0.0;
    bool m_initialized = false;
};

} // namespace dlz::presentation
