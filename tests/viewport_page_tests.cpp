#include "viewer/viewport/earth_lar_view.h"
#include "viewer/viewport/grid_lar_view.h"
#include "viewer/viewport/lar_viewport.h"

#include "domain/statefield.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QThread>
#include <QtTest>

#include <memory>

namespace {

class FakePage : public QWidget, public ILarViewportPage {
  public:
    QWidget &widget() noexcept override {
        return *this;
    }
    LarViewportPageEvents &events() noexcept override {
        return pageEvents;
    }
    void setSceneState(const LarSceneState &state) override {
        scene = state;
        ++sceneUpdates;
    }
    void clearScene() override {
        scene = {};
        ++clearCount;
    }
    void setCameraState(const ViewportCameraState &state) override {
        camera = state;
        ++cameraUpdates;
    }
    void fitToData() override {
        ++fitCount;
    }

    LarViewportPageEvents pageEvents;
    LarSceneState scene;
    ViewportCameraState camera;
    int sceneUpdates = 0;
    int clearCount = 0;
    int cameraUpdates = 0;
    int fitCount = 0;
    int releaseEvents = 0;

  protected:
    void mousePressEvent(QMouseEvent *event) override {
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        ++releaseEvents;
        QWidget::mouseReleaseEvent(event);
    }
};

class FakeEarthPage final : public QWidget, public IEarthLarViewportPage {
  public:
    QWidget &widget() noexcept override {
        return *this;
    }
    LarViewportPageEvents &events() noexcept override {
        return pageEvents;
    }
    void setSceneState(const LarSceneState &state) override {
        scene = state;
        ++sceneUpdates;
    }
    void clearScene() override {
        scene = {};
        ++clearCount;
    }
    void setCameraState(const ViewportCameraState &state) override {
        camera = state;
        ++cameraUpdates;
    }
    void fitToData() override {
        ++fitCount;
    }
    bool ensureAvailable() override {
        ++availabilityChecks;
        return available;
    }
    void setEarthViewMode(LarViewMode mode) override {
        selectedMode = mode;
    }

    LarViewportPageEvents pageEvents;
    LarSceneState scene;
    ViewportCameraState camera;
    LarViewMode selectedMode = LarViewMode::Mercator;
    int sceneUpdates = 0;
    int clearCount = 0;
    int cameraUpdates = 0;
    int fitCount = 0;
    int availabilityChecks = 0;
    bool available = true;
};

void exercisePageContract(ILarViewportPage &page) {
    QCOMPARE(page.widget().thread(), QThread::currentThread());
    QCOMPARE(&page.widget(), &page.widget());

    ViewportCameraState camera;
    camera.mode = CameraTrackingMode::FollowTarget;
    camera.hasAnchor = true;
    camera.trackingActive = true;
    camera.anchorRadians = {0.4, 0.5, 0.0};
    camera.bearingRadians = 0.0;
    page.setCameraState(camera);

    LarSceneState scene;
    scene.hasScene = true;
    scene.availableFields = QBitArray(StateField::Count);
    scene.availableFields.setBit(StateField::Location0);
    scene.availableFields.setBit(StateField::Location1);
    scene.plane.location[0] = 0.4;
    scene.plane.location[1] = 0.5;
    page.setSceneState(scene);
    page.fitToData();
    page.clearScene();
    page.clearScene();
}

} // namespace

class ViewportPageTests final : public QObject {
    Q_OBJECT

  private slots:
    void gridHonorsPageContract();
    void earthHonorsPageContract();
    void nullInjectedPagesUseSafeDefaults();
    void hostUsesInjectedPages();
    void pagesConsumeLeftButtonRelease();
    void hostStopsIgnoredReleaseReentry();
};

void ViewportPageTests::gridHonorsPageContract() {
    GridLarView page;
    page.resize(500, 400);
    exercisePageContract(page);
}

void ViewportPageTests::earthHonorsPageContract() {
    EarthLarView page({});
    page.resize(500, 400);
    exercisePageContract(page);
}

void ViewportPageTests::nullInjectedPagesUseSafeDefaults() {
    LarViewport host(std::unique_ptr<ILarViewportPage>{}, std::unique_ptr<IEarthLarViewportPage>{});

    QCOMPARE(host.viewMode(), LarViewMode::Grid);
    host.clearState();
    host.fitToData();
}

void ViewportPageTests::hostUsesInjectedPages() {
    auto grid = std::make_unique<FakePage>();
    auto earth = std::make_unique<FakeEarthPage>();
    auto planePage = std::make_unique<FakePage>();
    FakePage *gridObserver = grid.get();
    FakeEarthPage *earthObserver = earth.get();
    FakePage *planeObserver = planePage.get();
    LarViewport host(std::move(grid), std::move(earth), std::move(planePage),
                     std::unique_ptr<QWidget>{});
    host.resize(500, 400);

    Plane plane{};
    Target target{};
    QBitArray fields(StateField::Count);
    fields.setBit(StateField::Location0);
    fields.setBit(StateField::Location1);
    host.setState(plane, target, fields);
    QCOMPARE(gridObserver->sceneUpdates, 1);
    QCOMPARE(earthObserver->sceneUpdates, 1);
    QCOMPARE(planeObserver->sceneUpdates, 1);
    QVERIFY(gridObserver->cameraUpdates >= 1);
    QVERIFY(earthObserver->cameraUpdates >= 1);

    host.setViewMode(LarViewMode::Sphere);
    QCOMPARE(earthObserver->availabilityChecks, 1);
    QCOMPARE(earthObserver->selectedMode, LarViewMode::Sphere);
    QVERIFY(earthObserver->fitCount >= 1);

    host.setContentMode(ViewportContentMode::Plane);
    QCOMPARE(host.contentMode(), ViewportContentMode::Plane);
    QVERIFY(!planeObserver->isHidden());
    QVERIFY(gridObserver->isHidden());
    QVERIFY(earthObserver->isHidden());
    QCOMPARE(host.viewMode(), LarViewMode::Sphere);

    host.clearState();
    QCOMPARE(gridObserver->clearCount, 1);
    QCOMPARE(earthObserver->clearCount, 1);
    QCOMPARE(planeObserver->clearCount, 1);
}

void ViewportPageTests::pagesConsumeLeftButtonRelease() {
    GridLarView grid;
    EarthLarView earth({});

    const auto releaseIsConsumed = [](QWidget &page) {
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(10.0, 10.0), QPointF(10.0, 10.0),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        release.setAccepted(false);
        QCoreApplication::sendEvent(&page, &release);
        return release.isAccepted();
    };

    QVERIFY(releaseIsConsumed(grid));
    QVERIFY(releaseIsConsumed(earth));
}

void ViewportPageTests::hostStopsIgnoredReleaseReentry() {
    auto grid = std::make_unique<FakePage>();
    auto earth = std::make_unique<FakeEarthPage>();
    FakePage *gridObserver = grid.get();
    LarViewport host(std::move(grid), std::move(earth));

    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(10.0, 10.0), QPointF(10.0, 10.0),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    release.setAccepted(false);
    QCoreApplication::sendEvent(&gridObserver->widget(), &release);

    QVERIFY(release.isAccepted());
    QCOMPARE(gridObserver->releaseEvents, 2);
}

QTEST_MAIN(ViewportPageTests)
#include "viewport_page_tests.moc"
