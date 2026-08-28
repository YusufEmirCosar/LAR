#include "viewer/hud/dlz_control_panel.h"
#include "viewer/hud/dlz_hud_workspace.h"
#include "viewer/viewport/lar_viewport.h"
#include "viewer/viewport/lar_viewport_page.h"
#include "viewer/viewport/viewport_content_switch.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QStackedWidget>
#include <QStringList>
#include <QTimer>
#include <QtTest>

#include <memory>

namespace {

class FakePage final : public QWidget, public ILarViewportPage {
  public:
    QWidget &widget() noexcept override {
        return *this;
    }
    LarViewportPageEvents &events() noexcept override {
        return m_events;
    }
    void setSceneState(const LarSceneState &state) override {
        Q_UNUSED(state);
    }
    void clearScene() override {}
    void setCameraState(const ViewportCameraState &camera) override {
        Q_UNUSED(camera);
    }
    void fitToData() override {}

  private:
    LarViewportPageEvents m_events;
};

class FakeEarthPage final : public QWidget, public IEarthLarViewportPage {
  public:
    QWidget &widget() noexcept override {
        return *this;
    }
    LarViewportPageEvents &events() noexcept override {
        return m_events;
    }
    void setSceneState(const LarSceneState &state) override {
        Q_UNUSED(state);
    }
    void clearScene() override {}
    void setCameraState(const ViewportCameraState &camera) override {
        Q_UNUSED(camera);
    }
    void fitToData() override {}
    bool ensureAvailable() override {
        return true;
    }
    void setEarthViewMode(LarViewMode mode) override {
        Q_UNUSED(mode);
    }

  private:
    LarViewportPageEvents m_events;
};

} // namespace

class DlzViewTests final : public QObject {
    Q_OBJECT

  private slots:
    void hudWorkspaceExposesControlsAndView();
    void hudInputChangesResetTemporalPresentation();
    void invalidToyInputKeepsReadoutsAndShowsCanvasError();
    void externalTelemetryDoesNotOverrideSliderDrawing();
    void hostSwitchesContentWithoutChangingLarProjection();
};

void DlzViewTests::hudWorkspaceExposesControlsAndView() {
    dlz::presentation::HudWorkspace workspace;
    workspace.resize(800, 500);
    workspace.show();
    QCoreApplication::processEvents();
    QVERIFY(workspace.findChild<QWidget *>(QStringLiteral("dlzHudView")) != nullptr);
    QVERIFY(workspace.findChild<QWidget *>(QStringLiteral("dlzControlPanel")) != nullptr);
    auto *range = workspace.findChild<QSlider *>(QStringLiteral("dlzRangeSlider"));
    auto *aspect = workspace.findChild<QSlider *>(QStringLiteral("dlzAspectSlider"));
    auto *altitude = workspace.findChild<QSlider *>(QStringLiteral("dlzAltitudeSlider"));
    QVERIFY(range && aspect && altitude);
    auto *inputStack = workspace.findChild<QStackedWidget *>(QStringLiteral("dlzInputStack"));
    QVERIFY(inputStack);
    QCOMPARE(inputStack->currentIndex(), 0);
    workspace.setInputMode(dlz::presentation::ControlPanel::InputMode::SliderTest);
    QCoreApplication::processEvents();
    QCOMPARE(inputStack->currentIndex(), 1);
    QCOMPARE(workspace.inputMode(), dlz::presentation::ControlPanel::InputMode::SliderTest);
}

void DlzViewTests::hudInputChangesResetTemporalPresentation() {
    dlz::presentation::HudWorkspace workspace;
    workspace.resize(800, 500);
    workspace.show();
    QCoreApplication::processEvents();
    workspace.setInputMode(dlz::presentation::ControlPanel::InputMode::SliderTest);
    QCoreApplication::processEvents();

    auto *altitude = workspace.findChild<QSlider *>(QStringLiteral("dlzAltitudeSlider"));
    QVERIFY(altitude);
    QCOMPARE(workspace.hudView()->hudState().scaleMaximumNm, 40.0);

    altitude->setValue(0);
    QCoreApplication::processEvents();
    QCOMPARE(workspace.hudView()->hudState().scaleMaximumNm, 20.0);

    altitude->setValue(300);
    QCoreApplication::processEvents();
    QCOMPARE(workspace.hudView()->hudState().scaleMaximumNm, 40.0);
}

