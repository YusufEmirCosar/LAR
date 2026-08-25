#pragma once

/**
 * @file qt_recording_file_dialog.h
 * @brief Qt Widgets adapter for recording-save and discard decisions.
 */

#include "viewer/dialogs/recording_file_dialog.h"

class QWidget;

/** Native Qt implementation of recording-related user choices. */
class QtRecordingFileDialog final : public IRecordingFileDialog {
  public:
    explicit QtRecordingFileDialog(QWidget *parent) : m_parent(parent) {}

    QString chooseSavePath(RecordingSaveKind kind) override;
    bool confirmReset() override;
    bool confirmDiscardOnStop() override;
    bool confirmDiscardOnClose() override;

  private:
    QWidget *m_parent = nullptr;
};
