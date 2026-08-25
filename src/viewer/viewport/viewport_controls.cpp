
#include "viewer/viewport/viewport_controls.h"

#include "domain/statefield.h"
#include "viewer/lar_geodesic_geometry.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVariant>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

template <typename Enum> Enum selectedValue(const QComboBox &selector, Enum fallback) {
    const QVariant value = selector.currentData();
    return value.isValid() ? static_cast<Enum>(value.toInt()) : fallback;
}

template <typename Enum> void selectValue(QComboBox &selector, Enum value) {
    const int index = selector.findData(static_cast<int>(value));
    if (index < 0 || index == selector.currentIndex()) {
        return;
    }
    const QSignalBlocker blocker(&selector);
    selector.setCurrentIndex(index);
}

} // namespace

ViewportControls::ViewportControls(QWidget *parent)
    : QGroupBox(QStringLiteral("Viewport"), parent) {
    setObjectName(QStringLiteral("viewportControlRail"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_StyledBackground, false);
    if (auto *host = qobject_cast<QWidget *>(parent)) {
        host->installEventFilter(this);
        setGeometry(host->rect());
        raise();
    }
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(0);

    m_viewModeSelector = new QComboBox(this);
    m_viewModeSelector->setObjectName(QStringLiteral("viewportModeSelector"));
    m_viewModeSelector->setAccessibleName(QStringLiteral("Viewport projection"));
    m_viewModeSelector->addItem(QStringLiteral("Grid map"), static_cast<int>(LarViewMode::Grid));
    m_viewModeSelector->addItem(QStringLiteral("Mercator"),
                                static_cast<int>(LarViewMode::Mercator));
    m_viewModeSelector->addItem(QStringLiteral("Sphere"), static_cast<int>(LarViewMode::Sphere));

    m_cameraTrackingSelector = new QComboBox(this);
    m_cameraTrackingSelector->setObjectName(QStringLiteral("cameraTrackingSelector"));
    m_cameraTrackingSelector->setAccessibleName(QStringLiteral("Camera tracking mode"));
    m_cameraTrackingSelector->addItem(QStringLiteral("Follow plane"),
                                      static_cast<int>(CameraTrackingMode::FollowPlane));
    m_cameraTrackingSelector->addItem(QStringLiteral("Follow target"),
                                      static_cast<int>(CameraTrackingMode::FollowTarget));
    m_cameraTrackingSelector->addItem(QStringLiteral("Free movement"),
                                      static_cast<int>(CameraTrackingMode::Free));

    m_turnWithPlaneCheckBox = new QCheckBox(QStringLiteral("Turn with plane"), this);
    m_turnWithPlaneCheckBox->setObjectName(QStringLiteral("turnWithPlaneCheckBox"));
    m_turnWithPlaneCheckBox->setChecked(true);

    // Keep the typed selectors as an accessibility/test/API surface, while
    // the visible rail uses compact segmented buttons.
    m_viewModeSelector->setVisible(false);
    m_cameraTrackingSelector->setVisible(false);
    m_turnWithPlaneCheckBox->setVisible(false);

    auto *projectionFrame = new QFrame;
    projectionFrame->setObjectName(QStringLiteral("viewportProjectionPanel"));
    projectionFrame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *projectionLayout = new QVBoxLayout(projectionFrame);
    projectionLayout->setContentsMargins(4, 4, 4, 4);
    projectionLayout->setSpacing(0);
    m_gridButton = new QPushButton(QStringLiteral("Grid"));
    m_mercatorButton = new QPushButton(QStringLiteral("Mercator"));
    m_sphereButton = new QPushButton(QStringLiteral("Sphere"));
    m_gridButton->setObjectName(QStringLiteral("viewportGridButton"));
    m_mercatorButton->setObjectName(QStringLiteral("viewportMercatorButton"));
    m_sphereButton->setObjectName(QStringLiteral("viewportSphereButton"));
    for (QPushButton *button : {m_gridButton, m_mercatorButton, m_sphereButton}) {
        button->setCheckable(true);
        button->setMinimumWidth(62);
        projectionLayout->addWidget(button);
    }
    auto *projectionGroup = new QButtonGroup(this);
    projectionGroup->setExclusive(true);
    projectionGroup->addButton(m_gridButton, int(LarViewMode::Grid));
    projectionGroup->addButton(m_mercatorButton, int(LarViewMode::Mercator));
    projectionGroup->addButton(m_sphereButton, int(LarViewMode::Sphere));

    m_navigationReadout = new QLabel(QStringLiteral("Distance —   Direction —"));
    m_navigationReadout->setObjectName(QStringLiteral("viewportNavigationReadout"));
    m_navigationReadout->setAlignment(Qt::AlignCenter);
    m_navigationReadout->setMinimumWidth(190);
    m_navigationReadout->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_navigationReadout->setVisible(false);

    auto *cameraFrame = new QFrame;
    cameraFrame->setObjectName(QStringLiteral("viewportCameraPanel"));
    cameraFrame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *cameraLayout = new QVBoxLayout(cameraFrame);
    cameraLayout->setContentsMargins(4, 4, 4, 4);
    cameraLayout->setSpacing(0);
    m_planeButton = new QPushButton(QStringLiteral("Plane"));
    m_targetButton = new QPushButton(QStringLiteral("Target"));
    m_freeButton = new QPushButton(QStringLiteral("Free"));
    m_planeButton->setObjectName(QStringLiteral("viewportPlaneButton"));
    m_targetButton->setObjectName(QStringLiteral("viewportTargetButton"));
    m_freeButton->setObjectName(QStringLiteral("viewportFreeButton"));
    for (QPushButton *button : {m_planeButton, m_targetButton, m_freeButton}) {
        button->setCheckable(true);
        button->setMinimumWidth(62);
        cameraLayout->addWidget(button);
    }
    auto *cameraGroup = new QButtonGroup(this);
    cameraGroup->setExclusive(true);
    cameraGroup->addButton(m_planeButton, int(CameraTrackingMode::FollowPlane));
    cameraGroup->addButton(m_targetButton, int(CameraTrackingMode::FollowTarget));
    cameraGroup->addButton(m_freeButton, int(CameraTrackingMode::Free));

    auto *bottomLayout = new QHBoxLayout;
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);
    bottomLayout->addWidget(projectionFrame, 0, Qt::AlignLeft | Qt::AlignBottom);
    bottomLayout->addStretch(1);
    bottomLayout->addWidget(cameraFrame, 0, Qt::AlignRight | Qt::AlignBottom);
    layout->addStretch(1);
    layout->addLayout(bottomLayout);

    connect(projectionGroup, &QButtonGroup::idClicked, this, [this](int id) {
        const auto mode = static_cast<LarViewMode>(id);
        setViewMode(mode);
        emit viewModeRequested(mode);
    });
    connect(cameraGroup, &QButtonGroup::idClicked, this, [this](int id) {
        const auto mode = static_cast<CameraTrackingMode>(id);
        if (mode == CameraTrackingMode::FollowPlane &&
            cameraTrackingMode() == CameraTrackingMode::FollowPlane) {
            if (!m_yawAvailable) {
                setTurnWithPlane(false);
                emit turnWithPlaneRequested(false);
                return;
            }
            const bool enabled = !turnWithPlane();
            setTurnWithPlane(enabled);
            emit turnWithPlaneRequested(enabled);
            return;
        }
        if (mode == CameraTrackingMode::FollowPlane) {
            // The first Plane click is deliberately north-up; a
            // second click toggles turning with the aircraft.
            setTurnWithPlane(false);
            emit turnWithPlaneRequested(false);
        }
        setCameraTrackingMode(mode);
        emit cameraTrackingModeRequested(mode);
    });

    connect(m_viewModeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { emit viewModeRequested(viewMode()); });
    connect(m_cameraTrackingSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] {
                updateTurnWithPlanePresentation(m_yawAvailable);
                emit cameraTrackingModeRequested(cameraTrackingMode());
            });
    connect(m_turnWithPlaneCheckBox, &QCheckBox::toggled, this,
            &ViewportControls::turnWithPlaneRequested);

    updateTurnWithPlanePresentation(true);
    setViewMode(LarViewMode::Grid);
    setCameraTrackingMode(CameraTrackingMode::FollowPlane);
}

