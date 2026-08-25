#pragma once

/**
 * @file qt_viewer_file_dialog.h
 * @brief Qt Widgets adapter for mapping, policy, and session file selection.
 */

#include "viewer/dialogs/viewer_file_dialog.h"

class QWidget;

class QtViewerFileDialog final : public IViewerFileDialog {
  public:
    explicit QtViewerFileDialog(QWidget *parent) : m_parent(parent) {}

    QString chooseMappingPath() override;
    QString chooseWhitelistPath() override;
    QString chooseSessionPath() override;

  private:
    QWidget *m_parent = nullptr;
};
