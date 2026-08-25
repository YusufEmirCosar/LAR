#include "domain/statefield.h"
#include "viewer/map/packaged_map_asset_source.h"
#include "viewer/viewport/earth_lar_view.h"

#include <QApplication>
#include <QBitArray>
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>

#include <cmath>
#include <iostream>
#include <memory>

namespace {

constexpr double Pi = 3.14159265358979323846;

bool colorsAreClose(const QColor &actual, const QColor &expected, int tolerance = 18) {
    return std::abs(actual.red() - expected.red()) <= tolerance &&
           std::abs(actual.green() - expected.green()) <= tolerance &&
           std::abs(actual.blue() - expected.blue()) <= tolerance;
}

bool imageContainsColor(const QImage &image, const QColor &expected, int tolerance = 18) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (colorsAreClose(image.pixelColor(x, y), expected, tolerance)) {
                return true;
            }
        }
    }
    return false;
}

bool waitForFrame(EarthLarView &viewport, int previousFrameCount) {
    int frameCount = previousFrameCount;
    QMetaObject::Connection connection = QObject::connect(
        &viewport, &EarthLarView::frameRendered, &viewport, [&frameCount]() { ++frameCount; });

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000 &&
           (frameCount == previousFrameCount || !viewport.isValid() || !viewport.isShaderValid())) {
        QCoreApplication::processEvents();
    }
    QObject::disconnect(connection);
    return frameCount > previousFrameCount && viewport.isValid() && viewport.isShaderValid();
}

bool frameContainsLarOverlays(const QImage &frame) {
    return !frame.isNull() && imageContainsColor(frame, QColor(QStringLiteral("#346d91"))) &&
           imageContainsColor(frame, QColor(QStringLiteral("#0e755b")));
}

void populateRepresentativeState(Plane &plane, Target &target, QBitArray &availableFields) {
    const auto radians = [](double degrees) { return degrees * Pi / 180.0; };

    plane.location[0] = radians(39.0);
    plane.location[1] = radians(34.5);
    plane.euler[0] = radians(55.0);

    target.iz_pos[0] = radians(39.0);
    target.iz_pos[1] = radians(35.0);
    target.ir_pos[0] = radians(39.15);
    target.ir_pos[1] = radians(35.1);
    target.iz_theta1 = radians(25.0);
    target.iz_theta2 = radians(155.0);
    target.iz_r1 = 45000.0;
    target.iz_r2 = 180000.0;
    target.ir_r = 125000.0;

    availableFields = QBitArray(StateField::Count, true);
}

} // namespace

int main(int argc, char *argv[]) {
    if (!qEnvironmentVariableIsSet("LAR_RUN_MAP_GPU_TESTS")) {
        return 77;
    }

    QApplication application(argc, argv);
    EarthLarView viewport(
        std::make_shared<lar::map::PackagedMapAssetSource>(QCoreApplication::applicationDirPath()));
    viewport.resize(900, 650);

    QString diagnostic;
    QObject::connect(&viewport, &EarthLarView::diagnosticRaised, &viewport,
                     [&diagnostic](const QString &message) { diagnostic = message; });

    if (!viewport.ensureMapLoaded()) {
        std::cerr << "FAILED: " << diagnostic.toStdString() << "\n";
        return 1;
    }

    Plane plane{};
    Target target{};
    QBitArray availableFields;
    populateRepresentativeState(plane, target, availableFields);
    ViewportCameraController cameraController;
    cameraController.setMode(CameraTrackingMode::FollowTarget);
    cameraController.setScene(plane, target, availableFields);
    viewport.setState(plane, target, availableFields);
    viewport.setCameraState(cameraController.state());
    viewport.setLarViewMode(LarViewMode::Mercator);
    viewport.show();
    viewport.fitToData();

    if (!waitForFrame(viewport, 0)) {
        std::cerr << "FAILED: No valid Mercator OpenGL frame was rendered. "
                  << diagnostic.toStdString() << "\n";
        return 1;
    }
    const QImage mercatorFrame = viewport.grabFramebuffer();
    if (!frameContainsLarOverlays(mercatorFrame)) {
        mercatorFrame.save(QStringLiteral("lar-mercator-gpu-failure.png"));
        QPointF irScreen;
        QPointF izScreen;
        bool irVisible = false;
        bool izVisible = false;
        const bool irProjected = viewport.projectGeoToScreen(35.1, 39.15, irScreen, irVisible);
        const bool izProjected = viewport.projectGeoToScreen(35.0, 39.0, izScreen, izVisible);
        std::cerr << "FAILED: Mercator framebuffer does not contain both "
                     "geodesic LAR overlays. Captured "
                     "lar-mercator-gpu-failure.png. IR center=("
                  << irScreen.x() << ", " << irScreen.y() << "), projected=" << irProjected
                  << ", visible=" << irVisible << "; IZ center=(" << izScreen.x() << ", "
                  << izScreen.y() << "), projected=" << izProjected << ", visible=" << izVisible
                  << "; zoom=" << viewport.activeZoom() << ".\n";
        return 1;
    }
    if (std::abs(viewport.cameraBearing()) > 0.001F) {
        std::cerr << "FAILED: Follow-target Mercator camera is not north-up.\n";
        return 1;
    }

    viewport.setLarViewMode(LarViewMode::Sphere);
    viewport.fitToData();
    if (!waitForFrame(viewport, 0)) {
        std::cerr << "FAILED: No valid spherical OpenGL frame was rendered. "
                  << diagnostic.toStdString() << "\n";
        return 1;
    }
    const QImage sphereFrame = viewport.grabFramebuffer();
    if (!frameContainsLarOverlays(sphereFrame)) {
        sphereFrame.save(QStringLiteral("lar-sphere-gpu-failure.png"));
        std::cerr << "FAILED: Spherical framebuffer does not contain both "
                     "geodesic LAR overlays. Captured "
                     "lar-sphere-gpu-failure.png.\n";
        return 1;
    }
    if (std::abs(viewport.cameraBearing()) > 0.001F) {
        std::cerr << "FAILED: Follow-target spherical camera is not north-up.\n";
        return 1;
    }

    cameraController.setMode(CameraTrackingMode::FollowPlane);
    cameraController.setTurnWithPlane(true);
    viewport.setCameraState(cameraController.state());
    QCoreApplication::processEvents();
    if (std::abs(viewport.cameraBearing() - 55.0F) > 0.01F) {
        std::cerr << "FAILED: Follow-plane turn mode did not apply plane yaw.\n";
        return 1;
    }

    cameraController.setTurnWithPlane(false);
    viewport.setCameraState(cameraController.state());
    if (std::abs(viewport.cameraBearing()) > 0.001F) {
        std::cerr << "FAILED: Follow-plane north-up mode retained camera yaw.\n";
        return 1;
    }

    std::cout << "lar-earth-view-gpu-tests PASSED cleanly!\n";
    return 0;
}
