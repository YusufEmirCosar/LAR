
#include "viewer/hud/dlz_control_panel.h"

#include "domain/dlz/dlz_units.h"

#include <QButtonGroup>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace dlz::presentation {

ControlPanel::ControlPanel(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("dlzControlPanel"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(0, 1);
    grid->setColumnMinimumWidth(1, 132);

    QFont valueFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    valueFont.setStyleHint(QFont::Monospace);
    valueFont.setFixedPitch(true);

    int row = 0;
    const auto makeNameLabel = [this](const QString &text) {
        auto *label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("valueName"));
        label->setFixedHeight(28);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return label;
    };
    const auto makeValueLabel = [this, &valueFont](const QString &text) {
        auto *label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("currentValue"));
        label->setProperty("fieldId", -1);
        label->setFont(valueFont);
        label->setFixedHeight(28);
        label->setMinimumWidth(132);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return label;
    };
    const auto addEntityHeader = [&](const QString &text) {
        auto *header = new QLabel(text, this);
        header->setObjectName(QStringLiteral("entityHeader"));
        header->setFixedHeight(36);
        header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(header, row++, 0, 1, 2);
    };
    const auto addGroupHeader = [&](const QString &text) {
        auto *header = new QLabel(text, this);
        header->setObjectName(QStringLiteral("valueGroupHeader"));
        header->setFixedHeight(30);
        header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(header, row++, 0, 1, 2);
    };
    const auto addDivider = [&] {
        auto *divider = new QFrame(this);
        divider->setObjectName(QStringLiteral("entityDivider"));
        divider->setFrameShape(QFrame::HLine);
        grid->addWidget(divider, row++, 0, 1, 2);
    };
    const auto addValueRow = [&](const QString &name, QLabel *value) {
        grid->addWidget(makeNameLabel(name), row, 0);
        grid->addWidget(value, row++, 1);
    };

    auto *buttonRow = new QWidget(this);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0);
    m_udpReplayButton = new QPushButton(QStringLiteral("UDP / Offline Replay"), buttonRow);
    m_sliderTestButton = new QPushButton(QStringLiteral("Calculation Test"), buttonRow);
    m_udpReplayButton->setObjectName(QStringLiteral("dlzUdpReplayButton"));
    m_sliderTestButton->setObjectName(QStringLiteral("dlzSliderTestButton"));
    m_udpReplayButton->setCheckable(true);
    m_sliderTestButton->setCheckable(true);
    m_udpReplayButton->setToolTip(
        QStringLiteral("Draw DLZ from UDP packets or offline replay frames"));
    m_sliderTestButton->setToolTip(
        QStringLiteral("Draw DLZ only from the Calculation Test sliders"));
    auto *modeGroup = new QButtonGroup(buttonRow);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_udpReplayButton, int(InputMode::UdpOrReplay));
    modeGroup->addButton(m_sliderTestButton, int(InputMode::SliderTest));
    buttonLayout->addWidget(m_udpReplayButton, 1);
    buttonLayout->addWidget(m_sliderTestButton, 1);
    grid->addWidget(buttonRow, row++, 0, 1, 2);
    addDivider();

    addEntityHeader(QStringLiteral("DLZ Inputs"));
    addGroupHeader(QStringLiteral("Engagement Geometry"));

    m_inputStack = new QStackedWidget(this);
    m_inputStack->setObjectName(QStringLiteral("dlzInputStack"));

    m_externalInputPage = new QWidget(m_inputStack);
    m_externalInputPage->setObjectName(QStringLiteral("dlzExternalInputPage"));
    auto *externalGrid = new QGridLayout(m_externalInputPage);
    externalGrid->setContentsMargins(0, 0, 0, 0);
    externalGrid->setHorizontalSpacing(12);
    externalGrid->setVerticalSpacing(2);
    const auto makeExternalValue = [this, &valueFont] {
        auto *label = new QLabel(QStringLiteral("N/A"), m_externalInputPage);
        label->setObjectName(QStringLiteral("dlzExternalValue"));
        label->setFont(valueFont);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setMinimumWidth(132);
        return label;
    };
    m_externalRangeValue = makeExternalValue();
    m_externalAspectValue = makeExternalValue();
    m_externalAltitudeValue = makeExternalValue();
    externalGrid->addWidget(makeNameLabel(QStringLiteral("Range")), 0, 0);
    externalGrid->addWidget(m_externalRangeValue, 0, 1);
    externalGrid->addWidget(makeNameLabel(QStringLiteral("Aspect")), 1, 0);
    externalGrid->addWidget(m_externalAspectValue, 1, 1);
    externalGrid->addWidget(makeNameLabel(QStringLiteral("Altitude")), 2, 0);
    externalGrid->addWidget(m_externalAltitudeValue, 2, 1);

    m_sliderInputPage = new QWidget(m_inputStack);
    m_sliderInputPage->setObjectName(QStringLiteral("dlzSliderInputPage"));
    auto *sliderGrid = new QGridLayout(m_sliderInputPage);
    sliderGrid->setContentsMargins(0, 0, 0, 0);
    sliderGrid->setHorizontalSpacing(12);
    sliderGrid->setVerticalSpacing(4);
    const auto addSliderRow = [&](int sliderRow, const QString &name, QSlider *slider,
                                  QLabel *value) {
        auto *cell = new QWidget(m_sliderInputPage);
        cell->setObjectName(QStringLiteral("dlzSliderCell"));
        auto *cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(0);
        slider->setFixedHeight(20);
        value->setFixedHeight(20);
        cellLayout->addWidget(slider);
        cellLayout->addWidget(value);
        cell->setFixedHeight(44);
        sliderGrid->addWidget(makeNameLabel(name), sliderRow, 0);
        sliderGrid->addWidget(cell, sliderRow, 1);
    };

    m_rangeSlider = new QSlider(Qt::Horizontal, m_sliderInputPage);
    m_rangeSlider->setObjectName(QStringLiteral("dlzRangeSlider"));
    m_rangeSlider->setRange(1, 600);
    m_rangeSlider->setValue(200);
    m_rangeValue = new QLabel(m_sliderInputPage);
    m_rangeValue->setObjectName(QStringLiteral("dlzRangeValue"));
    m_rangeValue->setFont(valueFont);
    m_rangeValue->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    addSliderRow(0, QStringLiteral("Range"), m_rangeSlider, m_rangeValue);

    m_aspectSlider = new QSlider(Qt::Horizontal, m_sliderInputPage);
    m_aspectSlider->setObjectName(QStringLiteral("dlzAspectSlider"));
    m_aspectSlider->setRange(0, 180);
    m_aspectSlider->setValue(0);
    m_aspectValue = new QLabel(m_sliderInputPage);
    m_aspectValue->setObjectName(QStringLiteral("dlzAspectValue"));
    m_aspectValue->setFont(valueFont);
    m_aspectValue->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    addSliderRow(1, QStringLiteral("Aspect"), m_aspectSlider, m_aspectValue);

    m_altitudeSlider = new QSlider(Qt::Horizontal, m_sliderInputPage);
    m_altitudeSlider->setObjectName(QStringLiteral("dlzAltitudeSlider"));
    m_altitudeSlider->setRange(0, 600);
    m_altitudeSlider->setValue(300);
    m_altitudeValue = new QLabel(m_sliderInputPage);
    m_altitudeValue->setObjectName(QStringLiteral("dlzAltitudeValue"));
    m_altitudeValue->setFont(valueFont);
    m_altitudeValue->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    addSliderRow(2, QStringLiteral("Altitude"), m_altitudeSlider, m_altitudeValue);

    m_inputStack->addWidget(m_externalInputPage);
    m_inputStack->addWidget(m_sliderInputPage);
    grid->addWidget(m_inputStack, row++, 0, 1, 2);
    addDivider();

    addEntityHeader(QStringLiteral("Calculated DLZ"));
    addGroupHeader(QStringLiteral("Geometry"));
    m_calculatedRange = makeValueLabel(QStringLiteral("N/A"));
    addValueRow(QStringLiteral("Current Range"), m_calculatedRange);
    m_rangeRate = makeValueLabel(QStringLiteral("N/A"));
    addValueRow(QStringLiteral("Range Rate"), m_rangeRate);
    m_calculatedAspect = makeValueLabel(QStringLiteral("N/A"));
    addValueRow(QStringLiteral("Aspect"), m_calculatedAspect);

    addGroupHeader(QStringLiteral("Launch Zone"));
    m_rmaxValue = makeValueLabel(QStringLiteral("N/A"));
    addValueRow(QStringLiteral("RMAX"), m_rmaxValue);
    m_rpiValue = makeValueLabel(QStringLiteral("N/A"));
    addValueRow(QStringLiteral("RPI"), m_rpiValue);
    m_rneValue = makeValueLabel(QStringLiteral("N/A"));
    addValueRow(QStringLiteral("RNE"), m_rneValue);
    m_rtrValue = makeValueLabel(QStringLiteral("N/A"));
    addValueRow(QStringLiteral("RTR"), m_rtrValue);
    m_rminValue = makeValueLabel(QStringLiteral("N/A"));
    addValueRow(QStringLiteral("RMIN"), m_rminValue);

    addGroupHeader(QStringLiteral("Timing / Cue"));
    m_timeOfFlightValue = makeValueLabel(QStringLiteral("N/A"));
    addValueRow(QStringLiteral("Time of Flight"), m_timeOfFlightValue);
    m_cueValue = makeValueLabel(QStringLiteral("N/A"));
    addValueRow(QStringLiteral("Shoot Cue"), m_cueValue);

    layout->addLayout(grid);
    layout->addStretch(1);

    connect(modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        const auto mode = static_cast<InputMode>(id);
        setInputMode(mode);
        emit inputModeRequested(mode);
    });
    const auto sliderChanged = [this] {
        refreshLabels();
        emit inputsChanged();
    };
    connect(m_rangeSlider, &QSlider::valueChanged, this, sliderChanged);
    connect(m_aspectSlider, &QSlider::valueChanged, this, sliderChanged);
    connect(m_altitudeSlider, &QSlider::valueChanged, this, sliderChanged);

    setInputMode(InputMode::UdpOrReplay);
    refreshLabels();

    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(R"(
        QWidget#dlzControlPanel {
            background-color: #ffffff;
            color: #27322d;
        }
        QWidget#dlzSliderCell, QWidget#dlzExternalInputPage,
        QWidget#dlzSliderInputPage { background-color: #ffffff; }
        QWidget#dlzControlPanel QLabel#dlzRangeValue,
        QWidget#dlzControlPanel QLabel#dlzAspectValue,
        QWidget#dlzControlPanel QLabel#dlzAltitudeValue,
        QWidget#dlzControlPanel QLabel#dlzExternalValue {
            background-color: #ffffff;
            color: #18251f;
        }
    )"));
}

