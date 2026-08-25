#pragma once

/**
 * @file recording_service.h
 * @brief Application-layer recording session and timestamp management.
 */

#include "application/captured_packet.h"
#include "application/ports/recording_clock.h"
#include "application/ports/recording_transaction.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <optional>

/**
 * @brief Owns recording state while delegating bytes and time to ports.
 */
class RecordingService final : public QObject {
    Q_OBJECT

  public:
    enum class State { Idle, Recording, Paused };
    Q_ENUM(State)

    RecordingService(IRecordingTransaction &transaction, IRecordingClock &clock,
                     QObject *parent = nullptr);

    bool startRecording(const QByteArray &mappingJson, QString *error = nullptr);
    void pauseRecording();
    void pauseRecordingAt(qint64 boundaryNanoseconds);
    void resumeRecording();
    bool resetRecording(QString *error = nullptr);
    bool resetRecordingAt(qint64 boundaryNanoseconds, QString *error = nullptr);
    bool createSnapshot(SessionSnapshot *snapshot, QString *error = nullptr);
    /**
     * @brief Cancels the transaction and returns to Idle.
     *
     * @details Discards the active transaction, clears timing state, and publishes the Idle
     * state.
     */
    void cancelRecording() noexcept;
    void completeRecording() noexcept;

    void recordPacket(const CapturedPacket &packet);
    void recordPacket(const QByteArray &packet);
    void recordPackets(const QVector<CapturedPacket> &packets);

    State state() const noexcept {
        return m_state;
    }
    bool isRecording() const noexcept {
        return m_state != State::Idle;
    }
    bool isPaused() const noexcept {
        return m_state == State::Paused;
    }
    quint64 recordCount() const noexcept {
        return m_transaction.recordCount();
    }
    /**
     * @brief Returns the elapsed time between the first and last saved packets.
     *
     * @details The duration is measured on the active session timeline, so time spent paused
     * is not included.
     *
     * @return The saved-packet span.
     */
    SessionTimestamp recordingDuration() const noexcept;

  signals:
    void recordingStateChanged(bool isRecording, bool isPaused);
    void recordCountChanged(quint64 count);
    void recordingError(const QString &error);

  private:
    void setState(State state);
    std::optional<SessionTimestamp> timestampFor(const CapturedPacket &packet) const noexcept;

    IRecordingTransaction &m_transaction;
    IRecordingClock &m_clock;
    State m_state = State::Idle;
    qint64 m_accumulatedNanoseconds = 0;
    qint64 m_segmentStartNanoseconds = 0;
    std::optional<SessionTimestamp> m_firstRecordedTimestamp;
    std::optional<SessionTimestamp> m_lastRecordedTimestamp;
};
