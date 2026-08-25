#include "domain/statefield.h"
#include "viewer/map/map_camera.h"
#include "viewer/viewport/earth_overlay_coordinator.h"

#include <QBitArray>
#include <QSignalSpy>
#include <QtTest>

#include <limits>
#include <memory>

namespace {

QBitArray inRangeFields() {
    QBitArray fields(StateField::Count);
    fields.setBit(StateField::IrPos0);
    fields.setBit(StateField::IrPos1);
    fields.setBit(StateField::IrR);
    return fields;
}

Target validTarget() {
    Target target{};
    target.ir_pos[0] = 1.1;
    target.ir_pos[1] = 2.9;
    target.ir_r = 900000.0;
    return target;
}

} // namespace

class ViewportWorkerTests final : public QObject {
    Q_OBJECT

  private slots:
    void onlyLatestRevisionIsPublished();
    void destructionWhilePreparationIsPendingIsSafe();
};

void ViewportWorkerTests::onlyLatestRevisionIsPublished() {
    EarthOverlayCoordinator coordinator;
    QSignalSpy readySpy(&coordinator, &EarthOverlayCoordinator::meshAvailable);
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Mercator);

    Target stale = validTarget();
    stale.ir_r = 15000000.0;
    coordinator.request(stale, inRangeFields(), camera, 1600, 900);

    Target latest = validTarget();
    latest.ir_pos[0] = std::numeric_limits<double>::quiet_NaN();
    coordinator.request(latest, inRangeFields(), camera, 1600, 900);

    QTRY_COMPARE_WITH_TIMEOUT(readySpy.size(), 1, 5000);
    QCOMPARE(readySpy.at(0).at(0).toBool(), true);
    LarZoneMesh mesh;
    QVERIFY(coordinator.takePreparedMesh(&mesh));
    QVERIFY(mesh.inputRejected);

    readySpy.clear();
    coordinator.request(validTarget(), inRangeFields(), camera, 1600, 900);
    QTRY_COMPARE_WITH_TIMEOUT(readySpy.size(), 1, 5000);
    QVERIFY(coordinator.takePreparedMesh(&mesh));
    QVERIFY(!mesh.inputRejected);
    QVERIFY(!mesh.empty());
}

void ViewportWorkerTests::destructionWhilePreparationIsPendingIsSafe() {
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Sphere);
    Target target = validTarget();
    target.ir_r = 19000000.0;

    for (int iteration = 0; iteration < 20; ++iteration) {
        auto coordinator = std::make_unique<EarthOverlayCoordinator>();
        coordinator->request(target, inRangeFields(), camera, 2400, 1600);
        coordinator.reset();
    }
    QVERIFY(true);
}

QTEST_GUILESS_MAIN(ViewportWorkerTests)
#include "viewport_worker_tests.moc"
