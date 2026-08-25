#pragma once

/**
 * @file dlz_hud_workspace.h
 * @brief Coordinator joining DLZ controls, domain calculations, and HUD timing.
 */

#include "domain/dlz/dlz_scenario_adapter.h"
#include "viewer/hud/dlz_control_panel.h"
#include "viewer/hud/dlz_hud_view.h"
#include "viewer/hud/dlz_presentation_controller.h"

#include <QElapsedTimer>
#include <QWidget>

class QTimer;

namespace dlz::presentation {

/** @brief Hosted HUD page containing the DLZ canvas and its control binding. */
class HudWorkspace final : public QWidget {
    Q_OBJECT

  public:
    explicit HudWorkspace(QWidget *parent = nullptr);
    explicit HudWorkspace(ControlPanel *controlPanel, QWidget *parent = nullptr);

    HudView *hudView() const noexcept {
        return m_hudView;
    }
    ControlPanel *controlPanel() const noexcept {
        return m_controlPanel;
    }
    const dlz::ScenarioInputs &scenarioInputs() const noexcept {
        return m_inputs;
    }
    ControlPanel::InputMode inputMode() const noexcept {
        return m_inputMode;
    }

  public slots:
    void setInputMode(ControlPanel::InputMode mode);
    void setExternalInputs(const dlz::TelemetryInputs &inputs, bool available,
                           const QString &source = {});
    void clearExternalInputs(const QString &message = {});

  signals:
    void frameRendered();
    void diagnosticRaised(const QString &message);

  private slots:
    void rebuildFromControls();
    void tick();

  private:
    void rebuild(bool resetTemporal);
    void present(double dtSeconds);

    HudView *m_hudView = nullptr;
    ControlPanel *m_controlPanel = nullptr;
    QTimer *m_timer = nullptr;
    QElapsedTimer m_clock;
    qint64 m_lastTickNanoseconds = 0;
    dlz::ScenarioInputs m_inputs;
    dlz::ScenarioInputs m_calculationInputs;
    dlz::TelemetryInputs m_externalInputs;
    dlz::ScenarioFrame m_frame;
    PresentationController m_controller;
    ControlPanel::InputMode m_inputMode = ControlPanel::InputMode::UdpOrReplay;
    bool m_externalAvailable = false;
    bool m_hasFrame = false;
};

} // namespace dlz::presentation