bool ViewportControls::eventFilter(QObject *watched, QEvent *event) {
    if (watched == parentWidget() && event != nullptr && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        raise();
    }
    return QGroupBox::eventFilter(watched, event);
}

LarViewMode ViewportControls::viewMode() const {
    return selectedValue(*m_viewModeSelector, LarViewMode::Grid);
}

CameraTrackingMode ViewportControls::cameraTrackingMode() const {
    return selectedValue(*m_cameraTrackingSelector, CameraTrackingMode::FollowPlane);
}

bool ViewportControls::turnWithPlane() const {
    return m_turnWithPlaneCheckBox->isChecked();
}

void ViewportControls::setViewMode(LarViewMode mode) {
    selectValue(*m_viewModeSelector, mode);
    if (m_gridButton != nullptr) {
        const QSignalBlocker gridBlocker(m_gridButton);
        const QSignalBlocker mercatorBlocker(m_mercatorButton);
        const QSignalBlocker sphereBlocker(m_sphereButton);
        m_gridButton->setChecked(mode == LarViewMode::Grid);
        m_mercatorButton->setChecked(mode == LarViewMode::Mercator);
        m_sphereButton->setChecked(mode == LarViewMode::Sphere);
    }
}

void ViewportControls::setCameraTrackingMode(CameraTrackingMode mode) {
    selectValue(*m_cameraTrackingSelector, mode);
    updateTurnWithPlanePresentation(m_yawAvailable);
    if (m_planeButton != nullptr) {
        const QSignalBlocker planeBlocker(m_planeButton);
        const QSignalBlocker targetBlocker(m_targetButton);
        const QSignalBlocker freeBlocker(m_freeButton);
        m_planeButton->setChecked(mode == CameraTrackingMode::FollowPlane);
        m_targetButton->setChecked(mode == CameraTrackingMode::FollowTarget);
        m_freeButton->setChecked(mode == CameraTrackingMode::Free);
    }
}

