
#include "viewer/panels/metrics_panel.h"

#include "application/application_view_model.h"

#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

MetricsPanel::MetricsPanel(QWidget *parent) : QFrame(parent) {
    setObjectName(QStringLiteral("telemetryIndicators"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 10, 0, 0);
    layout->setSpacing(5);

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    m_fps = new QLabel(QStringLiteral("FPS: 0"));
    m_totalFrames = new QLabel(QStringLiteral("Total Frame Count: 0"));
    m_packetRate = new QLabel(QStringLiteral("Received Packages/s: 0"));
    m_processedPackets = new QLabel(QStringLiteral("Processed Packets: 0"));
    m_fps->setObjectName(QStringLiteral("fpsIndicator"));
    m_totalFrames->setObjectName(QStringLiteral("totalFramesIndicator"));
    m_packetRate->setObjectName(QStringLiteral("receivedPacketRateIndicator"));
    m_processedPackets->setObjectName(QStringLiteral("processedPacketsIndicator"));
    for (QLabel *indicator : {m_fps, m_totalFrames, m_packetRate, m_processedPackets}) {
        indicator->setFont(font);
        layout->addWidget(indicator);
    }

    auto *reset = new QPushButton(QStringLiteral("Reset"));
    reset->setObjectName(QStringLiteral("resetMetricsButton"));
    reset->setToolTip(QStringLiteral("Reset processed packets and total frames"));
    connect(reset, &QPushButton::clicked, this, &MetricsPanel::resetRequested);
    layout->addWidget(reset);
}

void MetricsPanel::setFramesPerSecond(int fps) {
    m_fps->setText(QStringLiteral("FPS: %1").arg(fps));
}

void MetricsPanel::setTotalFrameCount(quint64 count) {
    m_totalFrames->setText(QStringLiteral("Total Frame Count: %1").arg(count));
}

void MetricsPanel::render(const ApplicationViewModel &viewModel) {
    m_packetRate->setText(
        QStringLiteral("Received Packages/s: %1").arg(viewModel.processedPacketRate()));
    m_processedPackets->setText(
        QStringLiteral("Processed Packets: %1").arg(viewModel.processedPacketCount()));
}
