#pragma once

/**
 * @file recording_workflow_controller.h
 * @brief Presentation workflow for snapshot, final-save, reset, and close choices.
 */

#include "viewer/dialogs/recording_file_dialog.h"

#include <QObject>

class ApplicationFacade;

/** Owns recording dialog policy while the application owns transaction state. */
class RecordingWorkflowController final : public QObject {
    Q_OBJECT

  public:
    RecordingWorkflowController(ApplicationFacade &application, IRecordingFileDialog &dialog,
                                QObject *parent = nullptr);

    bool prepareToClose();
    /**
     * @brief Confirms and prepares stopping online capture.
     *
     * @details An empty recording is discarded without prompting. A non-empty recording
     * requires confirmation before it is discarded and the online source is stopped.
     *
     * @return True when stopping is accepted or queued; false when the user cancels or
     * the application rejects the request.
     */
    bool prepareToStopOnline();

  public slots:
    void requestSnapshot();
    void requestFinalSave();
    void requestReset();

  private:
    ApplicationFacade &m_application;
    IRecordingFileDialog &m_dialog;
};
