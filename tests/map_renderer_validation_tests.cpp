#include "viewer/map/earth_map_gpu_renderer.h"
#include "viewer/map/map_camera.h"

#include <QtTest>

#include <cmath>
#include <limits>
#include <memory>

class MapRendererValidationTests final : public QObject {
    Q_OBJECT

  private slots:
    void acceptsBoundedMeshWithoutAContext();
    void rejectsNonFiniteVerticesBeforeGpuUpload();
    void rejectsOutOfRangeIndicesBeforeGpuUpload();
    void sphereTrackingRetainsSubFloatPrecision();
};

std::shared_ptr<lar::map::MapMesh> triangleMesh() {
    auto mesh = std::make_shared<lar::map::MapMesh>();
    mesh->vertices = {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F};
    mesh->mercatorFillIndices = {0U, 1U, 2U};
    mesh->sphereFillIndices = {0U, 1U, 2U};
    mesh->borderIndices = {0U, 1U};
    return mesh;
}

void MapRendererValidationTests::acceptsBoundedMeshWithoutAContext() {
    lar::map::EarthMapGpuRenderer renderer;
    QString error;
    QVERIFY2(renderer.setMesh(triangleMesh(), &error), qPrintable(error));
}

void MapRendererValidationTests::rejectsNonFiniteVerticesBeforeGpuUpload() {
    auto mesh = triangleMesh();
    mesh->vertices[0] = std::numeric_limits<float>::quiet_NaN();
    lar::map::EarthMapGpuRenderer renderer;
    QString error;

    QVERIFY(!renderer.setMesh(std::move(mesh), &error));
    QVERIFY(!error.isEmpty());
}

void MapRendererValidationTests::rejectsOutOfRangeIndicesBeforeGpuUpload() {
    auto mesh = triangleMesh();
    mesh->sphereFillIndices[2] = 3U;
    lar::map::EarthMapGpuRenderer renderer;
    QString error;

    QVERIFY(!renderer.setMesh(std::move(mesh), &error));
    QVERIFY(!error.isEmpty());
}

void MapRendererValidationTests::sphereTrackingRetainsSubFloatPrecision() {
    constexpr int Width = 900;
    constexpr int Height = 650;
    constexpr double DegreesPerMeter = 180.0 / (3.14159265358979323846 * 6'371'000.0);
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Sphere);
    camera.setSphereZoom(5000.0F);

    for (int sample = 0; sample < 80; ++sample) {
        const double longitude = 32.0 + sample * 0.1 * DegreesPerMeter;
        const double latitude = 40.0 + sample * 0.07 * DegreesPerMeter;
        camera.setSphereCenter(longitude, latitude);

        QPointF screen;
        bool visible = false;
        QVERIFY(camera.projectGeoToScreen(longitude, latitude, Width, Height, screen, visible));
        QVERIFY(visible);
        QVERIFY(std::abs(screen.x() - Width * 0.5) < 1.0e-9);
        QVERIFY(std::abs(screen.y() - Height * 0.5) < 1.0e-9);

        const lar::map::SphereProjectionParameters projection = camera.sphereProjectionParameters();
        const double reconstructedLongitude =
            static_cast<double>(projection.centerHighDegrees.x()) +
            static_cast<double>(projection.centerLowDegrees.x());
        const double reconstructedLatitude = static_cast<double>(projection.centerHighDegrees.y()) +
                                             static_cast<double>(projection.centerLowDegrees.y());
        QVERIFY(std::abs(reconstructedLongitude - longitude) < 1.0e-12);
        QVERIFY(std::abs(reconstructedLatitude - latitude) < 1.0e-12);
    }
}

QTEST_APPLESS_MAIN(MapRendererValidationTests)

#include "map_renderer_validation_tests.moc"
