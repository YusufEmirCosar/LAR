
#include "viewer/workflows/online_workflow_controller.h"

#include "application/application_facade.h"
#include "viewer/dialogs/viewer_file_dialog.h"

#include <QFileInfo>

OnlineWorkflowController::OnlineWorkflowController(ApplicationFacade &application,
                                                   IViewerFileDialog &dialog, QObject *parent)
    : QObject(parent), m_application(application), m_dialog(dialog) {
    connect(&m_application, &ApplicationFacade::ipAccessPolicyChangeFinished, this,
            [this](bool applied, const IpAccessPolicy &activePolicy) {
                if (applied) {
                    m_activePolicy = activePolicy;
                    if (activePolicy.mode() == IpAccessPolicy::Mode::AllowAll) {
                        m_activeLabel = QStringLiteral("Allowing every sender");
                    } else if (!m_pendingLabel.isEmpty()) {
                        m_activeLabel = QStringLiteral("%1 address(es): %2")
                                            .arg(activePolicy.addresses().size())
                                            .arg(m_pendingLabel);
                    } else {
                        m_activeLabel =
                            QStringLiteral("%1 address(es)").arg(activePolicy.addresses().size());
                    }
                }
                m_pending = false;
                m_pendingLabel.clear();
                publish(false);
            });
}

void OnlineWorkflowController::requestMapping() {
    const QString path = m_dialog.chooseMappingPath();
    if (!path.isEmpty())
        m_application.loadMapping(path);
}

void OnlineWorkflowController::requestWhitelist() {
    if (m_application.isListening() || m_pending)
        return;
    const QString path = m_dialog.chooseWhitelistPath();
    if (path.isEmpty()) {
        publish(false);
        return;
    }

    m_pendingLabel = QFileInfo(path).fileName();
    m_pending = true;
    publish(true);
    if (!m_application.loadIpAccessPolicy(path) && m_pending) {
        m_pending = false;
        m_pendingLabel.clear();
        publish(false);
    }
}

void OnlineWorkflowController::requestAllowAll() {
    if (m_application.isListening() || m_pending)
        return;
    m_pendingLabel = QStringLiteral("Allowing every sender");
    m_pending = true;
    publish(true);
    if (!m_application.setIpAccessPolicy(IpAccessPolicy{}) && m_pending) {
        m_pending = false;
        m_pendingLabel.clear();
        publish(false);
    }
}

void OnlineWorkflowController::publish(bool pending) {
    emit policyViewChanged(m_activePolicy, pending ? m_pendingLabel : m_activeLabel, pending);
}
