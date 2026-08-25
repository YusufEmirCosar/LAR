
#include "viewer/hud/dlz_fixtures.h"

namespace dlz::presentation {
namespace {

RenderFixture makeFixture(double aero, double intercept, double noEscape, double turnAndRun,
                          double minimum, double tof, bool inEnvelope, bool shootCue,
                          double currentRange, double rangeRate, double scaleMaximum) noexcept {
    RenderFixture fixture;
    fixture.solution = {aero, intercept, noEscape, turnAndRun, minimum, tof, inEnvelope, shootCue};
    fixture.hudState = {currentRange,
                        rangeRate,
                        scaleMaximum,
                        0.0,
                        currentRange > scaleMaximum,
                        dlz::HudMode::Prelaunch,
                        0.0,
                        false};
    fixture.displayedRangeNm = currentRange;
    return fixture;
}

} // namespace

RenderFixture fixtureA() noexcept {
    return makeFixture(32.0, 26.5, 14.0, 8.5, 1.8, 48.0, true, true, 21.3, -1100.0, 40.0);
}

RenderFixture fixtureB() noexcept {
    return makeFixture(11.5, 9.0, 3.2, 2.6, 1.8, 22.0, false, false, 13.0, -120.0, 20.0);
}

} // namespace dlz::presentation
