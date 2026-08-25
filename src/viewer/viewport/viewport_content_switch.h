#pragma once

/**
 * @file viewport_content_switch.h
 * @brief Accessible three-way widget for selecting viewport content.
 */

#include "viewer/viewport/viewport_content_mode.h"

#include <QWidget>

class QPushButton;

/** @brief Accessible LAR/Plane/DLZ content selector owned by the host. */
class ViewportContentSwitch final : public QWidget {
    Q_OBJECT

  public:
    explicit ViewportContentSwitch(QWidget *parent = nullptr);

    ViewportContentMode contentMode() const noexcept {
        return m_mode;
    }
    void setContentMode(ViewportContentMode mode);

  signals:
    void contentModeRequested(ViewportContentMode mode);

  private:
    QPushButton *m_larButton = nullptr;
    QPushButton *m_planeButton = nullptr;
    QPushButton *m_hudButton = nullptr;
    ViewportContentMode m_mode = ViewportContentMode::Lar;
};
