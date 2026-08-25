#pragma once

/**
 * @file offline_panel.h
 * @brief Offline session replay and placeholder burst-analysis controls.
 */

#include "application/mode_coordinator.h"
#include "application/session_timestamp.h"

#include <QWidget>

class ApplicationFacade;
class ApplicationViewModel;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;

/** Owns offline-session selection, sampled replay controls, and timeline mapping. */
class OfflinePanel final : public QWidget {
    Q_OBJECT

  public:
    explicit OfflinePanel(ApplicationFacade &application, QWidget *parent = nullptr);

  signals:
    void sessionSelectionRequested();

  private:
    void renderMode(ApplicationMode mode);
    void renderPlayback(const ApplicationViewModel &viewModel);
    void sessionLoaded(const QString &path, qint64 recordCount);
    void applyPlaybackRate();

    ApplicationFacade &m_application;
    QLabel *m_fileLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QPushButton *m_playPause = nullptr;
    QPushButton *m_stop = nullptr;
    QPushButton *m_repeat = nullptr;
    QPushButton *m_burst = nullptr;
    QSlider *m_timeline = nullptr;
    QLineEdit *m_rate = nullptr;
    SessionTimestamp m_duration;
    bool m_playing = false;
};
