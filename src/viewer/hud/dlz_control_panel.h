#pragma once

/**
 * @file dlz_control_panel.h
 * @brief Editable controls and diagnostics for the DLZ HUD workspace.
 */

#include "domain/dlz/dlz_scenario_adapter.h"

#include <QMetaType>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;
class QSlider;

namespace dlz::presentation {

/** @brief Small interactive control surface for the mandatory toy model. */
class ControlPanel final : public QWidget {
    Q_OBJECT

  public:
    explicit ControlPanel(QWidget *parent = nullptr);

    /** @brief Selects whether DLZ geometry comes from transport or local test controls. */
    enum class InputMode { UdpOrReplay, SliderTest };

    dlz::ScenarioInputs inputs() const;
    void setInputs(const dlz::ScenarioInputs &inputs);
    void setInputMode(InputMode mode);
    void setExternalInputs(const dlz::TelemetryInputs &inputs, bool available,
                           const QString &source = {});
    InputMode inputMode() const noexcept {
        return m_inputMode;
    }
    void setGeometryReadout(const dlz::Geometry &geometry, const dlz::Solution &solution);

  signals:
    void inputsChanged();
    void inputModeRequested(InputMode mode);

  private:
    void refreshLabels();

    QPushButton *m_udpReplayButton = nullptr;
    QPushButton *m_sliderTestButton = nullptr;
    QStackedWidget *m_inputStack = nullptr;
    QWidget *m_externalInputPage = nullptr;
    QWidget *m_sliderInputPage = nullptr;
    QSlider *m_rangeSlider = nullptr;
    QSlider *m_aspectSlider = nullptr;
    QSlider *m_altitudeSlider = nullptr;
    QLabel *m_rangeValue = nullptr;
    QLabel *m_aspectValue = nullptr;
    QLabel *m_altitudeValue = nullptr;
    QLabel *m_externalRangeValue = nullptr;
    QLabel *m_externalAspectValue = nullptr;
    QLabel *m_externalAltitudeValue = nullptr;
    QLabel *m_calculatedRange = nullptr;
    QLabel *m_rangeRate = nullptr;
    QLabel *m_calculatedAspect = nullptr;
    QLabel *m_rmaxValue = nullptr;
    QLabel *m_rpiValue = nullptr;
    QLabel *m_rneValue = nullptr;
    QLabel *m_rtrValue = nullptr;
    QLabel *m_rminValue = nullptr;
    QLabel *m_timeOfFlightValue = nullptr;
    QLabel *m_cueValue = nullptr;
    InputMode m_inputMode = InputMode::UdpOrReplay;
};

} // namespace dlz::presentation

Q_DECLARE_METATYPE(dlz::presentation::ControlPanel::InputMode)