dlz::ScenarioInputs ControlPanel::inputs() const {
    return {m_rangeSlider->value() / 10.0, static_cast<double>(m_aspectSlider->value()),
            m_altitudeSlider->value() * 100.0, 0.90, 0.95};
}

void ControlPanel::setInputMode(InputMode mode) {
    m_inputMode = mode;
    const bool sliderMode = mode == InputMode::SliderTest;
    {
        const QSignalBlocker udpBlocker(m_udpReplayButton);
        const QSignalBlocker sliderBlocker(m_sliderTestButton);
        m_udpReplayButton->setChecked(!sliderMode);
        m_sliderTestButton->setChecked(sliderMode);
    }
    m_inputStack->setCurrentIndex(sliderMode ? 1 : 0);
}

void ControlPanel::setExternalInputs(const dlz::TelemetryInputs &inputs, bool available,
                                     const QString &source) {
    Q_UNUSED(source)
    if (!available) {
        m_externalRangeValue->setText(QStringLiteral("N/A"));
        m_externalAspectValue->setText(QStringLiteral("N/A"));
        m_externalAltitudeValue->setText(QStringLiteral("N/A"));
        return;
    }
    m_externalRangeValue->setText(QStringLiteral("%1 nm").arg(inputs.rangeNm, 0, 'f', 1));
    m_externalAspectValue->setText(QStringLiteral("%1°").arg(inputs.aspectDegrees, 0, 'f', 0));
    m_externalAltitudeValue->setText(
        QStringLiteral("%1 kft").arg(inputs.altitudeFeet / 1000.0, 0, 'f', 1));
}

