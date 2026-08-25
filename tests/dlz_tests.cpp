#include "domain/dlz/dlz_scenario_adapter.h"
#include "domain/dlz/dlz_solver.h"
#include "viewer/hud/dlz_fixtures.h"
#include "viewer/hud/dlz_hud_renderer.h"
#include "viewer/hud/dlz_presentation_controller.h"
#include "viewer/hud/dlz_range_scale.h"

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QtTest>

#include <cmath>
#include <limits>
#include <optional>

namespace {

dlz::ScenarioInputs headOnInputs() {
    return {20.0, 0.0, 30000.0, 0.90, 0.95};
}
dlz::ScenarioInputs beamInputs() {
    return {20.0, 90.0, 30000.0, 0.90, 0.95};
}
dlz::ScenarioInputs tailInputs() {
    return {13.0, 180.0, 55000.0, 1.00, 0.95};
}
dlz::ScenarioInputs minimumInputs() {
    return {1.2, 0.0, 30000.0, 0.90, 0.95};
}
dlz::ScenarioInputs farInputs() {
    return {60.0, 0.0, 30000.0, 0.90, 0.95};
}
dlz::ScenarioInputs sweepInputs() {
    return {50.0, 0.0, 55000.0, 1.00, 0.95};
}

} // namespace

class DlzTests final : public QObject {
    Q_OBJECT

  private slots:
    void geometryUsesBriefAspectDefinition();
    void invalidGeometryInputsAreRejected();
    void solverUsesExactToyModel();
    void solverRejectsDerivedOverflow();
    void invalidTailDomainIsRejected();
    void maintainedScenarioFixturesExerciseSolverPath();
    void aspectSweepShrinksMonotonically();
    void scaleMappingAndDirectionalHysteresis();
    void presentationFiltersCaretAndTimesShootCue();
    void noTrackClearsPresentation();
    void rendererDrawsDirectFixture();
};

void DlzTests::geometryUsesBriefAspectDefinition() {
    const auto headOn = headOnInputs();
    const auto beam = beamInputs();
    const auto tail = tailInputs();
    dlz::ScenarioFrame frame;
    QVERIFY(dlz::ScenarioAdapter::build(headOn, &frame));
    QCOMPARE(frame.geometry.rangeNm, headOn.rangeNm);
    QVERIFY(std::abs(frame.geometry.aspectRadians) < 1e-9);
    QVERIFY(frame.geometry.rangeRateKnots < 0.0);
    QVERIFY(dlz::ScenarioAdapter::build(beam, &frame));
    QVERIFY(std::abs(frame.geometry.aspectRadians - 1.5707963267948966) < 1e-9);
    QVERIFY(dlz::ScenarioAdapter::build(tail, &frame));
    QVERIFY(std::abs(frame.geometry.aspectRadians - 3.141592653589793) < 1e-9);
}

void DlzTests::invalidGeometryInputsAreRejected() {
    dlz::ScenarioFrame frame;
    QVERIFY(dlz::ScenarioAdapter::build(headOnInputs(), &frame));
    QString error;
    frame.target.vel = {};
    QVERIFY(!dlz::calculateGeometry(frame.shooter, frame.target, frame.atmosphere, frame.weapon,
                                    &error));
    QVERIFY(error.contains(QStringLiteral("velocity"), Qt::CaseInsensitive));

    frame.target.vel = {1.0, 0.0, 0.0};
    frame.shooter.theta = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!dlz::calculateGeometry(frame.shooter, frame.target, frame.atmosphere, frame.weapon,
                                    &error));
    QVERIFY(error.contains(QStringLiteral("attitude"), Qt::CaseInsensitive));
}

