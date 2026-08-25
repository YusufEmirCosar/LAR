
#include "application/mode_coordinator.h"

namespace {

bool isAllowedTransition(ApplicationMode from, ApplicationMode to) {
    if (to == ApplicationMode::ShuttingDown)
        return true;
    switch (from) {
    case ApplicationMode::Idle:
        return to == ApplicationMode::Online || to == ApplicationMode::SessionLoaded;
    case ApplicationMode::Online:
        return to == ApplicationMode::Idle || to == ApplicationMode::Recording ||
               to == ApplicationMode::SessionLoaded;
    case ApplicationMode::Recording:
        return to == ApplicationMode::RecordingPaused || to == ApplicationMode::Online ||
               to == ApplicationMode::Idle;
    case ApplicationMode::RecordingPaused:
        return to == ApplicationMode::Recording || to == ApplicationMode::Online ||
               to == ApplicationMode::Idle;
    case ApplicationMode::SessionLoaded:
        return to == ApplicationMode::Playing || to == ApplicationMode::PlaybackPaused ||
               to == ApplicationMode::Online || to == ApplicationMode::Idle;
    case ApplicationMode::Playing:
        return to == ApplicationMode::PlaybackPaused || to == ApplicationMode::SessionLoaded ||
               to == ApplicationMode::Online || to == ApplicationMode::Idle;
    case ApplicationMode::PlaybackPaused:
        return to == ApplicationMode::Playing || to == ApplicationMode::SessionLoaded ||
               to == ApplicationMode::Online || to == ApplicationMode::Idle;
    case ApplicationMode::ShuttingDown:
        return false;
    }
    return false;
}

} // namespace

ModeCoordinator::ModeCoordinator(QObject *parent) : QObject(parent) {}

QString ModeCoordinator::modeString() const {
    switch (m_mode) {
    case ApplicationMode::Idle:
        return QStringLiteral("Idle");
    case ApplicationMode::Online:
        return QStringLiteral("Online");
    case ApplicationMode::Recording:
        return QStringLiteral("Recording");
    case ApplicationMode::RecordingPaused:
        return QStringLiteral("Recording (Paused)");
    case ApplicationMode::SessionLoaded:
        return QStringLiteral("Session Loaded");
    case ApplicationMode::Playing:
        return QStringLiteral("Playing");
    case ApplicationMode::PlaybackPaused:
        return QStringLiteral("Playback Paused");
    case ApplicationMode::ShuttingDown:
        return QStringLiteral("Shutting Down");
    }
    return QStringLiteral("Unknown");
}

bool ModeCoordinator::transitionTo(ApplicationMode newMode, QString *error) {
    if (m_mode == newMode)
        return true;
    if (m_mode == ApplicationMode::ShuttingDown) {
        if (error)
            *error = QStringLiteral("Cannot transition mode while shutting down");
        return false;
    }
    if (!isAllowedTransition(m_mode, newMode)) {
        if (error) {
            *error =
                QStringLiteral("Illegal application mode transition from %1").arg(modeString());
        }
        return false;
    }
    m_mode = newMode;
    emit modeChanged(m_mode);
    return true;
}
