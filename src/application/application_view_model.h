#pragma once

/**
 * @file application_view_model.h
 * @brief Observable presentation state published to MainWindow.
 */

#include "application/mode_coordinator.h"
#include "application/session_timestamp.h"
#include "domain/decoded_state.h"
#include "domain/state.h"

#include <QBitArray>
#include <QObject>
#include <QString>

/**
 * @brief Main-thread projection of state, metrics, mode, and playback status.
 */
class ApplicationViewModel final : public QObject {
    Q_OBJECT

  public:
    explicit ApplicationViewModel(QObject *parent = nullptr);

    const Plane &plane() const noexcept {
        return m_plane;
    }
    const Target &target() const noexcept {
        return m_target;
    }
    const QBitArray &availableFields() const noexcept {
        return m_availableFields;
    }
    const dlz::TelemetryInputs &dlzInputs() const noexcept {
        return m_decodedState.dlzInputs;
    }
    const DecodedState &decodedState() const noexcept {
        return m_decodedState;
    }
    bool hasState() const noexcept {
        return m_hasState;
    }
    ApplicationMode mode() const noexcept {
        return m_mode;
    }

    quint64 processedPacketCount() const noexcept {
        return m_processedPacketCount;
    }
    quint64 processedPacketRate() const noexcept {
        return m_processedPacketRate;
    }
    quint64 recordedPacketCount() const noexcept {
        return m_recordedPacketCount;
    }
    /**
     * @brief Returns the time span between the first and last saved packets.
     *
     * @return The saved-packet duration.
     */
    SessionTimestamp recordingDuration() const noexcept {
        return m_recordingDuration;
    }

    SessionTimestamp playbackPosition() const noexcept {
        return m_playbackPosition;
    }
    SessionTimestamp playbackDuration() const noexcept {
        return m_playbackDuration;
    }
    double playbackRate() const noexcept {
        return m_playbackRate;
    }

    const QString &statusText() const noexcept {
        return m_statusText;
    }
    const QString &lastError() const noexcept {
        return m_lastError;
    }

    void setState(const DecodedState &state);
    void setState(const Plane &plane, const Target &target, const QBitArray &availableFields);
    void clearState();

    void setMode(ApplicationMode mode);
    void setProcessedPacketCount(quint64 count);
    /**
     * @brief Sets processed packet rate.
     *
     * @param[in] rate Finite numeric value used by the operation.
     */
    void setProcessedPacketRate(quint64 rate);
    void setRecordedPacketCount(quint64 count);
    void setRecordingDuration(SessionTimestamp duration);

    void setPlaybackPosition(SessionTimestamp position);
    void setPlaybackDuration(SessionTimestamp duration);
    /**
     * @brief Sets playback rate.
     *
     * @param[in] rate Finite numeric value used by the operation.
     */
    void setPlaybackRate(double rate);

    void setStatusText(const QString &text);
    void setLastError(const QString &error);

  signals:
    /**
     * @brief Returns the state changed.
     */
    void stateChanged();
    void modeChanged(ApplicationMode mode);
    void metricsChanged();
    void playbackStateChanged();
    void statusChanged(const QString &text);
    void errorOccurred(const QString &error);

  private:
    DecodedState m_decodedState{};
    Plane m_plane{};
    Target m_target{};
    QBitArray m_availableFields;
    bool m_hasState = false;
    ApplicationMode m_mode = ApplicationMode::Idle;

    quint64 m_processedPacketCount = 0;
    quint64 m_processedPacketRate = 0;
    quint64 m_recordedPacketCount = 0;
    SessionTimestamp m_recordingDuration;

    SessionTimestamp m_playbackPosition;
    SessionTimestamp m_playbackDuration;
    double m_playbackRate = 1.0;

    QString m_statusText;
    QString m_lastError;
};
