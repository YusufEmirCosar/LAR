
#include "viewer/panels/values_panel.h"

#include "application/application_view_model.h"
#include "domain/statefield.h"
#include "viewer/hud/dlz_control_panel.h"
#include "viewer/lar_geodesic_geometry.h"
#include "viewer/lar_projection.h"
#include "viewer/statevalueformatter.h"
#include "viewer/viewport/lar_zone_input_validator.h"

#include <QFontDatabase>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace {

enum class PlaneZoneStatus { Outside, InRange, InZone };

bool hasFields(const QBitArray &available, std::initializer_list<int> fields) noexcept {
    return std::all_of(fields.begin(), fields.end(), [&available](int field) {
        return field >= 0 && field < available.size() && available.testBit(field);
    });
}

bool validPlanePosition(double latitude, double longitude) noexcept {
    return std::isfinite(latitude) && std::isfinite(longitude) &&
           latitude >= -LarGeodesicGeometry::Pi * 0.5 &&
           latitude <= LarGeodesicGeometry::Pi * 0.5 && longitude >= -LarGeodesicGeometry::Pi &&
           longitude <= LarGeodesicGeometry::Pi;
}

QPointF planeWorldFor(const GeoCoordinateRadians &coordinate, const Plane &plane) noexcept {
    const double position[3]{coordinate.latitude, coordinate.longitude, 0.0};
    return LarProjection::geographicToPlaneWorld(position, plane.location, 0.0, plane.location[0],
                                                 true);
}

bool contains(const LarZoneDefinition &zone, const QPointF &zoneCenterOffset) noexcept {
    constexpr double BoundaryToleranceMeters = 1.0e-6;
    constexpr double BoundaryToleranceRadians = 1.0e-12;
    const double distance = std::hypot(zoneCenterOffset.x(), zoneCenterOffset.y());
    if (!std::isfinite(distance) || distance < zone.innerRadiusMeters - BoundaryToleranceMeters ||
        distance > zone.outerRadiusMeters + BoundaryToleranceMeters) {
        return false;
    }
    if (zone.spanRadians >= LarGeodesicGeometry::TwoPi - BoundaryToleranceRadians ||
        distance <= BoundaryToleranceMeters) {
        return true;
    }

    // The aircraft is the Plane scene origin; negate the center offset to get the
    // bearing from the zone center to the aircraft in the renderer's convention.
    const double bearing = std::atan2(-zoneCenterOffset.x(), -zoneCenterOffset.y());
    double bearingOffset =
        std::remainder(bearing - zone.startBearingRadians, LarGeodesicGeometry::TwoPi);
    if (bearingOffset < 0.0) {
        bearingOffset += LarGeodesicGeometry::TwoPi;
    }
    return bearingOffset <= zone.spanRadians + BoundaryToleranceRadians;
}

PlaneZoneStatus planeZoneStatus(const ApplicationViewModel &viewModel) noexcept {
    if (!viewModel.hasState() ||
        !hasFields(viewModel.availableFields(), {StateField::Location0, StateField::Location1}) ||
        !validPlanePosition(viewModel.plane().location[0], viewModel.plane().location[1])) {
        return PlaneZoneStatus::Outside;
    }

    // Plane mode uses this same local flat metric for its rings and sectors.
    const LarZoneValidationResult validation =
        LarZoneInputValidator().validate(viewModel.target(), viewModel.availableFields());
    if (validation.inZone &&
        contains(*validation.inZone, planeWorldFor(validation.inZone->center, viewModel.plane()))) {
        return PlaneZoneStatus::InZone;
    }
    if (validation.inRange &&
        contains(*validation.inRange,
                 planeWorldFor(validation.inRange->center, viewModel.plane()))) {
        return PlaneZoneStatus::InRange;
    }
    return PlaneZoneStatus::Outside;
}

void setPlaneZoneIndicator(QLabel *indicator, PlaneZoneStatus status) {
    QString text;
    QString background;
    QString border;
    switch (status) {
    case PlaneZoneStatus::InZone:
        text = QStringLiteral("IZ");
        background = QStringLiteral("#0e755b");
        border = QStringLiteral("#79d5b7");
        break;
    case PlaneZoneStatus::InRange:
        text = QStringLiteral("IR");
        background = QStringLiteral("#346d91");
        border = QStringLiteral("#8ac8e8");
        break;
    case PlaneZoneStatus::Outside:
        text = QStringLiteral("OUTSIDE");
        background = QStringLiteral("#6b3f47");
        border = QStringLiteral("#e0a3ad");
        break;
    }
    if (indicator->property("zoneStatus").toString() == text)
        return;
    indicator->setText(text);
    indicator->setToolTip(QStringLiteral("Plane area: %1").arg(text));
    indicator->setStyleSheet(
        QStringLiteral("QLabel#planeZoneIndicator { background-color: %1; color: #ffffff; "
                       "border: 1px solid %2; border-radius: 0px; font-weight: 700; }")
            .arg(background, border));
    indicator->setProperty("zoneStatus", text);
}

} // namespace

ValuesPanel::ValuesPanel(QWidget *parent) : QFrame(parent) {
    setObjectName(QStringLiteral("currentValuesColumn"));
    setMinimumWidth(330);
    setMaximumWidth(410);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_title = new QLabel(QStringLiteral("Plane Telemetry"));
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
    setContentMode(ViewportContentMode::Lar);
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

    m_planeZoneIndicator = new QLabel;
    m_planeZoneIndicator->setObjectName(QStringLiteral("planeZoneIndicator"));
    m_planeZoneIndicator->setAccessibleName(QStringLiteral("Plane zone status"));
    m_planeZoneIndicator->setFixedHeight(28);
    m_planeZoneIndicator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_planeZoneIndicator->setAlignment(Qt::AlignCenter);
    m_planeZoneIndicator->setVisible(false);
    setPlaneZoneIndicator(m_planeZoneIndicator, PlaneZoneStatus::Outside);
    grid->addWidget(m_planeZoneIndicator, row++, 0, 1, 2);
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
    if (m_planeZoneIndicator != nullptr) {
        m_planeZoneIndicator->setVisible(!hud);
    }
    if (hud) {
        m_title->setText(QStringLiteral("DLZ Values"));
    } else {
        m_title->setText(QStringLiteral("Plane Telemetry"));
    }
}

void ValuesPanel::render(const ApplicationViewModel &viewModel) {
    if (m_planeZoneIndicator != nullptr) {
        setPlaneZoneIndicator(m_planeZoneIndicator, planeZoneStatus(viewModel));
    }
    for (int id = 0; id < m_valueLabels.size(); ++id) {
        QLabel *label = m_valueLabels[id];
        if (!label)
            continue;
        const bool available = viewModel.hasState() && id < viewModel.availableFields().size() &&
                               viewModel.availableFields().testBit(id);
        if (!available) {
            if (label->text() != QStringLiteral("N/A"))
                label->setText(QStringLiteral("N/A"));
            continue;
        }
        const auto value = StateField::tryValue(viewModel.decodedState(), id);
        const QString text =
            value ? StateValueFormatter::format(id, *value) : QStringLiteral("N/A");
        if (label->text() != text)
            label->setText(text);
    }
}
