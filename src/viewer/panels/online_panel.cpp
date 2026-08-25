
#include "viewer/panels/online_panel.h"

#include "application/application_facade.h"
#include "viewer/workflows/online_workflow_controller.h"
#include "viewer/workflows/recording_workflow_controller.h"

#include <QButtonGroup>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QIcon folderIcon() {
    return QIcon(QStringLiteral(":/icons/folder.png"));
}

QIcon saveIcon() {
    return QIcon(QStringLiteral(":/icons/save.png"));
}

QIcon playIcon() {
    return QIcon(QStringLiteral(":/icons/play-button-empty.png"));
}

QIcon pauseIcon() {
    return QIcon(QStringLiteral(":/icons/pause.png"));
}

QIcon trashIcon() {
    return QIcon(QStringLiteral(":/icons/trash.png"));
}
} // namespace

OnlinePanel::OnlinePanel(ApplicationFacade &application, OnlineWorkflowController &onlineWorkflow,
                         RecordingWorkflowController &recordingWorkflow, QWidget *parent)
    : QWidget(parent), m_application(application), m_onlineWorkflow(onlineWorkflow),
      m_recordingWorkflow(recordingWorkflow) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *mappingGroup = new QGroupBox(QStringLiteral("Packet Mapping"));
    auto *mappingLayout = new QVBoxLayout(mappingGroup);
    m_mappingLabel = new QLabel(QStringLiteral("No mapping loaded"));
    m_mappingLabel->setWordWrap(true);
    m_mappingLabel->setStyleSheet(QStringLiteral("color: #65706a;"));
    auto *mappingButton = new QPushButton(folderIcon(), QStringLiteral("Select JSON Mapping"));
    mappingButton->setIconSize(QSize(18, 18));
    connect(mappingButton, &QPushButton::clicked, &m_onlineWorkflow,
            &OnlineWorkflowController::requestMapping);
    mappingLayout->addWidget(m_mappingLabel);
    mappingLayout->addWidget(mappingButton);
    layout->addWidget(mappingGroup);

    auto *udpGroup = new QGroupBox(QStringLiteral("UDP Input"));
    auto *udpLayout = new QVBoxLayout(udpGroup);
    auto *form = new QFormLayout;
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(45454);
    form->addRow(QStringLiteral("Port"), m_port);
    udpLayout->addLayout(form);

    auto *policyRow = new QHBoxLayout;
    policyRow->setSpacing(6);
    m_allowAll = new QPushButton(QStringLiteral("Allow all"));
    m_allowAll->setObjectName(QStringLiteral("ipAllowAllButton"));
    m_whitelist = new QPushButton(QStringLiteral("Whitelist"));
    m_whitelist->setObjectName(QStringLiteral("ipWhitelistButton"));
    for (QPushButton *button : {m_allowAll, m_whitelist}) {
        button->setCheckable(true);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        policyRow->addWidget(button, 1);
    }
    auto *policyGroup = new QButtonGroup(this);
    policyGroup->setExclusive(true);
    policyGroup->addButton(m_allowAll);
    policyGroup->addButton(m_whitelist);
    udpLayout->addLayout(policyRow);
    connect(m_allowAll, &QPushButton::clicked, &m_onlineWorkflow,
            &OnlineWorkflowController::requestAllowAll);
    connect(m_whitelist, &QPushButton::clicked, &m_onlineWorkflow,
            &OnlineWorkflowController::requestWhitelist);

    m_listener = new QPushButton(style()->standardIcon(QStyle::SP_MediaPlay),
                                 QStringLiteral("Start Listener"));
    m_listener->setCheckable(true);
    m_listener->setEnabled(false);
    connect(m_listener, &QPushButton::toggled, this, [this](bool enabled) {
        if (enabled) {
            if (!m_application.startOnline(quint16(m_port->value()))) {
                const QSignalBlocker blocker(m_listener);
                m_listener->setChecked(false);
            }
        } else {
            const bool hadRecordingSession = m_application.hasRecordingSession();
            if (!m_recordingWorkflow.prepareToStopOnline()) {
                const QSignalBlocker blocker(m_listener);
                m_listener->setChecked(true);
            } else if (hadRecordingSession && m_application.hasRecordingSession()) {
                // Keep the button checked until the asynchronous discard reaches the facade and
                // the online source is actually stopped.
                const QSignalBlocker blocker(m_listener);
                m_listener->setChecked(true);
            }
        }
    });
    udpLayout->addWidget(m_listener);
    layout->addWidget(udpGroup);

    auto *saveGroup = new QGroupBox(QStringLiteral("Save Session"));
    auto *saveLayout = new QVBoxLayout(saveGroup);
    m_recordDuration = new QLabel(QStringLiteral("Duration: 00:00:000"));
    m_recordDuration->setObjectName(QStringLiteral("recordDurationLabel"));
    m_recordCount = new QLabel(QStringLiteral("Saved packages: 0"));
    m_recordCount->setObjectName(QStringLiteral("recordCountLabel"));

    auto *recordButtonRow = new QHBoxLayout;
    m_recordPause = new QPushButton(playIcon(), QString());
    m_recordSave = new QPushButton(saveIcon(), QString());
    m_recordReset = new QPushButton(trashIcon(), QString());
    m_recordPause->setObjectName(QStringLiteral("recordPauseButton"));
    m_recordSave->setObjectName(QStringLiteral("recordSaveButton"));
    m_recordReset->setObjectName(QStringLiteral("recordResetButton"));
    m_recordPause->setAccessibleName(QStringLiteral("Continue recording"));
    m_recordSave->setAccessibleName(QStringLiteral("Save recording"));
    m_recordReset->setAccessibleName(QStringLiteral("Reset recording"));
    for (QPushButton *button : {m_recordPause, m_recordSave, m_recordReset}) {
        button->setIconSize(QSize(18, 18));
    }
    m_recordPause->setToolTip(QStringLiteral("Continue session recording"));
    m_recordSave->setToolTip(QStringLiteral("Save session"));
    m_recordReset->setToolTip(QStringLiteral("Clear the in-memory session"));
    connect(m_recordPause, &QPushButton::clicked, this, [this] {
        if (m_application.isRecording())
            m_application.pauseRecording();
        else if (m_application.hasRecordingSession()) {
            m_application.resumeRecording();
        } else {
            m_application.startRecording();
        }
    });
    connect(m_recordSave, &QPushButton::clicked, &m_recordingWorkflow,
            &RecordingWorkflowController::requestFinalSave);
    connect(m_recordReset, &QPushButton::clicked, &m_recordingWorkflow,
            &RecordingWorkflowController::requestReset);
    recordButtonRow->addWidget(m_recordPause, 1);
    recordButtonRow->addWidget(m_recordSave, 1);
    recordButtonRow->addWidget(m_recordReset, 1);
    saveLayout->addWidget(m_recordDuration);
    saveLayout->addWidget(m_recordCount);
    saveLayout->addLayout(recordButtonRow);
    layout->addWidget(saveGroup);
    layout->addStretch();

    connect(&m_application, &ApplicationFacade::mappingLoaded, this,
            [this](const QString &, int fields, int minimumSize) {
                m_mappingLabel->setText(
                    QStringLiteral("%1 fields, packet >= %2 bytes").arg(fields).arg(minimumSize));
                m_listener->setEnabled(true);
            });
    connect(&m_application, &ApplicationFacade::onlineStateChanged, this,
            &OnlinePanel::renderOnlineState);
    connect(&m_application.viewModel(), &ApplicationViewModel::modeChanged, this,
            [this](ApplicationMode) {
                renderOnlineState(m_application.isListening());
                updateRecordingControls();
            });
    connect(&m_application.viewModel(), &ApplicationViewModel::metricsChanged, this,
            [this] { renderRecordingMetrics(); });
    connect(&m_application, &ApplicationFacade::recordingChanged, this,
            &OnlinePanel::renderRecording);
    connect(&m_application, &ApplicationFacade::recordingOperationStateChanged, this,
            [this](RecordingOperationState) { updateRecordingControls(); });
    connect(&m_onlineWorkflow, &OnlineWorkflowController::policyViewChanged, this,
            &OnlinePanel::renderPolicy);

    renderPolicy(m_onlineWorkflow.activePolicy(), m_onlineWorkflow.activePolicyLabel(), false);
    renderOnlineState(m_application.isListening());
    renderRecording(m_application.isRecording());
}