void DlzTests::solverUsesExactToyModel() {
    const auto inputs = headOnInputs();
    dlz::ScenarioFrame frame;
    QVERIFY(dlz::ScenarioAdapter::build(inputs, &frame));
    const double expectedAero = 32.0 * 1.0 * 1.0 * (0.70 + 0.35 * 0.90);
    QCOMPARE(frame.solution.aerodynamicMaximumRangeNm, expectedAero);
    QCOMPARE(frame.solution.interceptRangeNm, 0.83 * expectedAero);
    QCOMPARE(frame.solution.noEscapeRangeNm, expectedAero * 0.52);
    QCOMPARE(frame.solution.turnAndRunRangeNm, expectedAero * 0.52 * 0.60);
    QCOMPARE(frame.solution.minimumRangeNm, 1.8);
    QCOMPARE(frame.solution.timeOfFlightSeconds, (20.0 / 1600.0) * 3600.0);
    QVERIFY(frame.solution.inEnvelope);
    QVERIFY(frame.solution.shootCue);
    QVERIFY(frame.solution.minimumRangeNm < frame.solution.turnAndRunRangeNm);
    QVERIFY(frame.solution.turnAndRunRangeNm < frame.solution.noEscapeRangeNm);
    QVERIFY(frame.solution.noEscapeRangeNm < frame.solution.interceptRangeNm);
    QVERIFY(frame.solution.interceptRangeNm < frame.solution.aerodynamicMaximumRangeNm);

    auto lowAltitudeInputs = inputs;
    lowAltitudeInputs.altitudeFeet = 0.0;
    QVERIFY(dlz::ScenarioAdapter::build(lowAltitudeInputs, &frame));
    QCOMPARE(frame.solution.aerodynamicMaximumRangeNm, 32.0 * 1.0 * 0.6 * (0.70 + 0.35 * 0.90));
    auto highAltitudeInputs = inputs;
    highAltitudeInputs.altitudeFeet = 100000.0;
    QVERIFY(dlz::ScenarioAdapter::build(highAltitudeInputs, &frame));
    QCOMPARE(frame.solution.aerodynamicMaximumRangeNm, 32.0 * 1.0 * 1.6 * (0.70 + 0.35 * 0.90));
}

void DlzTests::solverRejectsDerivedOverflow() {
    dlz::ScenarioFrame frame;
    QVERIFY(dlz::ScenarioAdapter::build(headOnInputs(), &frame));

    frame.weapon.referenceRangeNm = std::numeric_limits<double>::max();
    const auto overflow =
        dlz::solve(frame.shooter, frame.target, frame.atmosphere, frame.geometry, frame.weapon);
    QVERIFY(!overflow);
    QCOMPARE(overflow.error, dlz::DlzSolveError::NumericOverflow);

    QVERIFY(dlz::ScenarioAdapter::build(headOnInputs(), &frame));
    frame.weapon.averageMissileSpeedKnots = std::numeric_limits<double>::min();
    const auto timeOverflow =
        dlz::solve(frame.shooter, frame.target, frame.atmosphere, frame.geometry, frame.weapon);
    QVERIFY(!timeOverflow);
    QCOMPARE(timeOverflow.error, dlz::DlzSolveError::NumericOverflow);

    QVERIFY(dlz::ScenarioAdapter::build(headOnInputs(), &frame));
    frame.shooter.mach = std::numeric_limits<double>::quiet_NaN();
    const auto invalidInput =
        dlz::solve(frame.shooter, frame.target, frame.atmosphere, frame.geometry, frame.weapon);
    QCOMPARE(invalidInput.error, dlz::DlzSolveError::InvalidInput);

    QVERIFY(dlz::ScenarioAdapter::build(headOnInputs(), &frame));
    frame.geometry.aspectRadians = 3.14159265358979323846;
    const auto unsupported =
        dlz::solve(frame.shooter, frame.target, frame.atmosphere, frame.geometry, frame.weapon);
    QCOMPARE(unsupported.error, dlz::DlzSolveError::UnsupportedOrderedDomain);
}

void DlzTests::invalidTailDomainIsRejected() {
    dlz::ScenarioInputs inputs{13.0, 180.0, 30000.0, 0.90, 0.95};
    dlz::ScenarioFrame frame;
    QString error;
    QVERIFY(!dlz::ScenarioAdapter::build(inputs, &frame, &error));
    QVERIFY(error.contains(QStringLiteral("ordered domain")));
}

