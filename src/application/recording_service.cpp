
#include "application/recording_service.h"

namespace {
constexpr qint64 NanosecondsPerMillisecond = 1'000'000;
constexpr qint64 MaximumSessionNanoseconds =
    static_cast<qint64>(lar::session::MaximumDurationMilliseconds) * NanosecondsPerMillisecond;

qint64 boundedAccumulation(qint64 accumulated, qint64 delta) noexcept {
    if (accumulated > MaximumSessionNanoseconds ||
        delta > MaximumSessionNanoseconds - accumulated) {
        return MaximumSessionNanoseconds + 1;
    }
    return accumulated + delta;
}
} // namespace

RecordingService::RecordingService(IRecordingTransaction &transaction, IRecordingClock &clock,
                                   QObject *parent)
    : QObject(parent), m_transaction(transaction), m_clock(clock) {}

SessionTimestamp RecordingService::recordingDuration() const noexcept {
    if (!m_firstRecordedTimestamp || !m_lastRecordedTimestamp ||
        *m_lastRecordedTimestamp < *m_firstRecordedTimestamp) {
        return SessionTimestamp::zero();
    }
    return SessionTimestamp::clampedMilliseconds(m_lastRecordedTimestamp->milliseconds() -
                                                 m_firstRecordedTimestamp->milliseconds());
}

bool RecordingService::startRecording(const QByteArray &mappingJson, QString *error) {
    if (m_state != State::Idle || m_transaction.isActive()) {
        if (error)
            *error = QStringLiteral("A recording session is already active");
        return false;
    }
    if (mappingJson.isEmpty()) {
        if (error)
            *error = QStringLiteral("Recording requires a valid mapping");
        return false;
    }
    if (!m_transaction.begin(mappingJson, error)) {
        return false;
    }
    m_accumulatedNanoseconds = 0;
    m_firstRecordedTimestamp.reset();
    m_lastRecordedTimestamp.reset();
    m_segmentStartNanoseconds = m_clock.nowNanoseconds();
    setState(State::Recording);
    emit recordCountChanged(0);
    return true;
}

void RecordingService::pauseRecording() {
    pauseRecordingAt(m_clock.nowNanoseconds());
}

void RecordingService::pauseRecordingAt(qint64 boundaryNanoseconds) {
    if (m_state != State::Recording)
        return;
    if (boundaryNanoseconds <= 0)
        boundaryNanoseconds = m_clock.nowNanoseconds();
    if (m_segmentStartNanoseconds >= 0 && boundaryNanoseconds > m_segmentStartNanoseconds) {
        m_accumulatedNanoseconds = boundedAccumulation(
            m_accumulatedNanoseconds, boundaryNanoseconds - m_segmentStartNanoseconds);
    }
    setState(State::Paused);
}

void RecordingService::resumeRecording() {
    if (m_state != State::Paused)
        return;
    m_segmentStartNanoseconds = m_clock.nowNanoseconds();
    setState(State::Recording);
}

bool RecordingService::resetRecording(QString *error) {
    return resetRecordingAt(m_clock.nowNanoseconds(), error);
}

bool RecordingService::resetRecordingAt(qint64 boundaryNanoseconds, QString *error) {
    if (m_state == State::Idle) {
        if (error)
            *error = QStringLiteral("No recording session is active");
        return false;
    }
    if (!m_transaction.reset(error))
        return false;
    m_accumulatedNanoseconds = 0;
    m_firstRecordedTimestamp.reset();
    m_lastRecordedTimestamp.reset();
    if (m_state == State::Recording) {
        m_segmentStartNanoseconds =
            boundaryNanoseconds > 0 ? boundaryNanoseconds : m_clock.nowNanoseconds();
    }
    emit recordCountChanged(0);
    return true;
}

bool RecordingService::createSnapshot(SessionSnapshot *snapshot, QString *error) {
    if (m_state == State::Idle) {
        if (error)
            *error = QStringLiteral("No recording session is active");
        return false;
    }
    if (!snapshot) {
        if (error)
            *error = QStringLiteral("Session snapshot output is null");
        return false;
    }
    return m_transaction.createSnapshot(snapshot, error);
}

void RecordingService::cancelRecording() noexcept {
    if (m_state == State::Idle && !m_transaction.isActive())
        return;
    m_transaction.cancel();
    m_accumulatedNanoseconds = 0;
    m_firstRecordedTimestamp.reset();
    m_lastRecordedTimestamp.reset();
    setState(State::Idle);
    emit recordCountChanged(0);
}

void RecordingService::completeRecording() noexcept {
    cancelRecording();
}

void RecordingService::recordPacket(const CapturedPacket &packet) {
    recordPackets(QVector<CapturedPacket>{packet});
}

void RecordingService::recordPacket(const QByteArray &packet) {
    recordPacket(CapturedPacket{packet, m_clock.nowNanoseconds()});
}

void RecordingService::recordPackets(const QVector<CapturedPacket> &packets) {
    if (m_state != State::Recording || packets.isEmpty())
        return;
    QString error;
    for (const CapturedPacket &packet : packets) {
        const auto timestamp = timestampFor(packet);
        if (!timestamp) {
            error = QStringLiteral("Session duration exceeds the supported 365-day limit");
        }
        if (!timestamp || !m_transaction.append(*timestamp, packet.data, &error)) {
            if (m_transaction.isActive()) {
                pauseRecording();
            } else {
                m_accumulatedNanoseconds = 0;
                m_firstRecordedTimestamp.reset();
                m_lastRecordedTimestamp.reset();
                setState(State::Idle);
                emit recordCountChanged(0);
            }
            emit recordingError(error);
            return;
        }
        if (!m_firstRecordedTimestamp)
            m_firstRecordedTimestamp = *timestamp;
        m_lastRecordedTimestamp = *timestamp;
    }
    emit recordCountChanged(m_transaction.recordCount());
}

void RecordingService::setState(State state) {
    if (m_state == state)
        return;
    m_state = state;
    emit recordingStateChanged(isRecording(), isPaused());
}

std::optional<SessionTimestamp>
RecordingService::timestampFor(const CapturedPacket &packet) const noexcept {
    qint64 activeNanoseconds = 0;
    if (m_segmentStartNanoseconds >= 0 &&
        packet.receivedAtNanoseconds > m_segmentStartNanoseconds) {
        activeNanoseconds = packet.receivedAtNanoseconds - m_segmentStartNanoseconds;
    }
    if (m_accumulatedNanoseconds > MaximumSessionNanoseconds ||
        activeNanoseconds > MaximumSessionNanoseconds - m_accumulatedNanoseconds) {
        return std::nullopt;
    }
    return SessionTimestamp::fromNanoseconds(m_accumulatedNanoseconds + activeNanoseconds);
}
