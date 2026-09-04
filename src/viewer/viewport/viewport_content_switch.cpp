
#include "viewer/viewport/viewport_content_switch.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>

ViewportContentSwitch::ViewportContentSwitch(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("viewportContentSwitch"));
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_larButton = new QPushButton(QStringLiteral("LAR"), this);
    m_planeButton = new QPushButton(QStringLiteral("PLANE"), this);
    m_hudButton = new QPushButton(QStringLiteral("DLZ"), this);
    m_larButton->setObjectName(QStringLiteral("viewportLarContentButton"));
    m_planeButton->setObjectName(QStringLiteral("viewportPlaneContentButton"));
    m_hudButton->setObjectName(QStringLiteral("viewportHudContentButton"));
    m_larButton->setCheckable(true);
    m_planeButton->setCheckable(true);
    m_hudButton->setCheckable(true);
    m_larButton->setAccessibleName(QStringLiteral("LAR content"));
    m_planeButton->setAccessibleName(QStringLiteral("Plane simulation content"));
    m_hudButton->setAccessibleName(QStringLiteral("DLZ content"));
    m_larButton->setToolTip(QStringLiteral("Show the Grid, Mercator, or Sphere LAR view"));
    m_planeButton->setToolTip(QStringLiteral("Show the centered F-16 simulation"));
    m_hudButton->setToolTip(QStringLiteral("Show the Dynamic Launch Zone (DLZ)"));

    setStyleSheet(QStringLiteral(R"(
        QWidget#viewportContentSwitch {
            background: #f5f8f6;
            border: 1px solid #789087;
            border-radius: 7px;
        }
        QWidget#viewportContentSwitch QPushButton {
            min-width: 42px;
            min-height: 26px;
            padding: 2px 9px;
            border: 0;
            background: transparent;
            color: #263c31;
        }
        QWidget#viewportContentSwitch QPushButton#viewportLarContentButton {
            border-top-left-radius: 6px;
            border-bottom-left-radius: 6px;
        }
        QWidget#viewportContentSwitch QPushButton#viewportHudContentButton {
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
        }
        QWidget#viewportContentSwitch QPushButton:checked {
            background: #c9e6d8;
            color: #154b3a;
            font-weight: 600;
        }
        QWidget#viewportContentSwitch QPushButton:checked:hover {
            background: #b9dccb;
        }
        QWidget#viewportContentSwitch QPushButton:checked:pressed {
            background: #a9d2bf;
        }
        QWidget#viewportContentSwitch QPushButton:focus {
            outline: 2px solid #357461;
            outline-offset: -2px;
        }
    )"));

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(m_larButton, static_cast<int>(ViewportContentMode::Lar));
    group->addButton(m_planeButton, static_cast<int>(ViewportContentMode::Plane));
    group->addButton(m_hudButton, static_cast<int>(ViewportContentMode::Hud));
    layout->addWidget(m_larButton);
    layout->addWidget(m_planeButton);
    layout->addWidget(m_hudButton);
    connect(group, &QButtonGroup::idClicked, this, [this](int id) {
        const auto mode = static_cast<ViewportContentMode>(id);
        setContentMode(mode);
        emit contentModeRequested(mode);
    });
    setContentMode(ViewportContentMode::Lar);
}

void ViewportContentSwitch::setContentMode(ViewportContentMode mode) {
    m_mode = mode;
    const QSignalBlocker larBlocker(m_larButton);
    const QSignalBlocker planeBlocker(m_planeButton);
    const QSignalBlocker hudBlocker(m_hudButton);
    m_larButton->setChecked(mode == ViewportContentMode::Lar);
    m_planeButton->setChecked(mode == ViewportContentMode::Plane);
    m_hudButton->setChecked(mode == ViewportContentMode::Hud);
}