void DlzTests::maintainedScenarioFixturesExerciseSolverPath() {
    const auto buildScenario =
        [](const dlz::ScenarioInputs &inputs) -> std::optional<dlz::ScenarioFrame> {
        dlz::ScenarioFrame frame;
        QString error;
        const bool built = dlz::ScenarioAdapter::build(inputs, &frame, &error);
        if (!built) {
            return std::nullopt;
        }
        return frame;
    };

    const auto headOn = buildScenario(headOnInputs());
    const auto beam = buildScenario(beamInputs());
    const auto tail = buildScenario(tailInputs());
    const auto minimum = buildScenario(minimumInputs());
    const auto far = buildScenario(farInputs());
    const auto sweep = buildScenario(sweepInputs());
    QVERIFY(headOn.has_value());
    QVERIFY(beam.has_value());
    QVERIFY(tail.has_value());
    QVERIFY(minimum.has_value());
    QVERIFY(far.has_value());
    QVERIFY(sweep.has_value());

    QVERIFY(headOn->solution.inEnvelope);
    QVERIFY(headOn->solution.shootCue);
    QVERIFY(beam->solution.aerodynamicMaximumRangeNm < headOn->solution.aerodynamicMaximumRangeNm);
    QVERIFY(beam->geometry.rangeNm > beam->solution.interceptRangeNm);
    QVERIFY(!tail->solution.inEnvelope);
    QVERIFY(!tail->solution.shootCue);
    QVERIFY(!minimum->solution.inEnvelope);
    QVERIFY(!minimum->solution.shootCue);
    QVERIFY(!far->solution.inEnvelope);
    QVERIFY(!far->solution.shootCue);
    QVERIFY(!sweep->solution.inEnvelope);
}

void DlzTests::aspectSweepShrinksMonotonically() {
    double previousAero = std::numeric_limits<double>::infinity();
    double previousPi = std::numeric_limits<double>::infinity();
    double previousNe = std::numeric_limits<double>::infinity();
    double previousTr = std::numeric_limits<double>::infinity();
    for (int aspect = 0; aspect <= 180; aspect += 5) {
        auto inputs = sweepInputs();
        inputs.aspectDegrees = static_cast<double>(aspect);
        dlz::ScenarioFrame frame;
        QString error;
        QVERIFY2(dlz::ScenarioAdapter::build(inputs, &frame, &error), qPrintable(error));
        QVERIFY(frame.solution.aerodynamicMaximumRangeNm <= previousAero);
        QVERIFY(frame.solution.interceptRangeNm <= previousPi);
        QVERIFY(frame.solution.noEscapeRangeNm <= previousNe);
        QVERIFY(frame.solution.turnAndRunRangeNm <= previousTr);
        previousAero = frame.solution.aerodynamicMaximumRangeNm;
        previousPi = frame.solution.interceptRangeNm;
        previousNe = frame.solution.noEscapeRangeNm;
        previousTr = frame.solution.turnAndRunRangeNm;
    }
}

void DlzTests::scaleMappingAndDirectionalHysteresis() {
    QCOMPARE(dlz::presentation::rangeToY(0.0, 0.0, 40.0, 10.0, 410.0), 410.0);
    QCOMPARE(dlz::presentation::rangeToY(40.0, 0.0, 40.0, 10.0, 410.0), 10.0);
    QCOMPARE(dlz::presentation::rangeToY(60.0, 0.0, 40.0, 10.0, 410.0), 10.0);

    dlz::presentation::RangeScaleController scale;
    scale.reset(32.0);
    QCOMPARE(scale.scaleMaximumNm(), 40.0);
    QCOMPARE(scale.update(60.0), 80.0);
    // A falling sample crosses 48 nm and moves down once without immediately
    // oscillating back through the overlapping 90% threshold.
    QCOMPARE(scale.update(40.0), 40.0);
    QCOMPARE(scale.update(37.0), 40.0);
    QCOMPARE(scale.update(20.0), 20.0);

    dlz::presentation::RangeScaleController parked;
    parked.reset(60.0, 40.0);
    QCOMPARE(parked.scaleMaximumNm(), 40.0);
    QCOMPARE(parked.update(60.0), 80.0);
}

void DlzTests::presentationFiltersCaretAndTimesShootCue() {
    dlz::ScenarioFrame frame;
    QVERIFY(dlz::ScenarioAdapter::build(headOnInputs(), &frame));
    dlz::presentation::PresentationController controller;
    controller.reset();
    controller.update(frame.solution, frame.geometry.rangeNm, frame.geometry.rangeRateKnots,
                      dlz::HudMode::Prelaunch, 0.0);
    QCOMPARE(controller.displayedRangeNm(), 20.0);
    QVERIFY(controller.state().shootFlash);

    controller.update(frame.solution, 26.0, frame.geometry.rangeRateKnots, dlz::HudMode::Prelaunch,
                      0.10);
    QVERIFY(controller.displayedRangeNm() > 20.0);
    QVERIFY(controller.displayedRangeNm() < 26.0);
    QVERIFY(controller.state().shootFlash);

    controller.update(frame.solution, 26.0, frame.geometry.rangeRateKnots, dlz::HudMode::Prelaunch,
                      0.25);
    QVERIFY(controller.shootCueActive()); // minimum visible time
    controller.update(frame.solution, 30.0, frame.geometry.rangeRateKnots, dlz::HudMode::Prelaunch,
                      0.25);
    QVERIFY(!controller.state().shootFlash);

    dlz::presentation::PresentationController parkedController;
    parkedController.reset(40.0);
    parkedController.update(frame.solution, 60.0, frame.geometry.rangeRateKnots,
                            dlz::HudMode::Prelaunch, 0.0);
    QCOMPARE(parkedController.state().scaleMaximumNm, 40.0);
    QVERIFY(parkedController.state().caretParked);
    parkedController.update(frame.solution, 60.0, frame.geometry.rangeRateKnots,
                            dlz::HudMode::Prelaunch, 0.0);
    QCOMPARE(parkedController.state().scaleMaximumNm, 80.0);
}

