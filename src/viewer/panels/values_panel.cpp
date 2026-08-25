
#include "viewer/panels/values_panel.h"

#include "application/application_view_model.h"
#include "domain/statefield.h"
#include "viewer/hud/dlz_control_panel.h"
#include "viewer/statevalueformatter.h"

#include <QFontDatabase>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

ValuesPanel::ValuesPanel(QWidget *parent) : QFrame(parent) {
    setObjectName(QStringLiteral("currentValuesColumn"));
    setMinimumWidth(330);
    setMaximumWidth(410);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_title = new QLabel(QStringLiteral("Current Values"));
    m_title->setObjectName(QStringLiteral("columnTitle"));
    m_title->setContentsMargins(10, 8, 10, 8);

    auto *valuesScroll = new QScrollArea;
    valuesScroll->setObjectName(QStringLiteral("currentValuesScroll"));
    valuesScroll->setWidget(buildCurrentValues());
    valuesScroll->setWidgetResizable(true);
    valuesScroll->setFrameShape(QFrame::NoFrame);

    auto *hudContent = new QWidget;
    hudContent->setObjectName(QStringLiteral("hudValuesContent"));
    auto *hudLayout = new QVBoxLayout(hudContent);
    hudLayout->setContentsMargins(12, 10, 12, 12);
    hudLayout->setSpacing(0);
    m_hudControlPanel = new dlz::presentation::ControlPanel(hudContent);
    hudLayout->addWidget(m_hudControlPanel);
    hudLayout->addStretch(1);

    auto *hudScroll = new QScrollArea;
    hudScroll->setObjectName(QStringLiteral("hudValuesScroll"));
    hudScroll->setWidget(hudContent);
    hudScroll->setWidgetResizable(true);
    hudScroll->setFrameShape(QFrame::NoFrame);

    m_stack = new QStackedWidget;
    m_stack->setObjectName(QStringLiteral("currentValuesStack"));
    m_stack->addWidget(valuesScroll);
    m_stack->addWidget(hudScroll);
    layout->addWidget(m_title);
    layout->addWidget(m_stack, 1);
}

QWidget *ValuesPanel::buildCurrentValues() {
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("currentValuesContent"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(0, 1);
    grid->setColumnMinimumWidth(1, 132);

    int row = 0;
    const auto addEntityHeader = [&](const QString &text) {
        auto *header = new QLabel(text);
        header->setObjectName(QStringLiteral("entityHeader"));
        header->setFixedHeight(36);
        header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(header, row++, 0, 1, 2);
    };
    const auto addGroupHeader = [&](const QString &text) {
        auto *header = new QLabel(text);
        header->setObjectName(QStringLiteral("valueGroupHeader"));
        header->setFixedHeight(30);
        header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(header, row++, 0, 1, 2);
    };

    QFont valueFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    valueFont.setStyleHint(QFont::Monospace);
    valueFont.setFixedPitch(true);
    m_valueLabels.fill(nullptr, StateField::Count);
    const auto addValue = [&](int id) {
        auto *name = new QLabel(StateField::presentationName(id));
        name->setObjectName(QStringLiteral("valueName"));
        name->setFixedHeight(28);
        name->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        auto *value = new QLabel(QStringLiteral("N/A"));
        value->setObjectName(QStringLiteral("currentValue"));
        value->setProperty("fieldId", id);
        value->setFont(valueFont);
        value->setFixedHeight(28);
        value->setMinimumWidth(132);
        value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_valueLabels[id] = value;
        grid->addWidget(name, row, 0);
        grid->addWidget(value, row++, 1);
    };
    const auto addDivider = [&] {
        auto *divider = new QFrame;
        divider->setObjectName(QStringLiteral("entityDivider"));
        divider->setFrameShape(QFrame::HLine);
        grid->addWidget(divider, row++, 0, 1, 2);
    };

    addEntityHeader(QStringLiteral("Plane Data"));
    addGroupHeader(QStringLiteral("Location"));
    for (int id : {StateField::Location0, StateField::Location1, StateField::Location2})
        addValue(id);
    addGroupHeader(QStringLiteral("Euler (Rotation)"));
    for (int id : {StateField::Euler0, StateField::Euler1, StateField::Euler2})
        addValue(id);
    addGroupHeader(QStringLiteral("Velocity"));
    for (int id : {StateField::Velocity0, StateField::Velocity1, StateField::Velocity2})
        addValue(id);

    addDivider();
    addEntityHeader(QStringLiteral("Target Data"));
    addGroupHeader(QStringLiteral("In-Zone Center"));
    for (int id : {StateField::IzPos0, StateField::IzPos1, StateField::IzPos2})
        addValue(id);
    addGroupHeader(QStringLiteral("In-Range Center"));
    for (int id : {StateField::IrPos0, StateField::IrPos1, StateField::IrPos2})
        addValue(id);
    addGroupHeader(QStringLiteral("In-Zone Angles"));
    addValue(StateField::IzTheta1);
    addValue(StateField::IzTheta2);
    addGroupHeader(QStringLiteral("In-Zone Range"));
    addValue(StateField::IzR1);
    addValue(StateField::IzR2);
    addGroupHeader(QStringLiteral("In-Range"));
    addValue(StateField::IrR);
    addGroupHeader(QStringLiteral("Packet"));
    addValue(StateField::Time);

    addDivider();
    addEntityHeader(QStringLiteral("DLZ Data"));
    addGroupHeader(QStringLiteral("Telemetry"));
    addValue(StateField::DlzRangeNm);
    addValue(StateField::DlzAspectDegrees);
    addValue(StateField::DlzAltitudeFeet);

    layout->addLayout(grid);
    layout->addStretch();
    return content;
}

void ValuesPanel::setContentMode(ViewportContentMode mode) {
    const bool hud = mode == ViewportContentMode::Hud;
    m_stack->setCurrentIndex(hud ? 1 : 0);
    if (hud) {
        m_title->setText(QStringLiteral("DLZ Values"));
    } else if (mode == ViewportContentMode::Plane) {
        m_title->setText(QStringLiteral("Plane Telemetry"));
    } else {
        m_title->setText(QStringLiteral("Current Values"));
    }
}

void ValuesPanel::render(const ApplicationViewModel &viewModel) {
    for (int id = 0; id < m_valueLabels.size(); ++id) {
        QLabel *label = m_valueLabels[id];
        if (!label)
            continue;
        const bool available = viewModel.hasState() && id < viewModel.availableFields().size() &&
                               viewModel.availableFields().testBit(id);
        if (!available) {
            label->setText(QStringLiteral("N/A"));
            continue;
        }
        const auto value = StateField::tryValue(viewModel.decodedState(), id);
        label->setText(value ? StateValueFormatter::format(id, *value) : QStringLiteral("N/A"));
    }
}