void ControlPanel::setInputs(const dlz::ScenarioInputs &inputs) {
    const QSignalBlocker rangeBlocker(m_rangeSlider);
    const QSignalBlocker aspectBlocker(m_aspectSlider);
    const QSignalBlocker altitudeBlocker(m_altitudeSlider);
    m_rangeSlider->setValue(qBound(1, qRound(inputs.rangeNm * 10.0), 600));
    m_aspectSlider->setValue(qBound(0, qRound(inputs.aspectDegrees), 180));
    m_altitudeSlider->setValue(qBound(0, qRound(inputs.altitudeFeet / 100.0), 600));
    refreshLabels();
}

void ControlPanel::setGeometryReadout(const dlz::Geometry &geometry,
                                      const dlz::Solution &solution) {
    m_calculatedRange->setText(QStringLiteral("%1 nm").arg(geometry.rangeNm, 0, 'f', 1));
    m_rangeRate->setText(QStringLiteral("%1 kt").arg(geometry.rangeRateKnots, 0, 'f', 0));
    m_calculatedAspect->setText(QStringLiteral("%1°").arg(
        geometry.aspectRadians * dlz::units::DegreesPerRadian, 0, 'f', 0));
    m_rmaxValue->setText(
        QStringLiteral("%1 nm").arg(solution.aerodynamicMaximumRangeNm, 0, 'f', 1));
    m_rpiValue->setText(QStringLiteral("%1 nm").arg(solution.interceptRangeNm, 0, 'f', 1));
    m_rneValue->setText(QStringLiteral("%1 nm").arg(solution.noEscapeRangeNm, 0, 'f', 1));
    m_rtrValue->setText(QStringLiteral("%1 nm").arg(solution.turnAndRunRangeNm, 0, 'f', 1));
    m_rminValue->setText(QStringLiteral("%1 nm").arg(solution.minimumRangeNm, 0, 'f', 1));
    m_timeOfFlightValue->setText(
        QStringLiteral("%1 s").arg(solution.timeOfFlightSeconds, 0, 'f', 1));
    m_cueValue->setText(solution.shootCue ? QStringLiteral("SHOOT") : QStringLiteral("NO SHOOT"));
}

void ControlPanel::refreshLabels() {
    m_rangeValue->setText(QStringLiteral("%1 nm").arg(m_rangeSlider->value() / 10.0, 0, 'f', 1));
    m_aspectValue->setText(QStringLiteral("%1°").arg(m_aspectSlider->value()));
    m_altitudeValue->setText(
        QStringLiteral("%1 kft").arg(m_altitudeSlider->value() / 10.0, 0, 'f', 1));
}

} // namespace dlz::presentation
