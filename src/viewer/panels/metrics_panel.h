#pragma once

/**
 * @file metrics_panel.h
 * @brief Telemetry counters and frame-rate controls for the main window.
 */

#include <QFrame>

class ApplicationViewModel;
class QLabel;

/** Owns telemetry indicator widgets and their presentation formatting. */
class MetricsPanel final : public QFrame {
    Q_OBJECT

  public:
    explicit MetricsPanel(QWidget *parent = nullptr);

    void setFramesPerSecond(int fps);
    void setTotalFrameCount(quint64 count);
    void render(const ApplicationViewModel &viewModel);

  signals:
    void resetRequested();

  private:
    QLabel *m_fps = nullptr;
    QLabel *m_totalFrames = nullptr;
    QLabel *m_packetRate = nullptr;
    QLabel *m_processedPackets = nullptr;
};
