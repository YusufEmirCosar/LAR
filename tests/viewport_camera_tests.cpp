#include "domain/statefield.h"
#include "viewer/viewport/earth_camera_policy.h"
#include "viewer/viewport/lar_view_fit.h"
#include "viewer/viewport/viewport_camera_controller.h"

#include <QBitArray>
#include <QtMath>
#include <QtTest>

#include <cmath>
#include <initializer_list>

class ViewportCameraTests final : public QObject {
    Q_OBJECT

  private slots:
    void followsPlaneAndOptionallyTurns();
    void followsTargetNorthUp();
    void missingAnchorFreezesLastValidPosition();
    void outOfRangeAnchorDoesNotReplaceLastValidPosition();
    void freeMovementNeverAppliesBearing();
    void missingYawFallsBackNorthUp();
    void mercatorFitKeepsDatelineGeometryTogether();
    void sphereFitReportsOppositeHemisphere();
    void sphereFitPreservesTrackedCenterPrecision();
    void earthPolicyRejectsInvalidAnchorsAndAnchorsTrackedZoom();
};

QBitArray cameraFields(std::initializer_list<int> fields) {
    QBitArray available(StateField::Count);
    for (const int field : fields) {
        available.setBit(field);
    }
    return available;
}

void ViewportCameraTests::followsPlaneAndOptionallyTurns() {
    Plane plane{};
    plane.location[0] = 0.4;
    plane.location[1] = 0.8;
    plane.euler[0] = 0.75;
    Target target{};
    ViewportCameraController controller;
    controller.setScene(
        plane, target,
        cameraFields({StateField::Location0, StateField::Location1, StateField::Euler0}));

    QVERIFY(controller.state().trackingActive);
    QVERIFY(controller.state().hasAnchor);
    QCOMPARE(controller.state().anchorRadians[0], plane.location[0]);
    QCOMPARE(controller.state().anchorRadians[1], plane.location[1]);
    QCOMPARE(controller.state().bearingRadians, plane.euler[0]);

    controller.setTurnWithPlane(false);
    QCOMPARE(controller.state().bearingRadians, 0.0);
    QCOMPARE(controller.state().anchorRadians[0], plane.location[0]);
}

void ViewportCameraTests::followsTargetNorthUp() {
    Plane plane{};
    plane.euler[0] = 1.2;
    Target target{};
    target.iz_pos[0] = -0.3;
    target.iz_pos[1] = 2.7;
    ViewportCameraController controller;
    controller.setMode(CameraTrackingMode::FollowTarget);
    controller.setScene(plane, target,
                        cameraFields({StateField::Euler0, StateField::IzPos0, StateField::IzPos1}));

    QCOMPARE(controller.state().anchorRadians[0], target.iz_pos[0]);
    QCOMPARE(controller.state().anchorRadians[1], target.iz_pos[1]);
    QCOMPARE(controller.state().bearingRadians, 0.0);
}

void ViewportCameraTests::missingAnchorFreezesLastValidPosition() {
    Plane plane{};
    plane.location[0] = 0.2;
    plane.location[1] = -0.6;
    Target target{};
    ViewportCameraController controller;
    controller.setScene(plane, target,
                        cameraFields({StateField::Location0, StateField::Location1}));
    const auto previous = controller.state().anchorRadians;

    plane.location[0] = 1.0;
    plane.location[1] = 1.0;
    controller.setScene(plane, target, {});
    QVERIFY(controller.state().hasAnchor);
    QVERIFY(controller.state().anchorRadians == previous);
}

void ViewportCameraTests::outOfRangeAnchorDoesNotReplaceLastValidPosition() {
    Plane plane{};
    plane.location[0] = 0.2;
    plane.location[1] = -0.6;
    Target target{};
    ViewportCameraController controller;
    const QBitArray fields = cameraFields({StateField::Location0, StateField::Location1});
    controller.setScene(plane, target, fields);
    const auto previous = controller.state().anchorRadians;

    plane.location[0] = 2.0;
    plane.location[1] = 4.0;
    controller.setScene(plane, target, fields);
    QVERIFY(controller.state().anchorRadians == previous);
}

