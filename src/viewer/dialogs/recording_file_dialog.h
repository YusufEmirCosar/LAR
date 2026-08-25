#pragma once

/**
 * @file recording_file_dialog.h
 * @brief Presentation port for recording persistence and discard choices.
 */

#include <QString>

enum class RecordingSaveKind {
    Snapshot,
    FinalSave,
};

/** User-choice boundary for recording persistence and destructive actions. */
class IRecordingFileDialog {
  public:
    virtual ~IRecordingFileDialog() = default;

    virtual QString chooseSavePath(RecordingSaveKind kind) = 0;
    virtual bool confirmReset() = 0;
    /**
     * @brief Returns whether the user accepts losing the recording while stopping online capture.
     *
     * @return True when the destructive stop should proceed; false otherwise.
     */
    virtual bool confirmDiscardOnStop() = 0;
    virtual bool confirmDiscardOnClose() = 0;
};
