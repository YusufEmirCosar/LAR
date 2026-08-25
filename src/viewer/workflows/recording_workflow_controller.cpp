
#include "viewer/workflows/recording_workflow_controller.h"

#include "application/application_facade.h"

RecordingWorkflowController::RecordingWorkflowController(ApplicationFacade &application,
                                                         IRecordingFileDialog &dialog,
                                                         QObject *parent)
    : QObject(parent), m_application(application), m_dialog(dialog) {}

void RecordingWorkflowController::requestSnapshot() {
    if (m_application.recordingOperationPending())
        return;
    const QString path = m_dialog.chooseSavePath(RecordingSaveKind::Snapshot);
    if (path.isEmpty())
        return;
    m_application.snapshotRecording(path);
}

void RecordingWorkflowController::requestFinalSave() {
    if (m_application.recordingOperationPending())
        return;
    const QString path = m_dialog.chooseSavePath(RecordingSaveKind::FinalSave);
    if (path.isEmpty())
        return;
    m_application.stopRecording(path);
}

void RecordingWorkflowController::requestReset() {
    if (m_application.recordingOperationPending())
        return;
    if (m_dialog.confirmReset())
        m_application.resetRecording();
}

bool RecordingWorkflowController::prepareToClose() {
    if (!m_application.hasRecordingSession())
        return true;
    if (!m_dialog.confirmDiscardOnClose())
        return false;
    m_application.discardRecording();
    return true;
}

bool RecordingWorkflowController::prepareToStopOnline() {
    if (!m_application.hasRecordingSession())
        return m_application.stopOnline();
    if (m_application.viewModel().recordedPacketCount() > 0 && !m_dialog.confirmDiscardOnStop()) {
        return false;
    }
    return m_application.stopOnlineAndDiscardRecording();
}