void ViewportCameraTests::freeMovementNeverAppliesBearing() {
    Plane plane{};
    plane.location[0] = 0.2;
    plane.location[1] = 0.3;
    plane.euler[0] = 1.0;
    Target target{};
    ViewportCameraController controller;
    controller.setScene(
        plane, target,
        cameraFields({StateField::Location0, StateField::Location1, StateField::Euler0}));
    controller.setMode(CameraTrackingMode::Free);

    QVERIFY(!controller.state().trackingActive);
    QCOMPARE(controller.state().bearingRadians, 0.0);
    QVERIFY(controller.state().hasAnchor);
}

void ViewportCameraTests::missingYawFallsBackNorthUp() {
    Plane plane{};
    plane.location[0] = 0.2;
    plane.location[1] = 0.3;
    plane.euler[0] = 1.0;
    Target target{};
    ViewportCameraController controller;
    controller.setScene(plane, target,
                        cameraFields({StateField::Location0, StateField::Location1}));

    QVERIFY(controller.turnWithPlane());
    QCOMPARE(controller.state().bearingRadians, 0.0);
}

void ViewportCameraTests::mercatorFitKeepsDatelineGeometryTogether() {
    const std::vector<LarViewFit::Coordinate> coordinates{
        {qDegreesToRadians(10.0), qDegreesToRadians(179.0)},
        {qDegreesToRadians(10.0), qDegreesToRadians(-179.0)}};
    const auto fit = LarViewFit::mercator(coordinates, std::nullopt, 0.0, QSize(800, 600));

    QVERIFY(fit.has_value());
    QVERIFY(std::abs(std::abs(fit->center.x()) - 180.0) < 0.001);
    QVERIFY(fit->zoom > 10.0F);
}

void ViewportCameraTests::sphereFitReportsOppositeHemisphere() {
    const std::vector<LarViewFit::Coordinate> coordinates{{0.0, 0.0},
                                                          {0.0, qDegreesToRadians(170.0)}};
    const auto fit = LarViewFit::sphere(coordinates, LarViewFit::Coordinate{0.0, 0.0});

    QVERIFY(fit.has_value());
    QVERIFY(fit->crossesHorizon);
    QCOMPARE(fit->latitudeDegrees, 0.0);
    QCOMPARE(fit->longitudeDegrees, 0.0);
}

void ViewportCameraTests::sphereFitPreservesTrackedCenterPrecision() {
    const LarViewFit::Coordinate tracked{0.123456789012345, -2.345678901234567};
    const auto fit = LarViewFit::sphere({tracked}, tracked);

    QVERIFY(fit.has_value());
    QVERIFY(std::abs(fit->latitudeDegrees - qRadiansToDegrees(tracked.first)) < 1.0e-12);
    QVERIFY(std::abs(fit->longitudeDegrees - qRadiansToDegrees(tracked.second)) < 1.0e-12);
}

void ViewportCameraTests::earthPolicyRejectsInvalidAnchorsAndAnchorsTrackedZoom() {
    ViewportCameraState state;
    state.trackingActive = true;
    state.hasAnchor = true;
    state.anchorRadians = {2.0, 0.0};
    QVERIFY(!EarthCameraPolicy::trackedCoordinate(state, true).has_value());

    state.anchorRadians = {0.25, -2.75};
    const auto tracked = EarthCameraPolicy::trackedCoordinate(state, true);
    QVERIFY(tracked.has_value());
    QCOMPARE(tracked->first, 0.25);
    QCOMPARE(tracked->second, -2.75);
    QCOMPARE(EarthCameraPolicy::zoomAnchor(state, true, 801, 601, QPointF(12.0, 15.0)),
             QPointF(400.5, 300.5));
    QCOMPARE(EarthCameraPolicy::zoomAnchor(state, false, 801, 601, QPointF(12.0, 15.0)),
             QPointF(12.0, 15.0));
}

QTEST_APPLESS_MAIN(ViewportCameraTests)

#include "viewport_camera_tests.moc"
