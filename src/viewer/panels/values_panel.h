#pragma once

/**
 * @file values_panel.h
 * @brief Right-hand presentation panel for decoded values and DLZ controls.
 */

#include "viewer/viewport/viewport_content_mode.h"

#include <QFrame>
#include <QVector>

class ApplicationViewModel;
class QLabel;
class QStackedWidget;

namespace dlz::presentation {
class ControlPanel;
}

/** Owns construction and rendering of the right-hand values/HUD column. */
class ValuesPanel final : public QFrame {
  public:
    explicit ValuesPanel(QWidget *parent = nullptr);

    dlz::presentation::ControlPanel *hudControlPanel() const noexcept {
        return m_hudControlPanel;
    }

    void setContentMode(ViewportContentMode mode);
    void render(const ApplicationViewModel &viewModel);

  private:
    QWidget *buildCurrentValues();

    QLabel *m_title = nullptr;
    QStackedWidget *m_stack = nullptr;
    dlz::presentation::ControlPanel *m_hudControlPanel = nullptr;
    QVector<QLabel *> m_valueLabels;
};
