#include "domain/statefield.h"
#include "viewer/plane/plane_scene_widget.h"

#include "support/dted_fixture.h"

#include <QApplication>
#include <QBitArray>
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double EarthRadiusMeters = 6371008.8;

int centerLuminance(const QImage &image) {
    constexpr int Radius = 4;
    const int centerX = image.width() / 2;
    const int centerY = image.height() / 2;
    int total = 0;
    int samples = 0;
    for (int y = centerY - Radius; y <= centerY + Radius; ++y) {
        for (int x = centerX - Radius; x <= centerX + Radius; ++x) {
            total += qGray(image.pixel(x, y));
            ++samples;
        }
    }
    return samples > 0 ? total / samples : 0;
}

int surfaceGridContrast(const QImage &image) {
    int minimum = 255;
    int maximum = 0;
    for (int y = image.height() * 45 / 100; y < image.height() * 75 / 100; ++y) {
        for (int x = image.width() * 5 / 100; x < image.width() * 30 / 100; ++x) {
            const int luminance = qGray(image.pixel(x, y));
            minimum = std::min(minimum, luminance);
            maximum = std::max(maximum, luminance);
        }
    }
    return maximum - minimum;
}

int targetPixelCount(const QImage &image) {
    int count = 0;
    for (int y = image.height() / 3; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.red() > color.green() + 50 && color.red() > color.blue() + 50) {
                ++count;
            }
        }
    }
    return count;
}

int deepWaterPixelCount(const QImage &image) {
    int count = 0;
    for (int y = image.height() * 45 / 100; y < image.height() * 90 / 100; ++y) {
        for (int x = image.width() * 5 / 100; x < image.width() * 95 / 100; ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.blue() > 20 && color.blue() > color.red() + 20 &&
                color.blue() > color.green() + 10) {
                ++count;
            }
        }
    }
    return count;
}

int landPixelCount(const QImage &image) {
    int count = 0;
    for (int y = image.height() * 45 / 100; y < image.height() * 90 / 100; ++y) {
        for (int x = image.width() * 5 / 100; x < image.width() * 95 / 100; ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.green() > color.red() + 12 && color.green() > color.blue() + 8) {
                ++count;
            }
        }
    }
    return count;
}

bool waitForFrame(PlaneSceneWidget &widget) {
    bool rendered = false;
    const QMetaObject::Connection connection = QObject::connect(
        &widget, &PlaneSceneWidget::frameRendered, &widget, [&rendered] { rendered = true; });
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000 && (!rendered || !widget.rendererReady() || !widget.isValid())) {
        QCoreApplication::processEvents();
    }
    QObject::disconnect(connection);
    return rendered && widget.rendererReady() && widget.isValid();
}

bool waitForTerrain(PlaneSceneWidget &widget) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000 && !widget.terrainPatchReady()) {
        QCoreApplication::processEvents();
    }
    return widget.terrainPatchReady() && waitForFrame(widget);
}

LarSceneState representativeScene(double altitudeMeters = 10.0) {
    const auto radians = [](double degrees) { return degrees * Pi / 180.0; };
    LarSceneState scene;
    scene.hasScene = true;
    scene.availableFields = QBitArray(StateField::Count, true);
    scene.plane.location[0] = radians(39.0);
    scene.plane.location[1] = radians(35.0);
    scene.plane.location[2] = altitudeMeters;
    scene.plane.euler[0] = radians(35.0);
    scene.plane.euler[1] = radians(12.0);
    scene.plane.euler[2] = radians(-28.0);
    scene.target.ir_pos[0] = scene.plane.location[0];
    scene.target.ir_pos[1] = scene.plane.location[1];
    scene.target.iz_pos[0] = scene.plane.location[0];
    scene.target.iz_pos[1] = scene.plane.location[1];
    scene.target.ir_r = 45.0;
    scene.target.iz_r1 = 15.0;
    scene.target.iz_r2 = 60.0;
    scene.target.iz_theta1 = radians(20.0);
    scene.target.iz_theta2 = radians(150.0);
    return scene;
}

LarSceneState deepOceanScene() {
    const auto radians = [](double degrees) { return degrees * Pi / 180.0; };
    LarSceneState scene = representativeScene(3300.0);
    scene.plane.location[0] = radians(30.5);
    scene.plane.location[1] = radians(-29.5);
    scene.target.ir_pos[0] = scene.plane.location[0];
    scene.target.ir_pos[1] = scene.plane.location[1];
    scene.target.iz_pos[0] = scene.plane.location[0];
    scene.target.iz_pos[1] = scene.plane.location[1];
    return scene;
}