void DlzViewTests::invalidToyInputKeepsReadoutsAndShowsCanvasError() {
    dlz::presentation::HudWorkspace workspace;
    workspace.resize(800, 500);
    workspace.show();
    QCoreApplication::processEvents();
    workspace.setInputMode(dlz::presentation::ControlPanel::InputMode::SliderTest);
    QCoreApplication::processEvents();

    auto *aspect = workspace.findChild<QSlider *>(QStringLiteral("dlzAspectSlider"));
    auto *aspectValue = workspace.findChild<QLabel *>(QStringLiteral("dlzAspectValue"));
    QVERIFY(aspect);
    QVERIFY(aspectValue);

    const auto readoutTexts = [&workspace] {
        QStringList values;
        for (const auto *label :
             workspace.controlPanel()->findChildren<QLabel *>(QStringLiteral("currentValue"))) {
            values.append(label->text());
        }
        return values;
    };
    const QStringList validReadouts = readoutTexts();

    aspect->setValue(180);
    QCoreApplication::processEvents();
    QVERIFY(workspace.hudView()->hasError());
    QVERIFY(!workspace.hudView()->hasFrame());
    QVERIFY(
        workspace.hudView()->errorText().contains(QStringLiteral("outside its ordered domain")));
    QCOMPARE(aspectValue->text(), QStringLiteral("180°"));
    const QStringList invalidReadouts = readoutTexts();
    QVERIFY(invalidReadouts.contains(QStringLiteral("9.7 nm")));
    QVERIFY(invalidReadouts != validReadouts);

    QTest::qWait(100);
    QCoreApplication::processEvents();
    QVERIFY(workspace.hudView()->hasError());

    aspect->setValue(0);
    QCoreApplication::processEvents();
    QVERIFY(!workspace.hudView()->hasError());
    QVERIFY(workspace.hudView()->hasFrame());
}

void DlzViewTests::externalTelemetryDoesNotOverrideSliderDrawing() {
    dlz::presentation::HudWorkspace workspace;
    workspace.resize(800, 500);
    workspace.show();
    QCoreApplication::processEvents();

    workspace.setExternalInputs({12.0, 45.0, 22000.0}, true, QStringLiteral("test UDP frame"));
    QCoreApplication::processEvents();
    QCOMPARE(workspace.inputMode(), dlz::presentation::ControlPanel::InputMode::UdpOrReplay);
    QCOMPARE(workspace.hudView()->hudState().currentRangeNm, 12.0);

    workspace.setInputMode(dlz::presentation::ControlPanel::InputMode::SliderTest);
    auto *range = workspace.findChild<QSlider *>(QStringLiteral("dlzRangeSlider"));
    QVERIFY(range);
    range->setValue(200);
    QCoreApplication::processEvents();
    QCOMPARE(workspace.hudView()->hudState().currentRangeNm, 20.0);

    workspace.setExternalInputs({60.0, 0.0, 30000.0}, true, QStringLiteral("new UDP frame"));
    QCoreApplication::processEvents();
    QCOMPARE(workspace.hudView()->hudState().currentRangeNm, 20.0);

    workspace.setInputMode(dlz::presentation::ControlPanel::InputMode::UdpOrReplay);
    QCoreApplication::processEvents();
    QCOMPARE(workspace.hudView()->hudState().currentRangeNm, 60.0);
}

void DlzViewTests::hostSwitchesContentWithoutChangingLarProjection() {
    LarViewport host(std::make_unique<FakePage>(), std::make_unique<FakeEarthPage>(),
                     std::make_unique<dlz::presentation::HudWorkspace>());
    host.resize(800, 500);
    host.show();
    QCoreApplication::processEvents();
    QCOMPARE(host.contentMode(), ViewportContentMode::Lar);
    auto *hud =
        host.findChild<dlz::presentation::HudWorkspace *>(QStringLiteral("dlzHudWorkspace"));
    QVERIFY(hud);
    auto *hudTimer = hud->findChild<QTimer *>(QString{}, Qt::FindDirectChildrenOnly);
    QVERIFY(hudTimer);
    QVERIFY(!hudTimer->isActive());
    host.setDlzInputs({12.0, 45.0, 22000.0}, true, QStringLiteral("cached frame"));
    QCOMPARE(hud->scenarioInputs().rangeNm, 20.0);
    host.setViewMode(LarViewMode::Grid);
    host.setContentMode(ViewportContentMode::Hud);
    QCOMPARE(host.contentMode(), ViewportContentMode::Hud);
    QVERIFY(hud->isVisible());
    QVERIFY(hudTimer->isActive());
    QCOMPARE(hud->scenarioInputs().rangeNm, 12.0);
    auto *rail = host.findChild<QWidget *>(QStringLiteral("viewportControlRail"));
    QVERIFY(!rail || !rail->isVisible());
    auto *hudButton = host.findChild<QPushButton *>(QStringLiteral("viewportHudContentButton"));
    QVERIFY(hudButton);
    host.setContentMode(ViewportContentMode::Lar);
    QVERIFY(!hudTimer->isActive());
    host.setDlzInputs({60.0, 0.0, 30000.0}, true, QStringLiteral("new cached frame"));
    QCOMPARE(hud->scenarioInputs().rangeNm, 12.0);
    QSignalSpy contentSpy(&host, &LarViewport::contentModeChanged);
    QTest::mouseClick(hudButton, Qt::LeftButton);
    QCOMPARE(contentSpy.count(), 1);
    QCOMPARE(host.contentMode(), ViewportContentMode::Hud);
    QVERIFY(hudButton->isChecked());
    QCOMPARE(hud->scenarioInputs().rangeNm, 60.0);
    host.setContentMode(ViewportContentMode::Lar);
    QCOMPARE(host.contentMode(), ViewportContentMode::Lar);
    QCOMPARE(host.viewMode(), LarViewMode::Grid);
    QVERIFY(!rail || rail->isVisible());
}

QTEST_MAIN(DlzViewTests)

#include "dlz_view_tests.moc"