void DlzTests::noTrackClearsPresentation() {
    dlz::ScenarioFrame frame;
    QVERIFY(dlz::ScenarioAdapter::build(headOnInputs(), &frame));
    dlz::presentation::PresentationController controller;
    controller.update(frame.solution, frame.geometry.rangeNm, frame.geometry.rangeRateKnots,
                      dlz::HudMode::Prelaunch, 0.0);
    QVERIFY(controller.hasSolution());
    controller.update(frame.solution, frame.geometry.rangeNm, frame.geometry.rangeRateKnots,
                      dlz::HudMode::NoTrack, 0.1);
    QVERIFY(!controller.hasSolution());
    QCOMPARE(controller.state().mode, dlz::HudMode::NoTrack);

    controller.update(frame.solution, frame.geometry.rangeNm, frame.geometry.rangeRateKnots,
                      dlz::HudMode::Inflight, 0.1);
    QVERIFY(!controller.hasSolution());
    QCOMPARE(controller.state().mode, dlz::HudMode::Inflight);
}

void DlzTests::rendererDrawsDirectFixture() {
    const auto fixture = dlz::presentation::fixtureA();
    const auto &solution = fixture.solution;
    const auto &state = fixture.hudState;

    QImage image(640, 480, QImage::Format_ARGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    dlz::presentation::HudRenderer renderer;
    renderer.draw(painter, QRectF(image.rect()), solution, state, fixture.displayedRangeNm);
    painter.end();

    int nonBlackPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) != QColor(Qt::black)) {
                ++nonBlackPixels;
            }
        }
    }
    QVERIFY(nonBlackPixels > 500);

    const auto fixtureB = dlz::presentation::fixtureB();
    QCOMPARE(fixtureB.solution.aerodynamicMaximumRangeNm, 11.5);
    QCOMPARE(fixtureB.solution.interceptRangeNm, 9.0);
    QCOMPARE(fixtureB.hudState.scaleMaximumNm, 20.0);
    QVERIFY(!fixtureB.hudState.caretParked);
    QImage fixtureBImage(640, 480, QImage::Format_ARGB32);
    fixtureBImage.fill(Qt::black);
    QPainter fixtureBPainter(&fixtureBImage);
    renderer.draw(fixtureBPainter, QRectF(fixtureBImage.rect()), fixtureB.solution,
                  fixtureB.hudState, fixtureB.displayedRangeNm);
    fixtureBPainter.end();
    int fixtureBNonBlack = 0;
    for (int y = 0; y < fixtureBImage.height(); ++y) {
        for (int x = 0; x < fixtureBImage.width(); ++x) {
            if (fixtureBImage.pixelColor(x, y) != QColor(Qt::black)) {
                ++fixtureBNonBlack;
            }
        }
    }
    QVERIFY(fixtureBNonBlack > 500);

    dlz::Solution invalid = fixture.solution;
    invalid.aerodynamicMaximumRangeNm = std::numeric_limits<double>::quiet_NaN();
    QImage noTrackImage(320, 240, QImage::Format_ARGB32);
    noTrackImage.fill(Qt::black);
    QPainter noTrackPainter(&noTrackImage);
    renderer.draw(noTrackPainter, QRectF(noTrackImage.rect()), invalid, fixture.hudState,
                  fixture.displayedRangeNm);
    noTrackPainter.end();
    QVERIFY(noTrackImage.pixelColor(160, 120) != QColor(Qt::black));
}

QTEST_MAIN(DlzTests)

#include "dlz_tests.moc"
