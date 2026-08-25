#pragma once

/**
 * @file online_workflow_controller.h
 * @brief Presentation workflow for mapping and IP-policy file choices.
 */

#include "application/ip_access_policy.h"

#include <QObject>

class ApplicationFacade;
class IViewerFileDialog;

/** Owns mapping/IP file choices and the last successfully applied policy view. */
class OnlineWorkflowController final : public QObject {
    Q_OBJECT

  public:
    OnlineWorkflowController(ApplicationFacade &application, IViewerFileDialog &dialog,
                             QObject *parent = nullptr);

    const IpAccessPolicy &activePolicy() const noexcept {
        return m_activePolicy;
    }
    QString activePolicyLabel() const {
        return m_activeLabel;
    }

  public slots:
    void requestMapping();
    void requestWhitelist();
    void requestAllowAll();

  signals:
    void policyViewChanged(const IpAccessPolicy &activePolicy, const QString &label, bool pending);

  private:
    void publish(bool pending);

    ApplicationFacade &m_application;
    IViewerFileDialog &m_dialog;
    IpAccessPolicy m_activePolicy;
    QString m_activeLabel = QStringLiteral("Allowing every sender");
    QString m_pendingLabel;
    bool m_pending = false;
};
