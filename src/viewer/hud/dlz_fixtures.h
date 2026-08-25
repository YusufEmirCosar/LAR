#pragma once

/**
 * @file dlz_fixtures.h
 * @brief Named deterministic DLZ scenarios exposed by the HUD controls.
 */

#include "domain/dlz/dlz_types.h"

namespace dlz::presentation {

/** @brief Immutable direct-render fixtures that intentionally bypass solving. */
struct RenderFixture final {
    dlz::Solution solution;
    dlz::HudState hudState;
    double displayedRangeNm = 0.0;
};

RenderFixture fixtureA() noexcept;

RenderFixture fixtureB() noexcept;

} // namespace dlz::presentation