void OnlinePanel::renderOnlineState(bool listening) {
    const QSignalBlocker blocker(m_listener);
    m_listener->setChecked(listening);
    m_port->setEnabled(!listening);
    m_allowAll->setEnabled(!listening && !m_policyPending);
    m_whitelist->setEnabled(!listening && !m_policyPending);
    m_listener->setText(listening ? QStringLiteral("Stop Listener")
                                  : QStringLiteral("Start Listener"));
    m_listener->setIcon(
        style()->standardIcon(listening ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
    updateRecordingControls();
}

void OnlinePanel::renderPolicy(const IpAccessPolicy &activePolicy, const QString &label,
                               bool pending) {
    Q_UNUSED(label);
    m_policyPending = pending;
    {
        const QSignalBlocker allowAllBlocker(m_allowAll);
        const QSignalBlocker whitelistBlocker(m_whitelist);
        const bool allowAll = activePolicy.mode() == IpAccessPolicy::Mode::AllowAll;
        m_allowAll->setChecked(allowAll);
        m_whitelist->setChecked(!allowAll);
    }
    const bool listening = m_application.isListening();
    m_allowAll->setEnabled(!listening && !pending);
    m_whitelist->setEnabled(!listening && !pending);
}

void OnlinePanel::renderRecording(bool recording) {
    m_recordPause->setIcon(recording ? pauseIcon() : playIcon());
    m_recordPause->setAccessibleName(recording ? QStringLiteral("Pause recording")
                                               : QStringLiteral("Continue recording"));
    m_recordPause->setToolTip(recording ? QStringLiteral("Pause session recording")
                                        : QStringLiteral("Continue session recording"));
    renderRecordingMetrics();
    updateRecordingControls();
}

void OnlinePanel::renderRecordingMetrics() {
    const SessionTimestamp duration = m_application.viewModel().recordingDuration();
    const qint64 milliseconds = duration.milliseconds();
    const qint64 minutes = milliseconds / 60'000;
    const qint64 seconds = (milliseconds / 1'000) % 60;
    const qint64 remainder = milliseconds % 1'000;
    const QString formattedDuration = QStringLiteral("%1:%2:%3")
                                          .arg(minutes, 2, 10, QLatin1Char('0'))
                                          .arg(seconds, 2, 10, QLatin1Char('0'))
                                          .arg(remainder, 3, 10, QLatin1Char('0'));
    m_recordDuration->setText(QStringLiteral("Duration: %1").arg(formattedDuration));
    m_recordCount->setText(
        QStringLiteral("Saved packages: %1")
            .arg(QString::number(m_application.viewModel().recordedPacketCount())));
}

void OnlinePanel::updateRecordingControls() {
    const bool session = m_application.hasRecordingSession();
    const bool pending = m_application.recordingOperationPending();
    m_recordPause->setEnabled(m_application.isListening() && !pending);
    m_recordSave->setEnabled(session && !pending);
    m_recordReset->setEnabled(session && !pending);
}
