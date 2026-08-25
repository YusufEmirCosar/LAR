
#include "application/application_view_model.h"

ApplicationViewModel::ApplicationViewModel(QObject *parent) : QObject(parent) {}

void ApplicationViewModel::setState(const DecodedState &state) {
    m_decodedState = state;
    m_plane = state.plane;
    m_target = state.target;
    m_availableFields = state.availableFields;
    m_hasState = true;
    emit stateChanged();
}

void ApplicationViewModel::setState(const Plane &plane, const Target &target,
                                    const QBitArray &availableFields) {
    DecodedState state;
    state.plane = plane;
    state.target = target;
    state.availableFields = availableFields;
    setState(state);
}

void ApplicationViewModel::clearState() {
    m_plane = {};
    m_target = {};
    m_hasState = false;
    m_availableFields.clear();
    m_decodedState = {};
    emit stateChanged();
}

void ApplicationViewModel::setMode(ApplicationMode mode) {
    if (m_mode == mode)
        return;
    m_mode = mode;
    emit modeChanged(m_mode);
}

void ApplicationViewModel::setProcessedPacketCount(quint64 count) {
    if (m_processedPacketCount == count)
        return;
    m_processedPacketCount = count;
    emit metricsChanged();
}

void ApplicationViewModel::setProcessedPacketRate(quint64 rate) {
    if (m_processedPacketRate == rate)
        return;
    m_processedPacketRate = rate;
    emit metricsChanged();
}

void ApplicationViewModel::setRecordedPacketCount(quint64 count) {
    if (m_recordedPacketCount == count)
        return;
    m_recordedPacketCount = count;
    emit metricsChanged();
}

void ApplicationViewModel::setRecordingDuration(SessionTimestamp duration) {
    if (m_recordingDuration == duration)
        return;
    m_recordingDuration = duration;
    emit metricsChanged();
}

void ApplicationViewModel::setPlaybackPosition(SessionTimestamp position) {
    if (m_playbackPosition == position)
        return;
    m_playbackPosition = position;
    emit playbackStateChanged();
}

void ApplicationViewModel::setPlaybackDuration(SessionTimestamp duration) {
    if (m_playbackDuration == duration)
        return;
    m_playbackDuration = duration;
    emit playbackStateChanged();
}

void ApplicationViewModel::setPlaybackRate(double rate) {
    if (m_playbackRate == rate)
        return;
    m_playbackRate = rate;
    emit playbackStateChanged();
}

void ApplicationViewModel::setStatusText(const QString &text) {
    m_statusText = text;
    emit statusChanged(m_statusText);
}

void ApplicationViewModel::setLastError(const QString &error) {
    m_lastError = error;
    emit errorOccurred(m_lastError);
}
