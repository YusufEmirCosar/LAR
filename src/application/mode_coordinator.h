#pragma once

/**
 * @file mode_coordinator.h
 * @brief Legal application modes and validated workflow transitions.
 */

#include <QObject>
#include <QString>

/** @brief Mutually exclusive top-level modes visible to the UI. */
enum class ApplicationMode {
    Idle,
    Online,
    Recording,
    RecordingPaused,
    SessionLoaded,
    Playing,
    PlaybackPaused,
    ShuttingDown
};

/** @brief Rejects illegal mode changes and announces accepted transitions. */
class ModeCoordinator final : public QObject {
    Q_OBJECT

  public:
    explicit ModeCoordinator(QObject *parent = nullptr);

    ApplicationMode mode() const noexcept {
        return m_mode;
    }
    QString modeString() const;

    bool transitionTo(ApplicationMode newMode, QString *error = nullptr);

  signals:
    void modeChanged(ApplicationMode newMode);

  private:
    ApplicationMode m_mode = ApplicationMode::Idle;
};