void ViewportControls::setTurnWithPlane(bool enabled) {
    if (!m_yawAvailable) {
        enabled = false;
    }
    if (m_turnWithPlaneCheckBox->isChecked() == enabled) {
        updateTurnWithPlanePresentation(m_yawAvailable);
        return;
    }
    const QSignalBlocker blocker(m_turnWithPlaneCheckBox);
    m_turnWithPlaneCheckBox->setChecked(enabled);
    updateTurnWithPlanePresentation(m_yawAvailable);
}

void ViewportControls::setTurnWithPlaneAvailability(bool yawAvailable) {
    m_yawAvailable = yawAvailable;
    if (!m_yawAvailable) {
        setTurnWithPlane(false);
    }
    updateTurnWithPlanePresentation(yawAvailable);
}

void ViewportControls::updateTurnWithPlanePresentation(bool yawAvailable) {
    const bool followsPlane = cameraTrackingMode() == CameraTrackingMode::FollowPlane;
    m_turnWithPlaneCheckBox->setEnabled(followsPlane && yawAvailable);
    if (!followsPlane) {
        m_turnWithPlaneCheckBox->setToolTip(
            QStringLiteral("Available only while following the plane"));
    } else if (!yawAvailable) {
        m_turnWithPlaneCheckBox->setToolTip(
            QStringLiteral("Aircraft yaw is unavailable; the camera is north-up"));
    } else {
        m_turnWithPlaneCheckBox->setToolTip(
            QStringLiteral("Rotate the viewport so aircraft forward remains up"));
    }
    if (m_planeButton != nullptr) {
        const bool turning = followsPlane && yawAvailable && turnWithPlane();
        m_planeButton->setProperty("turning", turning);
        m_planeButton->setAccessibleName(turning ? QStringLiteral("Plane, turning with aircraft")
                                                 : QStringLiteral("Plane, north-up"));
        m_planeButton->setToolTip(
            turning ? QStringLiteral("Plane, turning with aircraft; click to switch north-up")
                    : QStringLiteral("Plane, north-up; click to turn with aircraft"));
        m_planeButton->style()->unpolish(m_planeButton);
        m_planeButton->style()->polish(m_planeButton);
        m_planeButton->update();
    }
}

void ViewportControls::setNavigationReadout(const Plane &plane, const Target &target,
                                            const QBitArray &availableFields) {
    if (m_navigationReadout == nullptr)
        return;
    const auto has = [&availableFields](int field) {
        return field >= 0 && field < availableFields.size() && availableFields.testBit(field);
    };
    const bool hasIz = has(StateField::IzPos0) && has(StateField::IzPos1);
    const bool hasIr = has(StateField::IrPos0) && has(StateField::IrPos1);
    if (!has(StateField::Location0) || !has(StateField::Location1) || (!hasIz && !hasIr)) {
        m_navigationReadout->setText(QStringLiteral("Distance —   Direction —"));
        return;
    }
    const double latitude = plane.location[0];
    const double longitude = plane.location[1];
    const double targetLatitude = hasIz ? target.iz_pos[0] : target.ir_pos[0];
    const double targetLongitude = hasIz ? target.iz_pos[1] : target.ir_pos[1];
    if (!std::isfinite(latitude) || !std::isfinite(longitude) || !std::isfinite(targetLatitude) ||
        !std::isfinite(targetLongitude)) {
        m_navigationReadout->setText(QStringLiteral("Distance —   Direction —"));
        return;
    }
    const double deltaLatitude = targetLatitude - latitude;
    const double deltaLongitude = LarGeodesicGeometry::wrapLongitude(targetLongitude - longitude);
    const double a = std::sin(deltaLatitude * 0.5) * std::sin(deltaLatitude * 0.5) +
                     std::cos(latitude) * std::cos(targetLatitude) *
                         std::sin(deltaLongitude * 0.5) * std::sin(deltaLongitude * 0.5);
    const double distance =
        LarGeodesicGeometry::EarthRadiusMeters * 2.0 *
        std::atan2(std::sqrt(std::max(0.0, a)), std::sqrt(std::max(0.0, 1.0 - a)));
    const double bearing =
        std::atan2(std::sin(deltaLongitude) * std::cos(targetLatitude),
                   std::cos(latitude) * std::sin(targetLatitude) -
                       std::sin(latitude) * std::cos(targetLatitude) * std::cos(deltaLongitude));
    double bearingDegrees = std::fmod(qRadiansToDegrees(bearing) + 360.0, 360.0);
    m_navigationReadout->setText(
        QStringLiteral("Distance %1   Direction %2°")
            .arg(distance >= 1000.0 ? QStringLiteral("%1 km").arg(distance / 1000.0, 0, 'f', 1)
                                    : QStringLiteral("%1 m").arg(distance, 0, 'f', 0))
            .arg(bearingDegrees, 0, 'f', 0));
}
