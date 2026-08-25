#include "application/application_facade.h"
#include "domain/dlz/dlz_scenario_adapter.h"
#include "infrastructure/runtime/threaded_application_runtime.h"
#include "viewer/hud/dlz_fixtures.h"
#include "viewer/hud/dlz_hud_view.h"
#include "viewer/mainwindow.h"

#include <QApplication>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QtMath>

int main(int argc, char **argv) {
    QApplication application(argc, argv);
    if (argc < 2)
        return 2;

    ModeCoordinator modes;
    ApplicationViewModel viewModel;
    ThreadedApplicationRuntime runtime;
    ApplicationFacade facade(modes, runtime, viewModel);

    MainWindow window(facade);
    window.resize(1160, 720);
    window.show();

    const QString outputPath = QString::fromLocal8Bit(argv[1]);
    QString presetName;
    bool offline = false;
    for (int index = 2; index < argc; ++index) {
        const QString option = QString::fromLocal8Bit(argv[index]).trimmed().toLower();
        if (option == QStringLiteral("offline")) {
            offline = true;
        } else if (!option.isEmpty()) {
            presetName = option;
        }
    }

    QTimer::singleShot(150, &application, [&] {
        bool configurationValid = true;
        if (offline) {
            auto *button = window.findChild<QPushButton *>(QStringLiteral("offlineMode"));
            if (button != nullptr)
                button->click();
            application.processEvents();
        }
        if (!presetName.isEmpty()) {
            const bool planeMode = presetName == QStringLiteral("plane") ||
                                   presetName == QStringLiteral("plane-skybox");
            const bool fixtureMode = presetName == QStringLiteral("fixture-a") ||
                                     presetName == QStringLiteral("fixture-b");
            if (planeMode) {
                auto *planeButton =
                    window.findChild<QPushButton *>(QStringLiteral("viewportPlaneContentButton"));
                if (planeButton == nullptr) {
                    configurationValid = false;
                } else {
                    planeButton->click();
                    application.processEvents();
                }
            } else if (fixtureMode) {
                auto *hudButton =
                    window.findChild<QPushButton *>(QStringLiteral("viewportHudContentButton"));
                auto *hudView =
                    window.findChild<dlz::presentation::HudView *>(QStringLiteral("dlzHudView"));
                if (hudButton == nullptr || hudView == nullptr) {
                    configurationValid = false;
                } else {
                    hudButton->click();
                    const auto fixture = presetName == QStringLiteral("fixture-a")
                                             ? dlz::presentation::fixtureA()
                                             : dlz::presentation::fixtureB();
                    hudView->setFrame(fixture.solution, fixture.hudState, fixture.displayedRangeNm);
                    application.processEvents();
                }
            } else {
                const auto inputsForName = [](const QString &name) {
                    if (name == QStringLiteral("head-on") || name == QStringLiteral("headon")) {
                        return dlz::ScenarioInputs{20.0, 0.0, 30000.0, 0.90, 0.95};
                    }
                    if (name == QStringLiteral("beam")) {
                        return dlz::ScenarioInputs{20.0, 90.0, 30000.0, 0.90, 0.95};
                    }
                    if (name == QStringLiteral("tail") || name == QStringLiteral("tail-chase")) {
                        return dlz::ScenarioInputs{13.0, 180.0, 55000.0, 1.00, 0.95};
                    }
                    if (name == QStringLiteral("minimum") || name == QStringLiteral("rmin")) {
                        return dlz::ScenarioInputs{1.2, 0.0, 30000.0, 0.90, 0.95};
                    }
                    if (name == QStringLiteral("far")) {
                        return dlz::ScenarioInputs{60.0, 0.0, 30000.0, 0.90, 0.95};
                    }
                    if (name == QStringLiteral("sweep") || name == QStringLiteral("aspect-sweep")) {
                        return dlz::ScenarioInputs{50.0, 90.0, 55000.0, 1.00, 0.95};
                    }
                    return dlz::ScenarioInputs{};
                };
                const auto requestedInputs = inputsForName(presetName);
                const bool recognized =
                    presetName == QStringLiteral("head-on") ||
                    presetName == QStringLiteral("headon") ||
                    presetName == QStringLiteral("beam") || presetName == QStringLiteral("tail") ||
                    presetName == QStringLiteral("tail-chase") ||
                    presetName == QStringLiteral("minimum") ||
                    presetName == QStringLiteral("rmin") || presetName == QStringLiteral("far") ||
                    presetName == QStringLiteral("sweep") ||
                    presetName == QStringLiteral("aspect-sweep");
                auto *hudButton =
                    window.findChild<QPushButton *>(QStringLiteral("viewportHudContentButton"));
                auto *sliderButton =
                    window.findChild<QPushButton *>(QStringLiteral("dlzSliderTestButton"));
                auto *rangeSlider = window.findChild<QSlider *>(QStringLiteral("dlzRangeSlider"));
                auto *aspectSlider = window.findChild<QSlider *>(QStringLiteral("dlzAspectSlider"));
                auto *altitudeSlider =
                    window.findChild<QSlider *>(QStringLiteral("dlzAltitudeSlider"));
                if (!recognized || hudButton == nullptr || sliderButton == nullptr ||
                    rangeSlider == nullptr || aspectSlider == nullptr ||
                    altitudeSlider == nullptr) {
                    configurationValid = false;
                } else {
                    hudButton->click();
                    sliderButton->click();
                    rangeSlider->setValue(qRound(requestedInputs.rangeNm * 10.0));
                    aspectSlider->setValue(qRound(requestedInputs.aspectDegrees));
                    altitudeSlider->setValue(qRound(requestedInputs.altitudeFeet / 100.0));
                    application.processEvents();
                }
            }
        }
        const bool saved = configurationValid && window.grab().save(outputPath);
        application.exit(saved ? 0 : 3);
    });

    return application.exec();
}
