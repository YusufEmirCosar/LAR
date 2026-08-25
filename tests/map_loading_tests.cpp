#include "viewer/map/map_asset_source.h"
#include "viewer/viewport/earth_lar_view.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QThread>
#include <QThreadPool>
#include <QtTest>

#include <atomic>
#include <memory>

namespace {

std::shared_ptr<const lar::map::MapMesh> minimalMesh() {
    auto mesh = std::make_shared<lar::map::MapMesh>();
    mesh->vertices = {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F};
    mesh->mercatorFillIndices = {0U, 1U, 2U};
    mesh->sphereFillIndices = {0U, 1U, 2U};
    mesh->borderIndices = {0U, 1U};
    return mesh;
}

class DelayedAssetSource final : public lar::map::IMapAssetSource {
  public:
    explicit DelayedAssetSource(int delayMilliseconds, bool fail = false)
        : m_delayMilliseconds(delayMilliseconds), m_fail(fail) {}

    [[nodiscard]] lar::map::MapAssetReadResult load() const override {
        ++loadCount;
        QThread::msleep(static_cast<unsigned long>(m_delayMilliseconds));
        if (m_fail) {
            return {nullptr, lar::map::MapAssetError::Integrity, QStringLiteral("fixture failure")};
        }
        return {minimalMesh(), lar::map::MapAssetError::None, {}};
    }

    mutable std::atomic_int loadCount{0};

  private:
    int m_delayMilliseconds = 0;
    bool m_fail = false;
};

class ThrowingAssetSource final : public lar::map::IMapAssetSource {
  public:
    [[nodiscard]] lar::map::MapAssetReadResult load() const override {
        throw 7;
    }
};

} // namespace

class MapLoadingTests final : public QObject {
    Q_OBJECT

  private slots:
    void selectionDoesNotBlockGuiThread();
    void repeatedRequestsRemainSingleFlight();
    void failureUsesTypedFallbackSignal();
    void sourceExceptionUsesTypedFallbackSignal();
    void deferredRendererFailureUsesTypedFallbackSignal();
    void destructionDuringLoadIsSafe();
    void cleanupTestCase();
};

void MapLoadingTests::selectionDoesNotBlockGuiThread() {
    auto source = std::make_shared<DelayedAssetSource>(150);
    EarthLarView viewport(source);
    QElapsedTimer timer;
    timer.start();
    QVERIFY(viewport.ensureMapLoaded());
    QVERIFY2(timer.elapsed() < 50, "Map selection blocked while the package was being read.");
    QTRY_VERIFY_WITH_TIMEOUT(viewport.isMapLoaded(), 2000);
    QCOMPARE(source->loadCount.load(), 1);
}

void MapLoadingTests::repeatedRequestsRemainSingleFlight() {
    auto source = std::make_shared<DelayedAssetSource>(100);
    EarthLarView viewport(source);
    for (int request = 0; request < 8; ++request) {
        QVERIFY(viewport.ensureMapLoaded());
    }
    QTRY_VERIFY_WITH_TIMEOUT(viewport.isMapLoaded(), 2000);
    QCOMPARE(source->loadCount.load(), 1);
}

void MapLoadingTests::failureUsesTypedFallbackSignal() {
    auto source = std::make_shared<DelayedAssetSource>(10, true);
    EarthLarView viewport(source);
    QSignalSpy fallback(&viewport, &EarthLarView::mapUnavailable);
    QVERIFY(viewport.ensureMapLoaded());
    QTRY_COMPARE_WITH_TIMEOUT(fallback.size(), 1, 2000);
    QVERIFY(!viewport.isMapLoaded());
    QVERIFY(!viewport.ensureMapLoaded());
    QCOMPARE(source->loadCount.load(), 1);
}

void MapLoadingTests::sourceExceptionUsesTypedFallbackSignal() {
    auto source = std::make_shared<ThrowingAssetSource>();
    EarthLarView viewport(source);
    QSignalSpy fallback(&viewport, &EarthLarView::mapUnavailable);

    QVERIFY(viewport.ensureMapLoaded());
    QTRY_COMPARE_WITH_TIMEOUT(fallback.size(), 1, 2000);
    QVERIFY(!viewport.isMapLoaded());
}

void MapLoadingTests::deferredRendererFailureUsesTypedFallbackSignal() {
    auto source = std::make_shared<DelayedAssetSource>(0);
    EarthLarView viewport(source);
    QSignalSpy fallback(&viewport, &EarthLarView::mapUnavailable);

    QVERIFY(viewport.ensureMapLoaded());
    QTRY_VERIFY_WITH_TIMEOUT(viewport.isMapLoaded(), 2000);
    QVERIFY(QMetaObject::invokeMethod(&viewport, "rendererError", Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("deferred upload failure"))));
    QCOMPARE(fallback.size(), 1);
    QVERIFY(!viewport.isMapLoaded());
    QVERIFY(!viewport.ensureMapLoaded());
}

void MapLoadingTests::destructionDuringLoadIsSafe() {
    auto source = std::make_shared<DelayedAssetSource>(100);
    auto viewport = std::make_unique<EarthLarView>(source);
    QVERIFY(viewport->ensureMapLoaded());
    viewport.reset();
    QTest::qWait(180);
    QCoreApplication::processEvents();
    QCOMPARE(source->loadCount.load(), 1);
}

void MapLoadingTests::cleanupTestCase() {
    QThreadPool::globalInstance()->waitForDone();
}

QTEST_MAIN(MapLoadingTests)

#include "map_loading_tests.moc"
