#pragma once

/**
 * @file online_panel.h
 * @brief Mapping, UDP listener, access-policy, and recording controls.
 */

#include "application/ip_access_policy.h"

#include <QWidget>

class ApplicationFacade;
class OnlineWorkflowController;
class RecordingWorkflowController;
class QLabel;
class QPushButton;
class QSpinBox;

/** Owns mapping, listener, IP-policy, and recording controls. */
class OnlinePanel final : public QWidget {
  public:
    OnlinePanel(ApplicationFacade &application, OnlineWorkflowController &onlineWorkflow,
                RecordingWorkflowController &recordingWorkflow, QWidget *parent = nullptr);

  private:
    void renderOnlineState(bool listening);
    void renderPolicy(const IpAccessPolicy &activePolicy, const QString &label, bool pending);
    void renderRecording(bool recording);
    /**
     * @brief Refreshes the saved-package duration and count labels.
     */
    void renderRecordingMetrics();
    void updateRecordingControls();

    ApplicationFacade &m_application;
    OnlineWorkflowController &m_onlineWorkflow;
    RecordingWorkflowController &m_recordingWorkflow;
    QLabel *m_mappingLabel = nullptr;
    QLabel *m_mappingDetails = nullptr;
    QSpinBox *m_port = nullptr;
    QPushButton *m_listener = nullptr;
    QPushButton *m_allowAll = nullptr;
    QPushButton *m_whitelist = nullptr;
    bool m_policyPending = false;
    QLabel *m_recordDuration = nullptr;
    QLabel *m_recordCount = nullptr;
    QPushButton *m_recordPause = nullptr;
    QPushButton *m_recordSave = nullptr;
    QPushButton *m_recordReset = nullptr;
};