LarSceneState nearShoreScene() {
    const auto radians = [](double degrees) { return degrees * Pi / 180.0; };
    LarSceneState scene = representativeScene(3000.0);
    scene.plane.location[0] = radians(41.0134);
    scene.plane.location[1] = radians(28.9550);
    scene.plane.euler[0] = radians(-135.0);
    scene.target.ir_pos[0] = radians(41.00658939751302);
    scene.target.ir_pos[1] = radians(28.945827810009188);
    scene.target.iz_pos[0] = scene.target.ir_pos[0];
    scene.target.iz_pos[1] = scene.target.ir_pos[1];
    scene.target.ir_r = 4000.0;
    scene.target.iz_r1 = 1000.0;
    scene.target.iz_r2 = 5000.0;
    return scene;
}

} // namespace

int main(int argc, char *argv[]) {
    if (!qEnvironmentVariableIsSet("LAR_RUN_PLANE_GPU_TESTS")) {
        return 77;
    }
    QApplication application(argc, argv);
    PlaneSceneWidget widget(QCoreApplication::applicationDirPath());
    widget.resize(900, 650);
    widget.setSceneState(representativeScene());
    widget.show();
    if (!waitForFrame(widget)) {
        std::cerr << "FAILED: Plane view did not produce a valid OpenGL frame.\n";
        return 1;
    }
    const QImage firstSkybox = widget.grabFramebuffer();
    if (firstSkybox.isNull()) {
        std::cerr << "FAILED: Plane framebuffer is empty.\n";
        return 1;
    }
    if (centerLuminance(firstSkybox) < 8) {
        std::cerr << "FAILED: F-16 texture rendered black at the scene center.\n";
        return 1;
    }
    widget.setSurfaceVisible(true);
    if (!waitForFrame(widget)) {
        std::cerr << "FAILED: Enabling the Plane tactical surface did not render.\n";
        return 1;
    }
    const QImage surfaceFrame = widget.grabFramebuffer();
    if (surfaceFrame.isNull() || surfaceFrame == firstSkybox) {
        std::cerr << "FAILED: Surface toggle left the Plane framebuffer unchanged.\n";
        return 1;
    }
    if (centerLuminance(surfaceFrame) < 8) {
        std::cerr << "FAILED: Enabling the surface made the fixed-size F-16 disappear.\n";
        return 1;
    }
    if (surfaceGridContrast(surfaceFrame) < 10) {
        std::cerr << "FAILED: Plane tactical surface grid is not visible.\n";
        return 1;
    }
    if (targetPixelCount(surfaceFrame) < 20) {
        std::cerr << "FAILED: Plane tactical surface target marker is not visible.\n";
        return 1;
    }

    LarSceneState highAltitudeScene = representativeScene(3300.0);
    constexpr double TargetDistanceMeters = 5'000.0;
    const double targetBearing = highAltitudeScene.plane.euler[0] + Pi / 12.0;
    const double targetNorth = std::cos(targetBearing) * TargetDistanceMeters;
    const double targetEast = std::sin(targetBearing) * TargetDistanceMeters;
    const double targetLatitude =
        highAltitudeScene.plane.location[0] + targetNorth / EarthRadiusMeters;
    const double targetLongitude =
        highAltitudeScene.plane.location[1] +
        targetEast / (EarthRadiusMeters * std::cos(highAltitudeScene.plane.location[0]));
    highAltitudeScene.target.ir_pos[0] = targetLatitude;
    highAltitudeScene.target.ir_pos[1] = targetLongitude;
    highAltitudeScene.target.iz_pos[0] = targetLatitude;
    highAltitudeScene.target.iz_pos[1] = targetLongitude;
    highAltitudeScene.target.ir_r = 4000.0;
    highAltitudeScene.target.iz_r1 = 1000.0;
    highAltitudeScene.target.iz_r2 = 5000.0;
    widget.setSceneState(highAltitudeScene);
    if (!waitForFrame(widget)) {
        std::cerr << "FAILED: Plane view did not render the 3.3 km altitude scene.\n";
        return 1;
    }
    const QImage highAltitudeFrame = widget.grabFramebuffer();
    const PlaneSurfaceState &highAltitudeSurface = widget.surfaceState();
    const double renderedAltitude = -static_cast<double>(highAltitudeSurface.surfaceHeight) *
                                    highAltitudeSurface.metersPerSceneUnit;
    if (highAltitudeFrame.isNull() || highAltitudeFrame == surfaceFrame ||
        std::abs(renderedAltitude - 3300.0) > 0.1) {
        std::cerr << "FAILED: Plane surface did not render at the 3.3 km telemetry altitude.\n";
        return 1;
    }
    if (surfaceGridContrast(highAltitudeFrame) < 10) {
        std::cerr << "FAILED: Ground grid is not visible at the 3.3 km telemetry altitude.\n";
        return 1;
    }
    const QString snapshotPath = qEnvironmentVariable("LAR_PLANE_SURFACE_SNAPSHOT");
    if (!snapshotPath.isEmpty() && !highAltitudeFrame.save(snapshotPath)) {
        std::cerr << "FAILED: Could not save the requested Plane surface snapshot.\n";
        return 1;
    }
    if (targetPixelCount(highAltitudeFrame) < 6) {
        std::cerr << "FAILED: Small target pyramid is not visible at 3.3 km.\n";
        return 1;
    }
    if (!widget.terrainAvailable()) {
        std::cerr << "FAILED: Configured DTED0 source is unavailable to Plane mode.\n";
        return 1;
    }
    widget.setTerrainVisible(true);
    if (!waitForTerrain(widget)) {
        std::cerr << "FAILED: DTED0 terrain did not prepare and render.\n";
        return 1;
    }
    const QImage terrainFrame = widget.grabFramebuffer();
    if (terrainFrame.isNull() || terrainFrame == highAltitudeFrame) {
        std::cerr << "FAILED: Terrain toggle left the Plane framebuffer unchanged.\n";
        return 1;
    }
    if (centerLuminance(terrainFrame) < 8 || surfaceGridContrast(terrainFrame) < 10) {
        std::cerr << "FAILED: DTED0 terrain obscured the aircraft or tactical overlays.\n";
        return 1;
    }

    const QString defaultTerrainRoot = widget.terrainRootDirectory();
    QTemporaryDir level1Directory;
    if (!level1Directory.isValid() ||
        !dted_test_fixture::writeCell(level1Directory.path(), DtedLevel::Level1, {35, 39}, 1,
                                      1600) ||
        !widget.loadTerrainFromDirectory(level1Directory.path(), DtedLevel::Level1) ||
        !waitForTerrain(widget)) {
        std::cerr << "FAILED: User-selected DT1 terrain did not prepare and render.\n";
        return 1;
    }
    const QImage level1Frame = widget.grabFramebuffer();
    if (level1Frame.isNull() || level1Frame == terrainFrame ||
        widget.terrainLevel() != DtedLevel::Level1) {
        std::cerr << "FAILED: DT1 source replacement left the terrain framebuffer unchanged.\n";
        return 1;
    }
    if (!widget.loadTerrainFromDirectory(defaultTerrainRoot, DtedLevel::Level0) ||
        !waitForTerrain(widget)) {
        std::cerr << "FAILED: Restoring the default DT0 source did not render.\n";
        return 1;
    }
    widget.setSceneState(nearShoreScene());
    if (!waitForTerrain(widget)) {
        std::cerr << "FAILED: Mixed vector-coast terrain did not prepare and render.\n";
        return 1;
    }
    const QImage nearShoreFrame = widget.grabFramebuffer();
    const int nearShoreWater = deepWaterPixelCount(nearShoreFrame);
    const int nearShoreLand = landPixelCount(nearShoreFrame);
    if (nearShoreFrame.isNull() || nearShoreFrame == level1Frame || nearShoreWater < 100 ||
        nearShoreLand < 100) {
        std::cerr << "FAILED: Mixed vector-coast land/water passes did not render "
                  << "(null=" << nearShoreFrame.isNull()
                  << ", unchanged=" << (nearShoreFrame == level1Frame)
                  << ", waterPixels=" << nearShoreWater << ", landPixels=" << nearShoreLand
                  << ").\n";
        return 1;
    }
    widget.setSceneState(deepOceanScene());
    if (!waitForTerrain(widget)) {
        std::cerr << "FAILED: DTED0 bathymetry did not prepare and render.\n";
        return 1;
    }
    const QImage waterFrame = widget.grabFramebuffer();
    if (waterFrame.isNull() || waterFrame == nearShoreFrame ||
        deepWaterPixelCount(waterFrame) < waterFrame.width() * waterFrame.height() / 100) {
        std::cerr << "FAILED: Deep-ocean terrain did not render with a blue depth ramp.\n";
        return 1;
    }
    if (!widget.selectNextSkybox() || !waitForFrame(widget)) {
        std::cerr << "FAILED: Changing the active skybox did not render.\n";
        return 1;
    }
    const QImage secondSkybox = widget.grabFramebuffer();
    if (secondSkybox == waterFrame) {
        std::cerr << "FAILED: Changing skybox left the framebuffer unchanged.\n";
        return 1;
    }
    std::cout << "lar-plane-view-gpu-tests PASSED cleanly!\n";
    return 0;
}
