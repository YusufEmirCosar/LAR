
#include "viewer/hud/dlz_hud_workspace.h"

#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace dlz::presentation {

HudWorkspace::HudWorkspace(QWidget *parent) : HudWorkspace(nullptr, parent) {}

HudWorkspace::HudWorkspace(ControlPanel *controlPanel, QWidget *parent)
    : QWidget(parent), m_inputs{}, m_calculationInputs{} {
    setObjectName(QStringLiteral("dlzHudWorkspace"));
    setMinimumSize(420, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_hudView = new HudView(this);
    m_controlPanel = controlPanel != nullptr ? controlPanel : new ControlPanel(this);
    layout->addWidget(m_hudView, 1);
    // MainWindow hosts the panel in its existing right-hand values column.
    // Keep a private fallback only for standalone workspace users and tests;
    // it must not float over the canvas when the workspace is shown.
    if (controlPanel == nullptr) {
        m_controlPanel->setVisible(false);
    }

    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_controlPanel, &ControlPanel::inputsChanged, this, &HudWorkspace::rebuildFromControls);
    connect(m_controlPanel, &ControlPanel::inputModeRequested, this, &HudWorkspace::setInputMode);
    connect(m_hudView, &HudView::frameRendered, this, &HudWorkspace::frameRendered);
    connect(m_timer, &QTimer::timeout, this, &HudWorkspace::tick);

    m_controlPanel->setInputs(m_calculationInputs);
    m_controlPanel->setInputMode(m_inputMode);
    m_controlPanel->setExternalInputs({}, false);
    m_clock.start();
    m_lastTickNanoseconds = m_clock.nsecsElapsed();
    m_controller.reset();
    rebuild(true);
    m_timer->start();
}

void HudWorkspace::setInputMode(ControlPanel::InputMode mode) {
    if (m_inputMode == mode) {
        m_controlPanel->setInputMode(mode);
        return;
    }
    m_inputMode = mode;
    m_controlPanel->setInputMode(mode);
    if (m_inputMode == ControlPanel::InputMode::SliderTest) {
        m_inputs = m_calculationInputs;
        rebuild(true);
        return;
    }
    if (!m_externalAvailable) {
        clearExternalInputs();
        return;
    }
    m_inputs = {m_externalInputs.rangeNm, m_externalInputs.aspectDegrees,
                m_externalInputs.altitudeFeet, 0.90, 0.95};
    rebuild(true);
}

void HudWorkspace::setExternalInputs(const dlz::TelemetryInputs &inputs, bool available,
                                     const QString &source) {
    m_externalInputs = inputs;
    m_externalAvailable = available;
    m_controlPanel->setExternalInputs(inputs, available, source);

    // Incoming frames are cached while the local calculation mode is active;
    // they must not alter the slider-driven drawing or its temporal state.
    if (m_inputMode != ControlPanel::InputMode::UdpOrReplay) {
        return;
    }
    if (!available) {
        clearExternalInputs();
        return;
    }
    m_inputs = {inputs.rangeNm, inputs.aspectDegrees, inputs.altitudeFeet, 0.90, 0.95};
    // A sequential external sample retains filter, scale, and cue state. The
    // controller is reset only when the source/mode changes or the stream is
    // cleared; resetTemporal is reserved for those discontinuities.
    rebuild(false);
}

void HudWorkspace::clearExternalInputs(const QString &message) {
    Q_UNUSED(message)
    m_externalAvailable = false;
    m_controlPanel->setExternalInputs({}, false);
    if (m_inputMode != ControlPanel::InputMode::UdpOrReplay) {
        return;
    }
    m_hasFrame = false;
    m_controller.clear();
    m_hudView->clearFrame();
}

void HudWorkspace::rebuildFromControls() {
    if (m_inputMode != ControlPanel::InputMode::SliderTest) {
        return;
    }
    m_calculationInputs = m_controlPanel->inputs();
    m_inputs = m_calculationInputs;
    // A slider edit is a new calculation, not a temporal sample of the old
    // one. Reset filtering, scale hysteresis, and cue timing together.
    rebuild(true);
}

void HudWorkspace::rebuild(bool resetTemporal) {
    if (resetTemporal) {
        m_controller.reset();
    }
    if (m_inputMode == ControlPanel::InputMode::UdpOrReplay && !m_externalAvailable) {
        m_hasFrame = false;
        m_controller.clear();
        m_hudView->clearFrame();
        return;
    }

    dlz::ScenarioFrame frame;
    QString error;
    if (!dlz::ScenarioAdapter::build(m_inputs, &frame, &error)) {
        // Keep finite geometry and candidate readouts live while withholding
        // an invalid solver result from the renderer.
        if (frame.geometry.rangeNm > 0.0) {
            m_controlPanel->setGeometryReadout(frame.geometry, frame.solution);
        }
        m_hasFrame = false;
        m_controller.clear();
        m_hudView->setError(
            error.isEmpty() ? QStringLiteral("DLZ calculation input is outside its ordered domain")
                            : error);
        return;
    }
    m_frame = frame;
    m_hasFrame = true;
    m_controlPanel->setGeometryReadout(m_frame.geometry, m_frame.solution);
    present(0.0);
}

void HudWorkspace::present(double dtSeconds) {
    if (!m_hasFrame) {
        return;
    }
    m_controller.update(m_frame.solution, m_frame.geometry.rangeNm, m_frame.geometry.rangeRateKnots,
                        dlz::HudMode::Prelaunch, dtSeconds);
    m_hudView->setFrame(m_frame.solution, m_controller.state(), m_controller.displayedRangeNm());
}

void HudWorkspace::tick() {
    const qint64 now = m_clock.nsecsElapsed();
    const double dt = static_cast<double>(now - m_lastTickNanoseconds) / 1.0e9;
    m_lastTickNanoseconds = now;
    present(std::clamp(dt, 0.0, 0.25));
}

} // namespace dlz::presentation
